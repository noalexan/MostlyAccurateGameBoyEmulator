#include <GBMU/APU.hpp>
#include <GBMU/GameBoy.hpp>
#include <cmath>

using namespace GBMU;

APU::APU(GameBoy &_gb) : gb(_gb)
{
	gb.getMMU().register_handler_range(
	    0xff10, 0xff26, [this](u16 addr) { return read_byte(addr); },
	    [this](u16 addr, u8 value) { write_byte(addr, value); });
	gb.getMMU().register_handler_range(
	    0xff30, 0xff3f, [this](u16 addr) { return read_byte(addr); },
	    [this](u16 addr, u8 value) { write_byte(addr, value); });
}

APU::~APU() {}

void APU::compute_audio()
{
	static float ch1_phase            = 0.0f;
	static int   ch1_length_counter   = 0;
	static int   ch1_envelope_counter = 0;
	static int   ch1_envelope_volume  = 0;
	static int   ch1_sweep_counter    = 0;
	static u16   ch1_shadow_frequency = 0;
	static bool  ch1_sweep_enabled    = false;

	static float ch2_phase            = 0.0f;
	static int   ch2_length_counter   = 0;
	static int   ch2_envelope_counter = 0;
	static int   ch2_envelope_volume  = 0;

	samples.fill(0);

	if (!(nr52 & AUDIO_ENABLE))
		return;

	const float        left_volume            = (float)(((nr50 & LEFT_VOLUME) >> 4) + 1) / 8.0f;
	const float        right_volume           = (float)((nr50 & RIGHT_VOLUME) + 1) / 8.0f;

	static const float DUTY_CYCLES[4]         = {0.125f, 0.25f, 0.5f, 0.75f};

	const int          SAMPLES_PER_FRAME_STEP = 44100 / 512;

	if (nr14 & TRIGGER) {
		ch1_phase  = 0.0f;
		nr52      |= CHANNEL_1_ON;
		nr14      &= ~TRIGGER;

		ch1_length_counter =
		    (nr14 & LENGTH_ENABLE) ? (64 - (nr11 & LENGTH_TIMER_MASK)) * (44100 / 256) : 0;

		ch1_envelope_volume  = (nr12 & INITIAL_VOLUME) >> 4;
		int envelope_period  = nr12 & ENVELOPE_PERIOD;
		ch1_envelope_counter = envelope_period * (44100 / 64);

		u16 period           = ((nr14 & PERIOD_HIGH_MASK) << 8) | nr13;
		ch1_shadow_frequency = period;
		int sweep_period     = (nr10 & SWEEP_TIME_MASK) >> 4;
		int sweep_shift      = nr10 & SWEEP_SHIFT;
		ch1_sweep_counter    = sweep_period * SAMPLES_PER_FRAME_STEP;
		ch1_sweep_enabled    = (sweep_period != 0 || sweep_shift != 0);

		if ((nr12 & 0xF8) == 0)
			nr52 &= ~CHANNEL_1_ON;
	}

	if (nr52 & CHANNEL_1_ON) {
		u16   period          = ((nr14 & PERIOD_HIGH_MASK) << 8) | nr13;
		float frequency       = 131072.0f / (float)(2048 - period);
		float duty_threshold  = DUTY_CYCLES[(nr11 >> 6) & 0x03];
		float period_samples  = 44100.0f / frequency;

		int   envelope_period = nr12 & ENVELOPE_PERIOD;
		int   sweep_period    = (nr10 & SWEEP_TIME_MASK) >> 4;
		int   sweep_shift     = nr10 & SWEEP_SHIFT;

		for (int i = 0; i < AUDIO_SAMPLES; i++) {
			if ((nr14 & LENGTH_ENABLE) && ch1_length_counter > 0 && --ch1_length_counter <= 0) {
				nr52 &= ~CHANNEL_1_ON;
				break;
			}

			if (envelope_period != 0 && ch1_envelope_counter > 0 && --ch1_envelope_counter <= 0) {
				ch1_envelope_volume  += (nr12 & ENVELOPE_DIRECTION) ? (ch1_envelope_volume < 15)
				                                                    : -(ch1_envelope_volume > 0);
				ch1_envelope_counter  = envelope_period * (44100 / 64);
			}

			if (ch1_sweep_enabled && sweep_period != 0 && ch1_sweep_counter > 0 &&
			    --ch1_sweep_counter <= 0) {
				u16 new_period;
				u16 delta  = ch1_shadow_frequency >> sweep_shift;

				new_period = ch1_shadow_frequency + ((nr10 & SWEEP_DIRECTION) ? -delta : delta);

				if (new_period > 2047) {
					nr52 &= ~CHANNEL_1_ON;
					break;
				}

				if (sweep_shift != 0) {
					ch1_shadow_frequency = new_period;
					nr13                 = new_period & 0xFF;
					nr14                 = (nr14 & 0xF8) | ((new_period >> 8) & 0x07);

					period               = new_period;
					frequency            = 131072.0f / (float)(2048 - period);
					period_samples       = 44100.0f / frequency;
				}

				ch1_sweep_counter = sweep_period * SAMPLES_PER_FRAME_STEP;
			}

			float volume = (float)ch1_envelope_volume / 15.0f;
			float wave   = (ch1_phase < duty_threshold) ? 1.0f : -1.0f;
			s16   sample = (s16)(wave * volume * 4096.0f);

			if (nr51 & CHANNEL_1_LEFT)
				samples[i * 2] += (s16)(sample * left_volume);
			if (nr51 & CHANNEL_1_RIGHT)
				samples[i * 2 + 1] += (s16)(sample * right_volume);

			ch1_phase += 1.0f / period_samples;
			if (ch1_phase >= 1.0f)
				ch1_phase -= 1.0f;
		}
	}

	if (nr24 & TRIGGER) {
		ch2_phase  = 0.0f;
		nr52      |= CHANNEL_2_ON;
		nr24      &= ~TRIGGER;

		ch2_length_counter =
		    (nr24 & LENGTH_ENABLE) ? (64 - (nr21 & LENGTH_TIMER_MASK)) * (44100 / 256) : 0;

		ch2_envelope_volume  = (nr22 & INITIAL_VOLUME) >> 4;
		ch2_envelope_counter = (nr22 & ENVELOPE_PERIOD) * (44100 / 64);

		if ((nr22 & 0xF8) == 0)
			nr52 &= ~CHANNEL_2_ON;
	}

	if (nr52 & CHANNEL_2_ON) {
		u16   period          = ((nr24 & PERIOD_HIGH_MASK) << 8) | nr23;
		float frequency       = 131072.0f / (float)(2048 - period);
		float duty_threshold  = DUTY_CYCLES[(nr21 >> 6) & 0x03];
		float period_samples  = 44100.0f / frequency;

		int   envelope_period = nr22 & ENVELOPE_PERIOD;

		for (int i = 0; i < AUDIO_SAMPLES; i++) {
			if ((nr24 & LENGTH_ENABLE) && ch2_length_counter > 0 && --ch2_length_counter <= 0) {
				nr52 &= ~CHANNEL_2_ON;
				break;
			}

			if (envelope_period != 0 && ch2_envelope_counter > 0) {
				ch2_envelope_counter--;
				if (ch2_envelope_counter <= 0) {
					ch2_envelope_volume  += (nr22 & ENVELOPE_DIRECTION) ? (ch2_envelope_volume < 15)
					                                                    : -(ch2_envelope_volume > 0);
					ch2_envelope_counter  = envelope_period * (44100 / 64);
				}
			}

			float volume = (float)ch2_envelope_volume / 15.0f;
			float wave   = (ch2_phase < duty_threshold) ? 1.0f : -1.0f;
			s16   sample = (s16)(wave * volume * 4096.0f);

			if (nr51 & CHANNEL_2_LEFT)
				samples[i * 2] += (s16)(sample * left_volume);
			if (nr51 & CHANNEL_2_RIGHT)
				samples[i * 2 + 1] += (s16)(sample * right_volume);

			ch2_phase += 1.0f / period_samples;
			if (ch2_phase >= 1.0f)
				ch2_phase -= 1.0f;
		}
	}
}

