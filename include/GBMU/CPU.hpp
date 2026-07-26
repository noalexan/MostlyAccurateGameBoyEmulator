#pragma once

#include <cstdint>
#include <types.h>

#define TICKS_PER_CYCLES 4

namespace GBMU {

class GameBoy;

class CPU {
private:
	GameBoy &gb;
	int      ticks = 0;

	u8   interrupt_flags        = 0;
	u8   interrupt_enable       = 0;
	u8   ime                    = 0;
	bool enable_interrupt_delay = false;

	bool halted = false;

	enum Flag { ZERO = 1 << 7, NEGATIVE = 1 << 6, HALF_CARRY = 1 << 5, CARRY = 1 << 4 };

	struct Registers {
#define REGISTER_PAIR(low, high, word)                                                             \
	union {                                                                                        \
		struct {                                                                                   \
			u8 low;                                                                                \
			u8 high;                                                                               \
		};                                                                                         \
		u16 word;                                                                                  \
	}

		REGISTER_PAIR(f, a, af);
		REGISTER_PAIR(c, b, bc);
		REGISTER_PAIR(e, d, de);
		REGISTER_PAIR(l, h, hl);

#undef REGISTER_PAIR

		u16 sp;
		u16 pc;
	} registers;

	u8   read_byte(u16 address);
	void write_byte(u16 address, u8 value);

	inline u8  imm8() { return read_byte(registers.pc++); }
	inline u16 imm16() { return imm8() | (imm8() << 8); }

	inline void set_r16(u16 &r16, u16 address)
	{
		ticks += TICKS_PER_CYCLES;
		r16 = address;
	}

	inline u8 read_r8(u8 code)
	{
		switch (code & 0b111) {
		case 0b000:
			return registers.b;
		case 0b001:
			return registers.c;
		case 0b010:
			return registers.d;
		case 0b011:
			return registers.e;
		case 0b100:
			return registers.h;
		case 0b101:
			return registers.l;
		case 0b110:
			return read_byte(registers.hl);
		default:
			return registers.a;
		}
	}

	inline void write_r8(u8 code, u8 value)
	{
		switch (code & 0b111) {
		case 0b000:
			registers.b = value;
			break;
		case 0b001:
			registers.c = value;
			break;
		case 0b010:
			registers.d = value;
			break;
		case 0b011:
			registers.e = value;
			break;
		case 0b100:
			registers.h = value;
			break;
		case 0b101:
			registers.l = value;
			break;
		case 0b110:
			write_byte(registers.hl, value);
			break;
		default:
			registers.a = value;
			break;
		}
	}

	inline u16 &r16(u8 code)
	{
		switch (code & 0b11) {
		case 0b00:
			return registers.bc;
		case 0b01:
			return registers.de;
		case 0b10:
			return registers.hl;
		default:
			return registers.sp;
		}
	}

	inline u8 read_r16mem(u8 code)
	{
		switch (code & 0b11) {
		case 0b00:
			return read_byte(registers.bc);
		case 0b01:
			return read_byte(registers.de);
		case 0b10:
			return read_byte(registers.hl++);
		default:
			return read_byte(registers.hl--);
		}
	}

	inline void write_r16mem(u8 code, u8 value)
	{
		switch (code & 0b11) {
		case 0b00:
			write_byte(registers.bc, value);
			break;
		case 0b01:
			write_byte(registers.de, value);
			break;
		case 0b10:
			write_byte(registers.hl++, value);
			break;
		default:
			write_byte(registers.hl--, value);
			break;
		}
	}

	inline u16 &r16stk(u8 code)
	{
		switch (code & 0b11) {
		case 0b00:
			return registers.bc;
		case 0b01:
			return registers.de;
		case 0b10:
			return registers.hl;
		default:
			return registers.af;
		}
	}

	inline bool cond(u8 code)
	{
		switch (code & 0b11) {
		case 0b00:
			return !(registers.f & ZERO);
		case 0b01:
			return registers.f & ZERO;
		case 0b10:
			return !(registers.f & CARRY);
		default:
			return registers.f & CARRY;
		}
	}

	inline u8 b3(u8 value) { return (1 << (value & 0b111)); }

	inline void push(u8 value) { write_byte(--registers.sp, value); }

	inline u8 pop() { return read_byte(registers.sp++); }

	inline bool getZeroFlag() const { return registers.f & ZERO; }
	inline bool getNegativeFlag() const { return registers.f & NEGATIVE; }
	inline bool getHalfCarryFlag() const { return registers.f & HALF_CARRY; }
	inline bool getCarryFlag() const { return registers.f & CARRY; }

	inline void setZeroFlag(bool value)
	{
		registers.f = (registers.f & ~ZERO) | (value ? ZERO : 0);
	}

	inline void setNegativeFlag(bool value)
	{
		registers.f = (registers.f & ~NEGATIVE) | (value ? NEGATIVE : 0);
	}

	inline void setHalfCarryFlag(bool value)
	{
		registers.f = (registers.f & ~HALF_CARRY) | (value ? HALF_CARRY : 0);
	}

	inline void setCarryFlag(bool value)
	{
		registers.f = (registers.f & ~CARRY) | (value ? CARRY : 0);
	}

public:
	CPU(GameBoy &);
	virtual ~CPU();
	void tick();

	u8 &getInterruptFlags() { return interrupt_flags; }
	u8 &getInterruptEnable() { return interrupt_enable; }

	enum Interrupt {
		VBLANK = 1 << 0,
		LCD    = 1 << 1,
		TIMER  = 1 << 2,
		SERIAL = 1 << 3,
		JOYPAD = 1 << 4
	};

	void requestInterrupt(enum Interrupt interrupt) { interrupt_flags |= interrupt; }

	u8   readIO(u16 address);
	void writeIO(u16 address, u8 value);
};

} // namespace GBMU
