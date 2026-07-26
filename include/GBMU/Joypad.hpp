#pragma once

#include <cstdint>
#include <types.h>

namespace GBMU {

class GameBoy;

class Joypad {
private:
	GameBoy &gb;

	enum P1 { BUTTONS = 1 << 5, DPAD = 1 << 4 };

	u8 p1 = 0x00;

	bool start  = false;
	bool select = false;
	bool b      = false;
	bool a      = false;
	bool down   = false;
	bool up     = false;
	bool left   = false;
	bool right  = false;

public:
	Joypad(GameBoy &);
	virtual ~Joypad();

	u8   read_byte(u16 address);
	void write_byte(u16 address, u8 value);

	enum Input { START, SELECT, B, A, DOWN, UP, LEFT, RIGHT };

	void press(enum Input);
	void release(enum Input);
};

} // namespace GBMU
