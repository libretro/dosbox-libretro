/* Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2016 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstring>

#include "internals.h"

#include "Analog.h"
#include "Synth.h"

using namespace std;

namespace MT32Emu {

#if MT32EMU_USE_FLOAT_SAMPLES

/* FIR approximation of the overall impulse response of the cascade composed of the sample & hold circuit and the low pass filter
 * of the MT-32 first generation.
 * The coefficients below are found by windowing the inverse DFT of the 1024 pin frequency response converted to the minimum phase.
 * The frequency response of the LPF is computed directly, the effect of the S&H is approximated by multiplying the LPF frequency
 * response by the corresponding sinc. Although, the LPF has DC gain of 3.2, we ignore this in the emulation and use normalised model.
 * The peak gain of the normalised cascade appears about 1.7 near 11.8 kHz. Relative error doesn't exceed 1% for the frequencies
 * below 12.5 kHz. In the higher frequency range, the relative error is below 8%. Peak error value is at 16 kHz.
 */
static const float COARSE_LPF_TAPS_MT32[] = {
	1.272473681f, -0.220267785f, -0.158039905f, 0.179603785f, -0.111484097f, 0.054137498f, -0.023518029f, 0.010997169f, -0.006935698f
};

// Similar approximation for new MT-32 and CM-32L/LAPC-I LPF. As the voltage controlled amplifier was introduced, LPF has unity DC gain.
// The peak gain value shifted towards higher frequencies and a bit higher about 1.83 near 13 kHz.
static const float COARSE_LPF_TAPS_CM32L[] = {
	1.340615635f, -0.403331694f, 0.036005517f, 0.066156844f, -0.069672532f, 0.049563806f, -0.031113416f, 0.019169774f, -0.012421368f
};

#else

static const unsigned int COARSE_LPF_FRACTION_BITS = 14;

// Integer versions of the FIRs above multiplied by (1 << 14) and rounded.
static const SampleEx COARSE_LPF_TAPS_MT32[] = {
	20848, -3609, -2589, 2943, -1827, 887, -385, 180, -114
};

static const SampleEx COARSE_LPF_TAPS_CM32L[] = {
	21965, -6608, 590, 1084, -1142, 812, -510, 314, -204
};

#endif

/* Combined FIR that both approximates the impulse response of the analogue circuits of sample & hold and the low pass filter
 * in the audible frequency range (below 20 kHz) and attenuates unwanted mirror spectra above 28 kHz as well. It is a polyphase
 * filter intended for resampling the signal to 48 kHz yet for applying high frequency boost.
 * As with the filter above, the analogue LPF frequency response is obtained for 1536 pin grid for range up to 96 kHz and multiplied
 * by the corresponding sinc. The result is further squared, windowed and passed to generalised Parks-McClellan routine as a desired response.
 * Finally, the minimum phase factor is found that's essentially the coefficients below.
 * Relative error in the audible frequency range doesn't exceed 0.0006%, attenuation in the stopband is better than 100 dB.
 * This level of performance makes it nearly bit-accurate for standard 16-bit sample resolution.
 */

// FIR version for MT-32 first generation.
static const float ACCURATE_LPF_TAPS_MT32[] = {
	0.003429281f, 0.025929869f, 0.096587777f, 0.228884848f, 0.372413431f, 0.412386503f, 0.263980018f,
	-0.014504962f, -0.237394528f, -0.257043496f, -0.103436603f, 0.063996095f, 0.124562333f, 0.083703206f,
	0.013921662f, -0.033475018f, -0.046239712f, -0.029310921f, 0.00126585f, 0.021060961f, 0.017925605f,
	0.003559874f, -0.005105248f, -0.005647917f, -0.004157918f, -0.002065664f, 0.00158747f, 0.003762585f,
	0.001867137f, -0.001090028f, -0.001433979f, -0.00022367f, 4.34308E-05f, -0.000247827f, 0.000157087f,
	0.000605823f, 0.000197317f, -0.000370511f, -0.000261202f, 9.96069E-05f, 9.85073E-05f, -5.28754E-05f,
	-1.00912E-05f, 7.69943E-05f, 2.03162E-05f, -5.67967E-05f, -3.30637E-05f, 1.61958E-05f, 1.73041E-05f
};