u8 APU::read_byte(u16 address)
{
	if (address >= 0xff10 && address <= 0xff26) {
		switch (address) {
		case 0xff10:
			return nr10;
		case 0xff11:
			return nr11;
		case 0xff12:
			return nr12;
		case 0xff13:
			return nr13;
		case 0xff14:
			return nr14;
		case 0xff16:
			return nr21;
		case 0xff17:
			return nr22;
		case 0xff18:
			return nr23;
		case 0xff19:
			return nr24;
		case 0xff1a:
			return nr30;
		case 0xff1b:
			return nr31;
		case 0xff1c:
			return nr32;
		case 0xff1d:
			return nr33;
		case 0xff1e:
			return nr34;
		case 0xff20:
			return nr41;
		case 0xff21:
			return nr42;
		case 0xff22:
			return nr43;
		case 0xff23:
			return nr44;
		case 0xff24:
			return nr50;
		case 0xff25:
			return nr51;
		case 0xff26:
			return nr52;
		default:
			return 0xff;
		}
	} else if (address >= 0xff30 && address <= 0xff3f) {
		return wave_pattern[address - 0xff30];
	}
	return 0xff;
}

void APU::write_byte(u16 address, u8 value)
{
	if (address >= 0xff10 && address <= 0xff26) {
		switch (address) {
		case 0xff10:
			nr10 = value;
			break;
		case 0xff11:
			nr11 = value;
			break;
		case 0xff12:
			nr12 = value;
			break;
		case 0xff13:
			nr13 = value;
			break;
		case 0xff14:
			nr14 = value;
			break;
		case 0xff16:
			nr21 = value;
			break;
		case 0xff17:
			nr22 = value;
			break;
		case 0xff18:
			nr23 = value;
			break;
		case 0xff19:
			nr24 = value;
			break;
		case 0xff1a:
			nr30 = value;
			break;
		case 0xff1b:
			nr31 = value;
			break;
		case 0xff1c:
			nr32 = value;
			break;
		case 0xff1d:
			nr33 = value;
			break;
		case 0xff1e:
			nr34 = value;
			break;
		case 0xff20:
			nr41 = value;
			break;
		case 0xff21:
			nr42 = value;
			break;
		case 0xff22:
			nr43 = value;
			break;
		case 0xff23:
			nr44 = value;
			break;
		case 0xff24:
			nr50 = value;
			break;
		case 0xff25:
			nr51 = value;
			break;
		case 0xff26:
			nr52 = value;
			break;
		}
	} else if (address >= 0xff30 && address <= 0xff3f) {
		wave_pattern[address - 0xff30] = value;
	}
}
