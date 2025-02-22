// envelope.c
// VCA -- handle modulation and ADSR

#include "amy.h"

extern const int16_t pcm[];


SAMPLE compute_mod_value(uint16_t mod_osc) {
    // Return the modulation-rate value for the specified oscillator.
    // i.e., this oscillator is acting as modulation for something, so
    // just calculate that modulation rate (without knowing what it
    // modulates).
    // Has this mod value already been calculated this frame?  Can't
    // recalculate, because compute_mod advance phase internally.
    if (synth[mod_osc].mod_value_clock == total_samples)
        return synth[mod_osc].mod_value;
    synth[mod_osc].mod_value_clock = total_samples;
    SAMPLE value = 0;
    if(synth[mod_osc].wave == NOISE) value = compute_mod_noise(mod_osc);
    if(synth[mod_osc].wave == SAW_DOWN) value = compute_mod_saw_down(mod_osc);
    if(synth[mod_osc].wave == SAW_UP) value = compute_mod_saw_up(mod_osc);
    if(synth[mod_osc].wave == PULSE) value = compute_mod_pulse(mod_osc);
    if(synth[mod_osc].wave == TRIANGLE) value = compute_mod_triangle(mod_osc);
    if(synth[mod_osc].wave == SINE) value = compute_mod_sine(mod_osc);
    if(pcm_samples)
        if(synth[mod_osc].wave == PCM) value = compute_mod_pcm(mod_osc);
    synth[mod_osc].mod_value = value;
    return value;
}

SAMPLE compute_mod_scale(uint16_t osc) {
    uint16_t source = synth[osc].mod_source;
    if(AMY_IS_SET(source)) {
        if(source != osc) {  // that would be weird
            hold_and_modify(source);
            return compute_mod_value(source);
        }
    }
    return 0; // 0 is no change, unlike bp scale
}

