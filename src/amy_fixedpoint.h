// amy_fixedpoint.h - common definitions for fixed-point arithmetic.

// This is the fixed point version.  We use 3 fixed-point formats:
//  - Lookup tables are stored as int16_t, using s.15 (one sign bit, then 15 bits expressing the fractional part, for [-1.0, 1.0) range).
//  - Oscillator phases and increments are stored as int32_t using s.31 (since these implicitly cannot exceed 1.0)
//  - Samples and other arguments are stored as int32_t using s8.23 (23 fractional bits, range [-256.0, 256.0) to permit headroom.
// s.15 conforms to ANSI "_Fract" type, s.31 to "long _Fract", and s8.23 to "long _Accum"
// (see https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1169.pdf).

// Fixed point multiply is accomplished by standard integer multiply followed by a right-shift to re-center the decimal point.
// In general, (w.x) * (y.z) will yeild a (w+y).(x+z) result, which will need to be shifted right by, for instance z bits to
// regain a *.x representation.  Depending on the multiply, there may be overflow in the basic multiply result (i.e., if w+x+y+z
// is greater than the width of the multiplier output).  The overall right-shift can be applied either before or after the multiply,
// as long as the total number of bits is achieved (bit shifts to each operand and the result summed).  Applying shifts to an operand
// before the multiply discards that many bits of resolution (the bits at the bottom that are shifted out before calculation); applying
// it after multiplication limits the range of the result (since that many of the top bits will be uninformative).  In general, then,
// we want to apply sufficient shifts to the operands to avoid overflow in the multiply.

// By limiting ourselves to the 32x32 -> 32 hardware multiplier in the RP2040, we have to decide which parameters to shift.
// Without knowing anything about the input values, we should shift down the bits on the operands, so the result will not overflow
// and will be correctly aligned on output.  This means discarding lots of input bits, however, which could be harmful if we are
// multiplying a large and a small value: right-shifting the small value will lose resolution, and after multiplying by the large
// value, that lost resolution could again become significant.

// Without assumptions, we apply the right-shift to the operands before multiplication, so the result needs no further adjustment:
// mul_kk(a, b) = ((a >> 12) * (b >> 11)
// If, however, we assume the result of the product is not going to exceed some value, we can reserve some of the right-shift for
// the result, preserving more bits for the input.  For instance, if we assume an s8.23 result has its magnitude bounded by 16
// (4 bits left of the fraction point), we could do
// mul_kk(a, b) = ((a >> 10) * (b >> 9)) >> 4.
// If we further assume that a is large but b is always less than 1, we can prefer to shift it down less, e.g.
// mul_kk(a, b) = ((a >> 12) * (b >> 7)) >> 4
// If we know that a and b are smaller than 1 (15 bits each), the result must also be less than 1:
// mul_kk(a, b) = ((a >> 8) * (b >> 7)) >> 8

// In general, then, the multiply operation has three parameters:
//  - max value of result -> number of right-shift bits applied to result
//  - resolution sensitivity of first operand -> right-shift applied before mult
//  - resolution sensitivity of second operand -> right-shift applied to it before mult
// These parameters are in fact constrained since the total number of right-shifts must match the needed alignment.
// We can directly parameterize these as:
// accum mulN_kAkB(x, y) { return ( (x >> A) * (y >> B) ) >> N; }
// where N + A + B must equal the number of accum bits (23).

// How best to express these?
//  - we can define a range, e.g. -16 to 16, dictating the number of bits applied after multiplication, and thus the
//    remaining number of bits that need to be applied to the operands.
//  - we can define a "sensitive" arg, meaning we discard fewer bits for that arg.  But that makes the other arg less sensitive.

// So for a standard set of [-16, 16) Accum operations:
// #define MUL_KK mul4_k12k11(a, b)
// #define MUL_KKS mul4_k16k7(a, b)  // the second arg is "sensitive".

#define AMY_USE_FIXEDPOINT


#ifndef AMY_USE_FIXEDPOINT

#warning "floating point calc"
#define ARITH "float"


#define GENFXP float
#define LUTSAMPLE int16_t
#define PHASOR float
#define SAMPLE float

#define S2P(s) (s)
#define P2S(p) (p)
#define L2S(l) ((l) / 32768.0f)
#define S2L(s) ((int)floorf(s * 32768.0f))
#define S2F(s) (s)
#define F2S(f) (f)
#define G2F(g) (g)
#define F2G(f) (f)
#define P2F(p) (p)
#define F2P(f) ((f) - floorf(f))

