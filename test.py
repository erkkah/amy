import sys
import os

import numpy as np
import scipy.io.wavfile as wav

import amy


def wavread(filename):
  """Read in audio data from a wav file.  Return d, sr."""
  # Read in wav file.
  file_handle = open(filename, 'rb')
  samplerate, wave_data = wav.read(file_handle)
  # Normalize short ints to floats in range [-1..1).
  data = np.asfarray(wave_data) / 32768.0
  return data, samplerate


def rms(samples):
  return np.sqrt(np.mean(samples ** 2))


def dB(level):
  return 20 * np.log10(level + 1e-5)



class AmyTest:

  ref_dir = './tests/ref'
  test_dir = './tests/tst'

  def __init__(self):
    amy.restart()
    amy.send(time=0)  # Defeat "computed_delta" offset.

  def test(self):

    name = self.__class__.__name__

    self.run()
    
    samples = amy.render(1.0)
    amy.write(samples, os.path.join(self.test_dir, name + '.wav'))
    rms_x = dB(rms(samples))
    message = ('%-16s:' % name) + (' signal=%.1f dB' % rms_x)

    ref_file = os.path.join(self.ref_dir, name + '.wav')
    try:
      expected_samples, _ = wavread(ref_file)

      rms_n = dB(rms(samples - expected_samples))
      message += (' err=%.1f dB' % rms_n)

    except FileNotFoundError:
      message += ' / Unable to read ' + ref_file

    print(message)

class TestSineOsc(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.SINE, freq=1000)
    amy.send(time=100, vel=1)
    amy.send(time=500, vel=0)


class TestPulseOsc(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.PULSE, freq=1000)
    amy.send(time=100, vel=1)
    amy.send(time=500, vel=0)


class TestSawDownOsc(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.SAW_DOWN)
    amy.send(time=100, note=48, vel=1)
    amy.send(time=900, vel=0)


class TestSawUpOsc(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.SAW_UP)
    amy.send(time=100, note=46, vel=1)
    amy.send(time=500, vel=0)


class TestTriangleOsc(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.TRIANGLE, freq=1000)
    amy.send(time=100, vel=1)
    amy.send(time=500, vel=0)


class TestNoiseOsc(AmyTest):

  def run(self):
    # If this is the first time noise is called, the waveform should be deterministic.
    amy.send(time=0, osc=0, wave=amy.NOISE, freq=1000)
    amy.send(time=100, vel=1)
    amy.send(time=500, vel=0)


class TestPcm(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.PCM, patch=1)
    amy.send(time=100, vel=1)


class TestPcmShift(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.PCM, patch=10)
    # Cowbell with no note should play at "default" pitch, midi 69 (for that patch)
    amy.send(time=100, vel=1)
    # Specifying a note should shift its pitch.
    amy.send(time=500, note=70, vel=1)


class TestPartial(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.PARTIALS, patch=1)
    amy.send(time=100, note=60, vel=1)


class TestSineEnv(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.SINE, freq=1000)
    # amy.send(time=0, osc=0, bp0_target=amy.TARGET_AMP, bp0='50,1,250,0.1,50,0')
    amy.send(time=0, osc=0, amp='0,0,0.85,1,0,0', bp0='50,1,250,0.1,50,0')
    amy.send(time=100, vel=1)
    amy.send(time=500, vel=0)


class TestAlgo(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.ALGO, patch=21)
    amy.send(time=100, note=70, vel=1)
    amy.send(time=500, vel=0)


class TestAlgo2(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.ALGO, patch=24)
    amy.send(time=100, note=70, vel=1)
    amy.send(time=500, vel=0)


class TestFilter(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.SAW_DOWN, filter_type=amy.FILTER_LPF, resonance=8.0, filter_freq='300,0,0,0,3', bp1='0,1,800,0.1,50,0.0')
    amy.send(time=100, note=48, vel=1.0)
    amy.send(time=900, vel=0)


class TestFilterLFO(AmyTest):

  def run(self):
    amy.send(time=0, osc=1, wave=amy.SINE, freq=6, amp=1.0)
    amy.send(time=0, osc=0, wave=amy.SAW_DOWN, filter_type=amy.FILTER_LPF, resonance=8.0, mod_source=1, filter_freq='400,0,0,0,3,0.5', bp1='0,1,500,0,100,0')
    amy.send(time=100, note=48, vel=1.0)
    amy.send(time=500, vel=0)


class TestLFO(AmyTest):

  def run(self):
    # LFO mod used to be 1+x i.e. 0.9..1.1
    #amy.send(time=0, osc=1, wave=amy.SINE, freq=4, amp=0.1)
    # With unit-per-octave scaling, that's approx log2(0.9) = -0.152, log2(1.1) = 0.138
    amy.send(time=0, osc=1, wave=amy.SINE, freq=4, amp=0.138)
    amy.send(time=0, osc=0, wave=amy.SINE, mod_source=1, mod_target=amy.TARGET_FREQ)
    amy.send(time=100, note=70, vel=1)
    amy.send(time=500, vel=0)
    

class TestDuty(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.PULSE, duty=0.1)
    amy.send(time=100, note=70, vel=1)
    amy.send(time=200, vel=0)
    amy.send(time=300, osc=0, wave=amy.PULSE, duty=0.9)
    amy.send(time=300, note=70, vel=1)
    amy.send(time=400, vel=0)
    