SAMPLE compute_breakpoint_scale(uint16_t osc, uint8_t bp_set) {
    AMY_PROFILE_START(COMPUTE_BREAKPOINT_SCALE)
    // given a breakpoint list, compute the scale
    // we first see how many BPs are defined, and where we are in them?
    int8_t found = -1;
    int8_t release = 0;
    uint32_t t1,t0;
    SAMPLE v1, v0;
    int8_t bp_r = 0;
    t0 = 0; v0 = 0;
    // exp2(4.328085) = exp(3.0)
    #define EXP_RATE_VAL -4.328085
    const SAMPLE exponential_rate = F2S(EXP_RATE_VAL);
    // We have to aim to overshoot to the desired gap so that we hit the target by exponential_rate time.
    const SAMPLE exponential_rate_overshoot_factor = F2S(1.0f / (1.0f - exp2f(EXP_RATE_VAL)));
    uint32_t elapsed = 0;    


    // Find out which one is release (the last one)
    
    while(AMY_IS_SET(synth[osc].breakpoint_times[bp_set][bp_r]) && bp_r < MAX_BREAKPOINTS) bp_r++;
    bp_r--;
    if(bp_r<0) {
        // no breakpoints, return key gate.
        SAMPLE scale = F2S(1.0f);
        if(AMY_IS_SET(synth[osc].note_off_clock)) scale = 0;
        synth[osc].last_scale[bp_set] = scale;
        return scale; 
    }

    // Find out which BP we're in
    if(AMY_IS_SET(synth[osc].note_on_clock)) {
        elapsed = (total_samples - synth[osc].note_on_clock) + 1; 
        for(uint8_t i = 0; i < bp_r; i++) {
            if(elapsed < synth[osc].breakpoint_times[bp_set][i]) {
                // We found a segment.
                found = i;
                //i = bp_r;
                break;
            }
        }
        if(found < 0) {
            // We didn't find anything, so we are in sustain.
            found = bp_r - 1; // segment before release defines sustain
            SAMPLE scale = F2S(synth[osc].breakpoint_values[bp_set][found]);
            synth[osc].last_scale[bp_set] = scale;
            //printf("env: time %lld bpset %d seg %d SUSTAIN %f\n", total_samples, bp_set, found, S2F(scale));
            return scale;
        }
    } else if(AMY_IS_SET(synth[osc].note_off_clock)) {
        release = 1;
        elapsed = (total_samples - synth[osc].note_off_clock) + 1; 
        // Get the last t/v pair , for release
        found = bp_r;
        int8_t bp_rx = 0;
        t0 = 0; // start the elapsed clock again
        // Release starts from wherever we got to
        v0 = synth[osc].last_scale[bp_set];
        if(elapsed > synth[osc].breakpoint_times[bp_set][bp_r]) {
            // are there multiple bp_sets? only turn off the note if it's the last one
            uint32_t my_bt = synth[osc].breakpoint_times[bp_set][bp_r];
            // This is a mess but works, we could fix 
            for(uint8_t test_bp_set=0;test_bp_set<MAX_BREAKPOINT_SETS;test_bp_set++) {
                if(test_bp_set != bp_set) {
                    // Find the last bp in this bp set
                    bp_rx = 0; while(AMY_IS_SET(synth[osc].breakpoint_times[test_bp_set][bp_rx]) && bp_rx < MAX_BREAKPOINTS) bp_rx++; bp_rx--;
                    if(bp_rx >= 0) {
                        // If my breakpoint time is less than another breakpoint time from a different set, return 1.0 and don't end the note
                        if(my_bt < synth[osc].breakpoint_times[test_bp_set][bp_rx]) {
                            SAMPLE scale = F2S(1.0f);
                            synth[osc].last_scale[bp_set] = scale;
                            return scale;
                          }
                    }
                }
            }
            // OK. partials (et al) need a frame to fade out to avoid clicks. This is in conflict with the breakpoint release, 
            // which will set it to the bp end value before the fade out, often 0 so the fadeout never gets to hit. 
            // I'm not sure i love this solution, but PARTIAL is such a weird type that i guess having it called out like this is fine.
            if(synth[osc].wave==PARTIAL) {
                SAMPLE scale = F2S(1.0f);
                synth[osc].last_scale[bp_set] = scale;
                return scale;
            }
            //printf("cbp: time %f osc %d amp %f OFF\n", total_samples / (float)AMY_SAMPLE_RATE, osc, msynth[osc].amp);
            // Synth is now turned off in hold_and_modify, which tracks when the amplitude goes to zero (and waits a bit).
            //synth[osc].status=OFF;
            //AMY_UNSET(synth[osc].note_off_clock);
            SAMPLE scale = F2S(synth[osc].breakpoint_values[bp_set][bp_r]);
            synth[osc].last_scale[bp_set] = scale;
            return scale;
        }
    }

    t1 = synth[osc].breakpoint_times[bp_set][found]; 
    v1 = F2S(synth[osc].breakpoint_values[bp_set][found]);
    if(found>0 && bp_r != found && !release) {
        t0 = synth[osc].breakpoint_times[bp_set][found-1];
        v0 = F2S(synth[osc].breakpoint_values[bp_set][found-1]);
    }
    SAMPLE scale = v0;
    int sign = 1;
    if (v0 < 0 || v1 < 0) {
        sign = -1;
        v0 = -v0; v1 = -v1;
    }
    if(t1==t0 || elapsed==t1) {
        // This way we return exact zero for v1 at the end of the segment, rather than BREAKPOINT_EPS
        scale = v1;
    } else {
#define BREAKPOINT_EPS 0.0002
        // OK, we are transition from v0 to v1 , and we're at elapsed time between t0 and t1
        float time_ratio = ((float)(elapsed - t0) / (float)(t1 - t0));
        // Compute scale based on which type we have
        if(synth[osc].breakpoint_target[bp_set] & TARGET_LINEAR) {
            scale = v0 + MUL4_SS(v1 - v0, F2S(time_ratio));
        } else if(synth[osc].breakpoint_target[bp_set] & TARGET_TRUE_EXPONENTIAL) {
            v0 = MAX(v0, F2S(BREAKPOINT_EPS));
            v1 = MAX(v1, F2S(BREAKPOINT_EPS));
            SAMPLE dx7_exponential_rate = F2S(S2F(log2_lut(v1) - log2_lut(v0))
                                              / (0.001f * (t1 - t0)));
            scale = MUL4_SS(v0,
                            exp2_lut(MUL4_SS(dx7_exponential_rate,
                                             F2S(0.001f * (elapsed - t0)))));
        } else if(synth[osc].breakpoint_target[bp_set] & TARGET_DX7_EXPONENTIAL) {
            // Somewhat complicated relationship, see https://colab.research.google.com/drive/1qZmOw4r24IDijUFlel_eSoWEf3L5VSok#scrollTo=F5zkeACrOlum
            // in SAMPLE version, DX7 levels are div 8 i.e. 0 to 12.375 instead of 0 to 99.
#define LINEAR_SAMP_TO_DX7_LEVEL(samp) (MIN(12.375, S2F(log2_lut(MAX(F2S(BREAKPOINT_EPS), samp))) + 12.375))
#define DX7_LEVEL_TO_LINEAR_SAMP(level) (exp2_lut(F2S(level - 12.375)))
#define MIN_LEVEL_S 4.25
#define ATTACK_RANGE_S 9.375
#define MAP_ATTACK_LEVEL_S(level) (1 - MAX(level - MIN_LEVEL_S, 0) / ATTACK_RANGE_S)
            SAMPLE mapped_current_level = F2S(MAP_ATTACK_LEVEL_S(LINEAR_SAMP_TO_DX7_LEVEL(v0)));
            SAMPLE mapped_target_level = F2S(MAP_ATTACK_LEVEL_S(LINEAR_SAMP_TO_DX7_LEVEL(v1)));
            float t_const = (t1 - t0) / S2F(log2_lut(mapped_current_level) - log2_lut(mapped_target_level));
            float my_t0 = -t_const * S2F(log2_lut(mapped_current_level));
            if (v1 > v0) {
                // This is the magic equation that shapes the DX7 attack envelopes.
                scale = DX7_LEVEL_TO_LINEAR_SAMP(MIN_LEVEL_S + ATTACK_RANGE_S * S2F(F2S(1.0f) - exp2_lut(-F2S((my_t0 + elapsed)/t_const))));
            } else {
                // Decay is regular true_exponential
                v0 = MAX(v0, F2S(BREAKPOINT_EPS));
                v1 = MAX(v1, F2S(BREAKPOINT_EPS));
                //float dx7_exponential_rate = -logf(S2F(v1)/S2F(v0)) / (t1 - t0);
                //scale = MUL4_SS(v0, F2S(expf(-dx7_exponential_rate * (elapsed - t0))));
                SAMPLE dx7_exponential_rate = F2S(S2F(log2_lut(v1) - log2_lut(v0))
                                                  / (0.001f * (t1 - t0)));
                scale = MUL4_SS(v0,
                                exp2_lut(MUL4_SS(dx7_exponential_rate,
                                                 F2S(0.001f * (elapsed - t0)))));
            }
        } else { // "false exponential?"
            // After the full amount of time, the exponential decay will reach (1 - expf(-3)) = 0.95
            // so make the target gap a little bit bigger, to ensure we meet v1
            //scale = v0 + MUL4_SS(v1 - v0, F2S(exponential_rate_overshoot_factor * (1.0f - exp2f(-exponential_rate * time_ratio))));
            scale = v0 + MUL4_SS(v1 - v0,
                                 MUL4_SS(exponential_rate_overshoot_factor,
                                         F2S(1.0f)
                                         - exp2_lut(MUL4_SS(exponential_rate,
                                                            F2S(time_ratio)))));
            //printf("false_exponential time %lld bpset %d seg %d time_ratio %f scale %f\n", total_samples, bp_set, found, time_ratio, S2F(scale));
        }
    }
    // If sign is negative, flip it back again.
    if (sign < 0) {
        scale = -scale;
    }
    // Keep track of the most-recently returned non-release scale.
    if (!release) synth[osc].last_scale[bp_set] = scale;
    //printf("env: time %f osc %d bpset %d seg %d t0 %d t1 %d elapsed %d v0 %f v1 %f scale %f\n", total_samples / (float)AMY_SAMPLE_RATE, osc, bp_set, found, t0, t1, elapsed, S2F(v0), S2F(v1), S2F(scale));
    AMY_PROFILE_STOP(COMPUTE_BREAKPOINT_SCALE)
    return scale;
}