// FIR version for new MT-32 and CM-32L/LAPC-I.
static const float ACCURATE_LPF_TAPS_CM32L[] = {
	0.003917452f, 0.030693861f, 0.116424199f, 0.275101674f, 0.43217361f, 0.431247894f, 0.183255659f,
	-0.174955671f, -0.354240244f, -0.212401714f, 0.072259178f, 0.204655344f, 0.108336211f, -0.039099027f,
	-0.075138174f, -0.026261906f, 0.00582663f, 0.003052193f, 0.00613657f, 0.017017951f, 0.008732535f,
	-0.011027427f, -0.012933664f, 0.001158097f, 0.006765958f, 0.00046778f, -0.002191106f, 0.001561017f,
	0.001842871f, -0.001996876f, -0.002315836f, 0.000980965f, 0.001817454f, -0.000243272f, -0.000972848f,
	0.000149941f, 0.000498886f, -0.000204436f, -0.000347415f, 0.000142386f, 0.000249137f, -4.32946E-05f,
	-0.000131231f, 3.88575E-07f, 4.48813E-05f, -1.31906E-06f, -1.03499E-05f, 7.71971E-06f, 2.86721E-06f
};

// According to the CM-64 PCB schematic, there is a difference in the values of the LPF entrance resistors for the reverb and non-reverb channels.
// This effectively results in non-unity LPF DC gain for the reverb channel of 0.68 while the LPF has unity DC gain for the LA32 output channels.
// In emulation, the reverb output gain is multiplied by this factor to compensate for the LPF gain difference.
static const float CM32L_REVERB_TO_LA32_ANALOG_OUTPUT_GAIN_FACTOR = 0.68f;

static const unsigned int OUTPUT_GAIN_FRACTION_BITS = 8;
static const float OUTPUT_GAIN_MULTIPLIER = float(1 << OUTPUT_GAIN_FRACTION_BITS);

static const unsigned int COARSE_LPF_DELAY_LINE_LENGTH = 8; // Must be a power of 2
static const unsigned int ACCURATE_LPF_DELAY_LINE_LENGTH = 16; // Must be a power of 2
static const unsigned int ACCURATE_LPF_NUMBER_OF_PHASES = 3; // Upsampling factor
static const unsigned int ACCURATE_LPF_PHASE_INCREMENT_REGULAR = 2; // Downsampling factor
static const unsigned int ACCURATE_LPF_PHASE_INCREMENT_OVERSAMPLED = 1; // No downsampling
static const Bit32u ACCURATE_LPF_DELTAS_REGULAR[][ACCURATE_LPF_NUMBER_OF_PHASES] = { { 0, 0, 0 }, { 1, 1, 0 }, { 1, 2, 1 } };
static const Bit32u ACCURATE_LPF_DELTAS_OVERSAMPLED[][ACCURATE_LPF_NUMBER_OF_PHASES] = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 } };

class AbstractLowPassFilter {
public:
	static AbstractLowPassFilter &createLowPassFilter(AnalogOutputMode mode, bool oldMT32AnalogLPF);

	virtual ~AbstractLowPassFilter() {}
	virtual SampleEx process(SampleEx sample) = 0;
	virtual bool hasNextSample() const;
	virtual unsigned int getOutputSampleRate() const;
	virtual unsigned int estimateInSampleCount(unsigned int outSamples) const;
	virtual void addPositionIncrement(unsigned int) {}
};

class NullLowPassFilter : public AbstractLowPassFilter {
public:
	SampleEx process(SampleEx sample);
};

class CoarseLowPassFilter : public AbstractLowPassFilter {
private:
	const SampleEx * const LPF_TAPS;
	SampleEx ringBuffer[COARSE_LPF_DELAY_LINE_LENGTH];
	unsigned int ringBufferPosition;

public:
	CoarseLowPassFilter(bool oldMT32AnalogLPF);
	SampleEx process(SampleEx sample);
};

class AccurateLowPassFilter : public AbstractLowPassFilter {
private:
	const float * const LPF_TAPS;
	const Bit32u (* const deltas)[ACCURATE_LPF_NUMBER_OF_PHASES];
	const unsigned int phaseIncrement;
	const unsigned int outputSampleRate;

	SampleEx ringBuffer[ACCURATE_LPF_DELAY_LINE_LENGTH];
	unsigned int ringBufferPosition;
	unsigned int phase;

public:
	AccurateLowPassFilter(bool oldMT32AnalogLPF, bool oversample);
	SampleEx process(SampleEx sample);
	bool hasNextSample() const;
	unsigned int getOutputSampleRate() const;
	unsigned int estimateInSampleCount(unsigned int outSamples) const;
	void addPositionIncrement(unsigned int positionIncrement);
};

