#pragma once

#include <array>
#include <types.h>

#define AUDIO_SAMPLES             735
#define AUDIO_CHANNELS            2 // Stereo
#define AUDIO_SAMPLE_RATE         44100
#define AUDIO_SAMPLES_BUFFER_SIZE (AUDIO_SAMPLES * AUDIO_CHANNELS)

namespace GBMU {

class GameBoy;

class APU {
private:
	GameBoy                                   &gb;

	std::array<s16, AUDIO_SAMPLES_BUFFER_SIZE> samples;

	enum NR10 {
		SWEEP_SHIFT     = 0x07,   // Sweep shift (0-7)
		SWEEP_DIRECTION = 1 << 3, // 0=Addition, 1=Subtraction
		SWEEP_TIME_MASK = 0x70    // Sweep time bits
	};

	enum NR11_NR21 {
		LENGTH_TIMER_MASK = 0x3F, // Length timer (0-63)
		DUTY_MASK         = 0xC0  // Duty cycle bits
	};

	enum NR12_NR22_NR42 {
		ENVELOPE_PERIOD    = 0x07,   // Envelope sweep (0-7)
		ENVELOPE_DIRECTION = 1 << 3, // 0=Decrease, 1=Increase
		INITIAL_VOLUME     = 0xF0    // Initial volume bits
	};

	enum NR14_NR24_NR34_NR44 {
		PERIOD_HIGH_MASK = 0x07,   // High 3 bits of period
		LENGTH_ENABLE    = 1 << 6, // Length enable
		TRIGGER          = 1 << 7  // Trigger
	};

	enum NR30 {
		DAC_ENABLE = 1 << 7 // DAC on/off
	};

	enum NR32 {
		OUTPUT_LEVEL_MASK = 0x60, // Output level select (00=mute, 01=100%, 10=50%, 11=25%)
		OUTPUT_LEVEL_100  = 0x20,
		OUTPUT_LEVEL_50   = 0x40,
		OUTPUT_LEVEL_25   = 0x60
	};

	enum NR43 {
		DIVISOR_CODE = 0x07,   // Frequency divisor code
		LFSR_WIDTH   = 1 << 3, // 0=15bit, 1=7bit
		CLOCK_SHIFT  = 0xF0    // Clock shift bits
	};

	enum NR50 {
		RIGHT_VOLUME = 0x07,   // Right volume (0-7)
		VIN_RIGHT    = 1 << 3, // VIN to right enable
		LEFT_VOLUME  = 0x70,   // Left volume bits
		VIN_LEFT     = 1 << 7  // VIN to left enable
	};

	enum NR51 {
		CHANNEL_1_RIGHT = 1 << 0,
		CHANNEL_2_RIGHT = 1 << 1,
		CHANNEL_3_RIGHT = 1 << 2,
		CHANNEL_4_RIGHT = 1 << 3,
		CHANNEL_1_LEFT  = 1 << 4,
		CHANNEL_2_LEFT  = 1 << 5,
		CHANNEL_3_LEFT  = 1 << 6,
		CHANNEL_4_LEFT  = 1 << 7
	};

	enum NR52 {
		CHANNEL_1_ON = 1 << 0,
		CHANNEL_2_ON = 1 << 1,
		CHANNEL_3_ON = 1 << 2,
		CHANNEL_4_ON = 1 << 3,
		AUDIO_ENABLE = 1 << 7
	};

	// Channel 1 - Tone & Sweep
	u8 nr10 = 0x80; // Sweep
	u8 nr11 = 0xBF; // Length timer & duty cycle
	u8 nr12 = 0xF3; // Volume & envelope
	u8 nr13 = 0xff; // Period low
	u8 nr14 = 0xBF; // Period high & control

	// Channel 2 - Tone
	u8 nr21 = 0x3F; // Length timer & duty cycle
	u8 nr22 = 0x00; // Volume & envelope
	u8 nr23 = 0x00; // Period low
	u8 nr24 = 0xBF; // Period high & control

	// Channel 3 - Wave
	u8 nr30 = 0x7F; // DAC enable
	u8 nr31 = 0xFF; // Length timer
	u8 nr32 = 0x9F; // Output level
	u8 nr33 = 0x00; // Period low
	u8 nr34 = 0xBF; // Period high & control

	// Channel 4 - Noise
	u8 nr41 = 0xFF; // Length timer
	u8 nr42 = 0x00; // Volume & envelope
	u8 nr43 = 0x00; // Frequency & randomness
	u8 nr44 = 0xBF; // Control

	// Control registers
	u8 nr50 = 0x77; // Master volume & VIN panning
	u8 nr51 = 0xF3; // Sound panning
	u8 nr52 = 0xF1; // Sound on/off

public:
	APU(GameBoy &);
	virtual ~APU();

	u8         read_byte(u16 address);
	void       write_byte(u16 address, u8 value);

	const s16 *get_audio_buffer() const { return samples.data(); }
	s16       *get_audio_buffer() { return samples.data(); }

	void       compute_audio();

	u8         wave_pattern[0x10];
};

} // namespace GBMU