#define I2S(i, b) ((i) / (float)(1 << (b)))
// Integer part of s interpreted as a proportion of a b-bit table.
#define INT_OF_S(s, b) ((int)floorf((s) * (float)(1 << (b))))
#define S_FRAC_OF_S(s, b) ((s) * (1 << (b)) - floorf((s) * (1 << (b))))

#define MUL0_SS(a, b) ((a) * (b))
#define MUL4_SS(a, b) ((a) * (b))
#define MUL8_SS(a, b) ((a) * (b))
#define MUL8F_SS(a, b) ((a) * (b))
#define MUL4E_SS(a, b) ((a) * (b))
#define MUL4_SP_S(a, b) ((a) * (b))
#define SMULR7(a, b) ((a) * (b))

#define SHIFTR(s, b) ((s) * exp2f(-(b)))
#define SHIFTL(s, b) ((s) * exp2f(b))

#define INT_OF_P(p, b) (((int)floorf((p) * (float)(1 << (b))) + (1 << (b))) % (1 <<(b)))
#define I2P(i, b) ((i) / (float)(1 << (b)))

#define S_FRAC_OF_P(p, b) ((p) * (1 << (b)) - floorf((p) * (1 << (b))))
#define P_WRAPPED_SUM(p, i) ((p) + (i) - floorf((p) + (i)))

#else

//#warning "FIXED point calc"
#define ARITH "FIXED"

typedef int16_t s_15;    // s.15 LUT sample
typedef int32_t s_31;    // s.31 phasor
typedef int32_t s8_23;  // s8.23 sample
typedef int32_t s16_15; // s16.15 general

#define GENFXP s16_15 // "65536 Hz should be enough for anyone?" -- dpwe
#define LUTSAMPLE s_15
#define PHASOR s_31
#define SAMPLE s8_23

#define L_FRAC_BITS 15
#define S_FRAC_BITS 23
#define P_FRAC_BITS 31
#define G_FRAC_BITS 15

// Convert a SAMPLE to a PHASOR
#define S2P(s) ((s) << (P_FRAC_BITS - S_FRAC_BITS))
// Convert a PHASOR to a SAMPLE
#define P2S(p) ((p) >> (P_FRAC_BITS - S_FRAC_BITS))
// Convert a LUTSAMPLE (s.15) to a SAMPLE (s8.23)
#define L2S(l) (((int32_t)(l)) << (S_FRAC_BITS - L_FRAC_BITS))
// L is also the format used in the final output.
#define S2L(s) ((s) >> (S_FRAC_BITS - L_FRAC_BITS))
// Scale an integer into a SAMPLE, where integer is a numerator and 2**B is denominator.
#define I2S(I, B) ((I) << (S_FRAC_BITS - (B)))
// Convert S and integer-only part.
// Regard SAMPLE as index into B-bit table, return integer (floor) index, strip sign bit.
#define INT_OF_S(S, B) (int32_t)(S >> (S_FRAC_BITS - B))
// Fractonal part of SAMPLE within B bits, returns a SAMPLE.
// Add 1 to B shift-up and cast to uint32_t to strip sign bit, pad a zero sign bit on way down.
#define S_FRAC_OF_S(S, B) (((S) & ((1 << (S_FRAC_BITS - (B))) - 1)) << (B))


// Convert between SAMPLE and float
#define S2F(s) ((float)(s) / (float)(1 << S_FRAC_BITS))
#define F2S(f) (SAMPLE)((f) * (float)(1 << S_FRAC_BITS))

// Convert between GENFXP and float
#define G2F(s) ((float)(s) / (float)(1 << G_FRAC_BITS))
#define F2G(f) (GENFXP)((f) * (float)(1 << G_FRAC_BITS))

// Convert between PHASOR and float
// 1 << P_FRAC_BITS is 1 << 31 which actually flips to (-2^31).
// So make the constant one less and do the last power of 2 in float.
#define P2F(s) ((float)(s) / (2.0 * (float)(1 << (P_FRAC_BITS - 1))))
#define F2P(f) ((PHASOR)((f) * 2.0 * (float)(1 << (P_FRAC_BITS - 1))))

// Fixed-point multiply routines

#define FXMUL_TEMPLATE(a, b, a_bitloss, b_bitloss, reqd_bitloss) ((((a) >> a_bitloss) * ((b) >> b_bitloss)) >> (reqd_bitloss - a_bitloss - b_bitloss))