Analog::Analog(const AnalogOutputMode mode, const bool oldMT32AnalogLPF) :
	leftChannelLPF(AbstractLowPassFilter::createLowPassFilter(mode, oldMT32AnalogLPF)),
	rightChannelLPF(AbstractLowPassFilter::createLowPassFilter(mode, oldMT32AnalogLPF)),
	synthGain(0),
	reverbGain(0)
{}

Analog::~Analog() {
	delete &leftChannelLPF;
	delete &rightChannelLPF;
}

void Analog::process(Sample *outStream, const Sample *nonReverbLeft, const Sample *nonReverbRight, const Sample *reverbDryLeft, const Sample *reverbDryRight, const Sample *reverbWetLeft, const Sample *reverbWetRight, Bit32u outLength) {
	if (outStream == NULL) {
		leftChannelLPF.addPositionIncrement(outLength);
		rightChannelLPF.addPositionIncrement(outLength);
		return;
	}

	while (0 < (outLength--)) {
		SampleEx outSampleL;
		SampleEx outSampleR;

		if (leftChannelLPF.hasNextSample()) {
			outSampleL = leftChannelLPF.process(0);
			outSampleR = rightChannelLPF.process(0);
		} else {
			SampleEx inSampleL = ((SampleEx)*(nonReverbLeft++) + (SampleEx)*(reverbDryLeft++)) * synthGain + (SampleEx)*(reverbWetLeft++) * reverbGain;
			SampleEx inSampleR = ((SampleEx)*(nonReverbRight++) + (SampleEx)*(reverbDryRight++)) * synthGain + (SampleEx)*(reverbWetRight++) * reverbGain;

#if !MT32EMU_USE_FLOAT_SAMPLES
			inSampleL >>= OUTPUT_GAIN_FRACTION_BITS;
			inSampleR >>= OUTPUT_GAIN_FRACTION_BITS;
#endif

			outSampleL = leftChannelLPF.process(inSampleL);
			outSampleR = rightChannelLPF.process(inSampleR);
		}

		*(outStream++) = Synth::clipSampleEx(outSampleL);
		*(outStream++) = Synth::clipSampleEx(outSampleR);
	}
}

unsigned int Analog::getOutputSampleRate() const {
	return leftChannelLPF.getOutputSampleRate();
}

Bit32u Analog::getDACStreamsLength(Bit32u outputLength) const {
	return leftChannelLPF.estimateInSampleCount(outputLength);
}

void Analog::setSynthOutputGain(float useSynthGain) {
#if MT32EMU_USE_FLOAT_SAMPLES
	synthGain = useSynthGain;
#else
	if (OUTPUT_GAIN_MULTIPLIER < useSynthGain) useSynthGain = OUTPUT_GAIN_MULTIPLIER;
	synthGain = SampleEx(useSynthGain * OUTPUT_GAIN_MULTIPLIER);
#endif
}

void Analog::setReverbOutputGain(float useReverbGain, bool mt32ReverbCompatibilityMode) {
	if (!mt32ReverbCompatibilityMode) useReverbGain *= CM32L_REVERB_TO_LA32_ANALOG_OUTPUT_GAIN_FACTOR;
#if MT32EMU_USE_FLOAT_SAMPLES
	reverbGain = useReverbGain;
#else
	if (OUTPUT_GAIN_MULTIPLIER < useReverbGain) useReverbGain = OUTPUT_GAIN_MULTIPLIER;
	reverbGain = SampleEx(useReverbGain * OUTPUT_GAIN_MULTIPLIER);
#endif
}

AbstractLowPassFilter &AbstractLowPassFilter::createLowPassFilter(AnalogOutputMode mode, bool oldMT32AnalogLPF) {
	switch (mode) {
		case AnalogOutputMode_COARSE:
			return *new CoarseLowPassFilter(oldMT32AnalogLPF);
		case AnalogOutputMode_ACCURATE:
			return *new AccurateLowPassFilter(oldMT32AnalogLPF, false);
		case AnalogOutputMode_OVERSAMPLED:
			return *new AccurateLowPassFilter(oldMT32AnalogLPF, true);
		default:
			return *new NullLowPassFilter;
	}
}

bool AbstractLowPassFilter::hasNextSample() const {
	return false;
}

unsigned int AbstractLowPassFilter::getOutputSampleRate() const {
	return SAMPLE_RATE;
}

unsigned int AbstractLowPassFilter::estimateInSampleCount(unsigned int outSamples) const {
	return outSamples;
}

SampleEx NullLowPassFilter::process(const SampleEx inSample) {
	return inSample;
}

