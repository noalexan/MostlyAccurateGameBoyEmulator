#pragma once

#include <cstdint>
#include <types.h>

namespace GBMU {

class GameBoy;

class Timer {
private:
	GameBoy &gb;

	u16 div_counter = 0;

	u8 div  = 0x00; // DIV (will be managed by div_counter)
	u8 tima = 0x00; // TIMA - Timer counter
	u8 tma  = 0x00; // TMA - Timer modulo
	u8 tac  = 0x00; // TAC - Timer control

	int timer_counter = 0;

public:
	Timer(GameBoy &);
	virtual ~Timer();

	void tick();
	u8   read_byte(u16 address);
	void write_byte(u16 address, u8 value);
};

} // namespace GBMU