static inline SAMPLE FXMUL_CARRY_TEMPLATE(SAMPLE a, SAMPLE b, int a_bitloss, int b_bitloss, int reqd_bitloss) {
    SAMPLE result = ((a >> (a_bitloss - 1)) * (b >> b_bitloss)) >> (reqd_bitloss - a_bitloss - b_bitloss);
    // Carry: if result is positive and result & 1 is set, add 1.
    // if result is negative and result & 1 is clear, sub 1.
    // (In 2s complement, -1 is 0xFFFF and should round to -2 (0xFFFE) when shifting right 1 bit.
    if (result & 1) {  // Carry bit set.
        if (result > 0)   result = (result + 1);
        else              result = (result - 1);
    }
    return result >> 1;  // final bit of shift down.
}

// from https://colab.research.google.com/drive/1_uQto5WSVMiSPHQ34cHbCC6qkF614EoN#scrollTo=73JbLFhg44QG
static inline SAMPLE SMULR7(SAMPLE a, SAMPLE b) {
    // Rounding on input and output; 3 zeros on output (so output is limited to -32.0 .. 32.0).
    SAMPLE r = 4 + ((a + (1 << 9)) >> 10) * ((b + (1 << 9)) >> 10);
    return r >> 3;
}

// Multiply two SAMPLE values when the result will always be [-1.0, 1.0).
#define MUL0_SS(a, b) FXMUL_TEMPLATE(a, b, 8, 7, S_FRAC_BITS)  // 8+7 = 15, so additional 8 bits shift right on output to make 23 req'd total.

// Multiply two SAMPLE values when the result will always be [-16.0, 16.0).
#define MUL4_SS(a, b)  FXMUL_TEMPLATE(a, b, 10, 9, S_FRAC_BITS)  // 10+9 = 19, so result is >>4, leaving 4 bits integer part.

// Multiply two SAMPLE values and allow result to occupy full [-256, 256) range
#define MUL8_SS(a, b)  FXMUL_TEMPLATE(a, b, 12, 11, S_FRAC_BITS)  // 12+11 = 23, so no more shift on result.

// Multiply two SAMPLE values and allow result to occupy full [-256, 256) range. Assume first arg is filter coef with |a| < 2.0.
//#define MUL8F_SS(a, b)  FXMUL_TEMPLATE(a, b, 9, 14, S_FRAC_BITS)  // 9+14 = 23, so no more shift on result.
#define MUL8F_SS(a, b)  FXMUL_CARRY_TEMPLATE(a, b, 13, 10, S_FRAC_BITS)  // 9+14 = 23, so no more shift on result.

// First argument is positive and less that 1/16, i.e. only 19 low-order bits.
//#define MUL4E_SS(a, b)  FXMUL_TEMPLATE(a, b, 5, 10, S_FRAC_BITS)  // 5+10 = 15, result is >> 8.
#define MUL4E_SS(a, b)  FXMUL_CARRY_TEMPLATE(a, b, 9, 10, S_FRAC_BITS)  // 5+10 = 15, result is >> 8.

// Multiply a SAMPLE (s8.23) by a PHASOR (s.31) yeilding a SAMPLE, so 31 bits to lose.
#define MUL4_SP_S(s, p) FXMUL_TEMPLATE(s, p, 11, 16, P_FRAC_BITS)  // need to lose 31 bits; 11+16=27, so 4 more on result

// Shifts
#define SHIFTR(s, b) ((s) >> (b))
#define SHIFTL(s, b) ((s) << (b))

// Regard PHASOR as index into B-bit table, return integer (floor) index, strip sign bit.
#define INT_OF_P(P, B) (int32_t)((((uint32_t)((P) << 1)) >> (P_FRAC_BITS + 1 - (B))))
// Scale an integer into a phasor, where integer is index into a B-bit table.
#define I2P(I, B) ((I) << (P_FRAC_BITS - (B)))

// Fractonal part of phasor within B bits, returns a SAMPLE.
// Add 1 to B shift-up and cast to uint32_t to strip sign bit, pad a zero sign bit on way down.
#define S_FRAC_OF_P(P, B) (int32_t)(((uint32_t)(((P) << ((B) + 1)))) >> (1 + P_FRAC_BITS - S_FRAC_BITS))
// Increment phasor but wrap at +1.0
#define P_WRAPPED_SUM(P, I) (int32_t)(((uint32_t)((((uint32_t)(P)) + ((uint32_t)(I))) << 1)) >> 1)

#endif // AMY_USED_FIXEDPOINT

// Fixed-point lookup table structure (copied from e.g. sine_lutset_fxpt.h).
#ifndef LUTENTRY_FXPT_DEFINED
#define LUTENTRY_FXPT_DEFINED
typedef struct {
    const int16_t *table;
    int table_size;
    int log_2_table_size;
    int highest_harmonic;
    float scale_factor;
} lut_entry_fxpt;
#endif // LUTENTRY_FXPT_DEFINED

#define LUT lut_entry_fxpt