CoarseLowPassFilter::CoarseLowPassFilter(bool oldMT32AnalogLPF) :
	LPF_TAPS(oldMT32AnalogLPF ? COARSE_LPF_TAPS_MT32 : COARSE_LPF_TAPS_CM32L),
	ringBufferPosition(0)
{
	Synth::muteSampleBuffer(ringBuffer, COARSE_LPF_DELAY_LINE_LENGTH);
}

SampleEx CoarseLowPassFilter::process(const SampleEx inSample) {
	static const unsigned int DELAY_LINE_MASK = COARSE_LPF_DELAY_LINE_LENGTH - 1;

	SampleEx sample = LPF_TAPS[COARSE_LPF_DELAY_LINE_LENGTH] * ringBuffer[ringBufferPosition];
	ringBuffer[ringBufferPosition] = Synth::clipSampleEx(inSample);

	for (unsigned int i = 0; i < COARSE_LPF_DELAY_LINE_LENGTH; i++) {
		sample += LPF_TAPS[i] * ringBuffer[(i + ringBufferPosition) & DELAY_LINE_MASK];
	}

	ringBufferPosition = (ringBufferPosition - 1) & DELAY_LINE_MASK;

#if !MT32EMU_USE_FLOAT_SAMPLES
	sample >>= COARSE_LPF_FRACTION_BITS;
#endif

	return sample;
}

AccurateLowPassFilter::AccurateLowPassFilter(const bool oldMT32AnalogLPF, const bool oversample) :
	LPF_TAPS(oldMT32AnalogLPF ? ACCURATE_LPF_TAPS_MT32 : ACCURATE_LPF_TAPS_CM32L),
	deltas(oversample ? ACCURATE_LPF_DELTAS_OVERSAMPLED : ACCURATE_LPF_DELTAS_REGULAR),
	phaseIncrement(oversample ? ACCURATE_LPF_PHASE_INCREMENT_OVERSAMPLED : ACCURATE_LPF_PHASE_INCREMENT_REGULAR),
	outputSampleRate(SAMPLE_RATE * ACCURATE_LPF_NUMBER_OF_PHASES / phaseIncrement),
	ringBufferPosition(0),
	phase(0)
{
	Synth::muteSampleBuffer(ringBuffer, ACCURATE_LPF_DELAY_LINE_LENGTH);
}

SampleEx AccurateLowPassFilter::process(const SampleEx inSample) {
	static const unsigned int DELAY_LINE_MASK = ACCURATE_LPF_DELAY_LINE_LENGTH - 1;

	float sample = (phase == 0) ? LPF_TAPS[ACCURATE_LPF_DELAY_LINE_LENGTH * ACCURATE_LPF_NUMBER_OF_PHASES] * ringBuffer[ringBufferPosition] : 0.0f;
	if (!hasNextSample()) {
		ringBuffer[ringBufferPosition] = inSample;
	}

	for (unsigned int tapIx = phase, delaySampleIx = 0; delaySampleIx < ACCURATE_LPF_DELAY_LINE_LENGTH; delaySampleIx++, tapIx += ACCURATE_LPF_NUMBER_OF_PHASES) {
		sample += LPF_TAPS[tapIx] * ringBuffer[(delaySampleIx + ringBufferPosition) & DELAY_LINE_MASK];
	}

	phase += phaseIncrement;
	if (ACCURATE_LPF_NUMBER_OF_PHASES <= phase) {
		phase -= ACCURATE_LPF_NUMBER_OF_PHASES;
		ringBufferPosition = (ringBufferPosition - 1) & DELAY_LINE_MASK;
	}

	return SampleEx(ACCURATE_LPF_NUMBER_OF_PHASES * sample);
}

bool AccurateLowPassFilter::hasNextSample() const {
	return phaseIncrement <= phase;
}

unsigned int AccurateLowPassFilter::getOutputSampleRate() const {
	return outputSampleRate;
}

unsigned int AccurateLowPassFilter::estimateInSampleCount(unsigned int outSamples) const {
	Bit32u cycleCount = outSamples / ACCURATE_LPF_NUMBER_OF_PHASES;
	Bit32u remainder = outSamples - cycleCount * ACCURATE_LPF_NUMBER_OF_PHASES;
	return cycleCount * phaseIncrement + deltas[remainder][phase];
}

void AccurateLowPassFilter::addPositionIncrement(const unsigned int positionIncrement) {
	phase = (phase + positionIncrement * phaseIncrement) % ACCURATE_LPF_NUMBER_OF_PHASES;
}

} // namespace MT32Emu