class TestPWM(AmyTest):

  def run(self):
    amy.send(time=0, osc=0, wave=amy.PULSE, mod_source=1, duty='0.5,0,0,0,0,0.25')
    amy.send(time=0, osc=1, wave=amy.SINE, freq=4, amp=1)
    amy.send(time=100, note=70, vel=1)
    amy.send(time=500, vel=0)
    

class TestGlobalEQ(AmyTest):

  def run(self):
    amy.send(time=0, eq_l=-10, eq_m=10, eq_h=3)
    amy.send(time=0, osc=0, wave=amy.SAW_UP)
    amy.send(time=100, note=46, vel=1)
    amy.send(time=500, vel=0)


class TestChorus(AmyTest):

  def run(self):
    # Turn on chorus.
    amy.send(chorus_level=1)
    # Note from TestFilter.
    amy.send(time=0, osc=0, wave=amy.SAW_DOWN, filter_type=amy.FILTER_LPF, resonance=8.0, filter_freq='300,0,0,0,3', bp1='0,1,800,0.1,50,0.0')
    amy.send(time=100, note=48, vel=1.0)
    amy.send(time=900, vel=0)


class TestBrass(AmyTest):
  """One of the Juno-6 patches, spelled out."""

  def run(self):
    #amy.send(time=0, osc=0, wave=amy.SAW_UP, amp='0,0,0.85,1,0,0', freq='130.81,1,0,0,0,0', filter_type=amy.FILTER_LPF,
    #         resonance=0.167, bp0='60,1,800,0.9,200,0', filter_freq='6000,0.5,0,0,1,0',
    #         bp1='60,1,800,0.9,200,0')
    #amy.send(time=0, osc=0, wave=amy.SAW_UP, amp='0,0,0.85,1,0,0', freq='130.81,1,0,0,0,0', filter_type=amy.FILTER_LPF24,
    #         resonance=0.167, bp0='60,1,400,0.3,200,0', filter_freq='2000,0.5,0,0,4,0',
    #         bp1='60,1,400,0.3,200,0')
    amy.send(time=0, osc=1, wave=amy.SAW_UP, freq='130.81,1,0,0,0,0.0',
             amp='0,0,0.85,1,0,0', bp0='30,1,702,0.354,100,0',
             filter_type=amy.FILTER_LPF24, resonance=0.167,
             filter_freq='93.73,0.677,0,0,9.133,0', bp1='30,1,702,0.354,100,0',
             mod_source=2,
             )
    amy.send(time=0, osc=2,
             wave=amy.SINE, freq=0.974, bp0='156,1.0,256,1.0,100,0')  # amp='1,0,0,0,0,0') #
    amy.send(time=100, osc=1, note=76, vel=1.0)
    amy.send(time=300, osc=1, vel=0)
    amy.send(time=600, osc=1, note=76, vel=1.0)
    amy.send(time=800, osc=1, vel=0)
    # 'filter_freq': '93.73,0.677,0,0,4.567,0', 'bp1': '30,1,702,0.354,232,0'


class TestBrass2(AmyTest):
  """Trying to catch the note-off thump."""

  def run(self):
    amy.send(time=0, osc=0, wave=amy.SAW_UP, amp='0,0,0.85,1', freq='130.815,1', 
             resonance=0.713, filter_type=amy.FILTER_LPF24, filter_freq='93.726,0.677,0,0,9.134',
             bp0='30,1,702,0.354,232,0', bp1='30,1,702,0.354,232,0')
    amy.send(time=100, osc=0, note=60, vel=1.0)
    amy.send(time=600, osc=0, vel=0)

class TestGuitar(AmyTest):
  """Trying to catch the note-off zzzzzip."""

  def run(self):
    amy.send(time=0, osc=0, wave=amy.SAW_UP, amp='0,0,0.756,1', freq='130.815,1',
             filter_freq='16.23,0.236,0,0,11.181', resonance=0.753, filter_type=amy.FILTER_LPF24,
             bp0='6,1,57,0.425,153,0',
             bp1='6,1,57,0.425,153,0')
    amy.send(time=100, osc=0, note=60, vel=4.0)
    amy.send(time=150, osc=0, vel=0)
    amy.send(time=500, osc=0, note=60, vel=4.0)
    amy.send(time=550, osc=0, vel=0)

class TestBleep(AmyTest):
  """Test the tulip start-up beep."""

  def run(self):
    amy.send(time=0, wave=amy.SINE, freq=220)
    amy.send(time=100, osc=0, pan=0.9, vel=1)
    amy.send(time=250, osc=0, pan=0.1, freq=440)
    amy.send(time=300, osc=0, pan=0.5, vel=0)


def main(argv):
  if len(argv) > 1:
    # Override location of reference files.
    AmyTest.ref_dir = argv[1]

  do_all_tests = True

  if do_all_tests:
    for testClass in AmyTest.__subclasses__():
      test_object = testClass()
      test_object.test()
  else:
    #TestPcmShift().test()
    #TestChorus().test()
    #TestBleep().test()
    #TestBrass().test()
    #TestBrass2().test()
    #TestSineEnv().test()
    #TestSawDownOsc().test()
    TestGuitar().test()
    TestFilter().test()
  amy.send(debug=0)
  print("tests done.")


if __name__ == "__main__":
  main(sys.argv)

  
