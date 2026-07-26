#include <GBMU/CPU.hpp>
#include <GBMU/GameBoy.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace GBMU;

CPU::CPU(GameBoy &_gb) : gb(_gb), registers{}
{
	registers.af = 0x01B0;
	registers.bc = 0x0013;
	registers.de = 0x00D8;
	registers.hl = 0x014D;
	registers.sp = 0xFFFE;
	registers.pc = 0x0100;

	interrupt_flags        = 0x00;
	interrupt_enable       = 0x00;
	ime                    = 0;
	enable_interrupt_delay = false;
	halted                 = false;

	gb.getMMU().register_handler(
	    0xff0f, [this](u16) { return readIO(0xff0f); },
	    [this](u16, u8 value) { writeIO(0xff0f, value); });
	gb.getMMU().register_handler(
	    0xffff, [this](u16) { return readIO(0xffff); },
	    [this](u16, u8 value) { writeIO(0xffff, value); });
}

CPU::~CPU() {}

u8 CPU::read_byte(u16 address)
{
	ticks += TICKS_PER_CYCLES;
	return gb.getMMU().read_byte(address);
}

void CPU::write_byte(u16 address, u8 value)
{
	ticks += TICKS_PER_CYCLES;
	gb.getMMU().write_byte(address, value);
}

void CPU::tick()
{
	if (ticks > 0) {
		ticks--;
		return;
	}

	u8 fired_interrupts = interrupt_flags & interrupt_enable;

	if (ime && fired_interrupts) {
		halted                 = false;
		enable_interrupt_delay = false;

		for (int i = 0; i < 5; i++) {
			if (fired_interrupts & (1 << i)) {
				interrupt_flags &= ~(1 << i);
				ime = 0;

				// Interrupt handling takes 5 machine cycles
				ticks += TICKS_PER_CYCLES * 2;
				push(registers.pc >> 8);
				push(registers.pc);
				set_r16(registers.pc, 0x40 + i * 8);

				break;
			}
		}
	}

	if (halted) {
		if (fired_interrupts) {
			halted = false;
		} else {
			return;
		}
	}

	if (enable_interrupt_delay) {
		enable_interrupt_delay = false;
		ime                    = 1;
	}

	u8 opcode = imm8();

	// Message to someone reading this:
	//   Don't hate me for this big switch statement,
	//   I'm planning to refactor this later.
	switch (opcode) {
	// nop
	case 0x00:
		break;

	// ld r16, imm16
	case 0x01:
	case 0x11:
	case 0x21:
	case 0x31:
		r16(opcode >> 4) = imm16();
		break;

	// ld [r16mem], a
	case 0x02:
	case 0x12:
	case 0x22:
	case 0x32:
		write_r16mem(opcode >> 4, registers.a);
		break;

	// ld a, [r16mem]
	case 0x0A:
	case 0x1A:
	case 0x2A:
	case 0x3A:
		registers.a = read_r16mem(opcode >> 4);
		break;

	// ld [imm16], sp
	case 0x08: {
		u16 address = imm16();
		write_byte(address, registers.sp & 0x00FF);
		write_byte(address + 1, (registers.sp & 0xFF00) >> 8);
		break;
	}

	// inc r16
	case 0x03:
	case 0x13:
	case 0x23:
	case 0x33: {
		u16 &reg = r16(opcode >> 4);
		set_r16(reg, reg + 1);
		break;
	}

	// dec r16
	case 0x0B:
	case 0x1B:
	case 0x2B:
	case 0x3B: {
		u16 &reg = r16(opcode >> 4);
		set_r16(reg, reg - 1);
		break;
	}

	// add hl, r16
	case 0x09:
	case 0x19:
	case 0x29:
	case 0x39: {
		u16 value  = r16(opcode >> 4);
		u32 result = registers.hl + value;
		setNegativeFlag(false);
		setHalfCarryFlag((registers.hl & 0x0FFF) + (value & 0x0FFF) > 0x0FFF);
		setCarryFlag(result > 0xFFFF);
		set_r16(registers.hl, static_cast<u16>(result));
		break;
	}

	// inc r8
	case 0x04:
	case 0x14:
	case 0x24:
	case 0x34:
	case 0x0C:
	case 0x1C:
	case 0x2C:
	case 0x3C: {
		u8 old    = read_r8(opcode >> 3);
		u8 result = old + 1;
		write_r8(opcode >> 3, result);
		setZeroFlag(result == 0);
		setNegativeFlag(false);
		setHalfCarryFlag((old & 0xF) == 0xF);
		break;
	}

	// dec r8
	case 0x05:
	case 0x15:
	case 0x25:
	case 0x35:
	case 0x0D:
	case 0x1D:
	case 0x2D:
	case 0x3D: {
		u8 old    = read_r8(opcode >> 3);
		u8 result = old - 1;
		write_r8(opcode >> 3, result);
		setNegativeFlag(true);
		setZeroFlag(result == 0);
		setHalfCarryFlag((old & 0xF) == 0);
		break;
	}

	// ld r8, imm8
	case 0x06:
	case 0x16:
	case 0x26:
	case 0x36:
	case 0x0E:
	case 0x1E:
	case 0x2E:
	case 0x3E:
		write_r8(opcode >> 3, imm8());
		break;

	// rlca
	case 0x07: {
		bool carry  = (registers.a & 0x80) != 0;
		registers.a = (registers.a << 1) | (carry ? 1 : 0);
		setZeroFlag(false);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(carry);
		break;
	}

	// rrca
	case 0x0F: {
		bool carry  = (registers.a & 0x01) != 0;
		registers.a = (registers.a >> 1) | (carry ? 0x80 : 0);
		setZeroFlag(false);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(carry);
		break;
	}

	// rla
	case 0x17: {
		bool carry  = (registers.a & 0x80) != 0;
		registers.a = (registers.a << 1) | (registers.f & CARRY ? 1 : 0);
		setZeroFlag(false);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(carry);
		break;
	}

	// rra
	case 0x1F: {
		bool carry  = (registers.a & 0x01) != 0;
		registers.a = (registers.a >> 1) | (registers.f & CARRY ? 0x80 : 0);
		setZeroFlag(false);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(carry);
		break;
	}

	// daa
	case 0x27: {
		u8 correction = 0;
		if (getHalfCarryFlag() || (!getNegativeFlag() && (registers.a & 0x0F) > 0x09))
			correction |= 0x06;
		if (getCarryFlag() || (!getNegativeFlag() && registers.a > 0x99)) {
			correction |= 0x60;
			setCarryFlag(true);
		}
		if (getNegativeFlag())
			registers.a -= correction;
		else
			registers.a += correction;
		setZeroFlag(registers.a == 0);
		setHalfCarryFlag(false);
		break;
	}

	// cpl
	case 0x2F:
		registers.a = ~registers.a;
		setNegativeFlag(true);
		setHalfCarryFlag(true);
		break;

	// scf
	case 0x37:
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(true);
		break;

	// ccf
	case 0x3F:
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(!getCarryFlag());
		break;

	// jr imm8
	case 0x18: {
		s8 offset = static_cast<s8>(imm8());
		set_r16(registers.pc, registers.pc + offset);
		break;
	}

	// jr cond, imm8
	case 0x20:
	case 0x30:
	case 0x28:
	case 0x38: {
		s8 offset = static_cast<s8>(imm8());
		if (cond(opcode >> 3))
			set_r16(registers.pc, registers.pc + offset);
		break;
	}

	// stop
	case 0x10:
		std::cerr << "STOP instr called" << std::endl;
		break;

	// ld r8, r8
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4A:
	case 0x4B:
	case 0x4C:
	case 0x4D:
	case 0x4E:
	case 0x4F:
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
	case 0x58:
	case 0x59:
	case 0x5A:
	case 0x5B:
	case 0x5C:
	case 0x5D:
	case 0x5E:
	case 0x5F:
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
	case 0x69:
	case 0x6A:
	case 0x6B:
	case 0x6C:
	case 0x6D:
	case 0x6E:
	case 0x6F:
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7A:
	case 0x7B:
	case 0x7C:
	case 0x7D:
	case 0x7E:
	case 0x7F:
		write_r8(opcode >> 3, read_r8(opcode));
		break;

	// halt
	case 0x76:
		halted = true;
		break;

	// add a, r8
	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87: {
		u8 value = read_r8(opcode);
		setHalfCarryFlag((registers.a & 0xF) + (value & 0xF) > 0xF);
		setCarryFlag(registers.a + value > 0xFF);
		registers.a += value;
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		break;
	}

	// adc a, r8
	case 0x88:
	case 0x89:
	case 0x8A:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8E:
	case 0x8F: {
		u8 value = read_r8(opcode);
		u8 carry = (registers.f & CARRY) ? 1 : 0;
		setHalfCarryFlag((registers.a & 0xF) + (value & 0xF) + carry > 0xF);
		setCarryFlag(registers.a + value + carry > 0xFF);
		registers.a += value + carry;
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		break;
	}

	// sub a, r8
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97: {
		u8 value = read_r8(opcode);
		setNegativeFlag(true);
		setHalfCarryFlag((registers.a & 0xF) < (value & 0xF));
		setCarryFlag(registers.a < value);
		registers.a -= value;
		setZeroFlag(registers.a == 0);
		break;
	}

	// sbc a, r8
	case 0x98:
	case 0x99:
	case 0x9A:
	case 0x9B:
	case 0x9C:
	case 0x9D:
	case 0x9E:
	case 0x9F: {
		u8 value = read_r8(opcode);
		u8 carry = (registers.f & CARRY) ? 1 : 0;
		setNegativeFlag(true);
		setHalfCarryFlag((registers.a & 0xF) < (value & 0xF) + carry);
		setCarryFlag(registers.a < value + carry);
		registers.a -= value + carry;
		setZeroFlag(registers.a == 0);
		break;
	}

	// and a, r8
	case 0xA0:
	case 0xA1:
	case 0xA2:
	case 0xA3:
	case 0xA4:
	case 0xA5:
	case 0xA6:
	case 0xA7:
		registers.a &= read_r8(opcode);
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		setHalfCarryFlag(true);
		setCarryFlag(false);
		break;

	// xor a, r8
	case 0xa8:
	case 0xa9:
	case 0xaa:
	case 0xab:
	case 0xac:
	case 0xad:
	case 0xae:
	case 0xaf:
		registers.a ^= read_r8(opcode);
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(false);
		break;

	// cp a, r8
	case 0xb8:
	case 0xb9:
	case 0xba:
	case 0xbb:
	case 0xbc:
	case 0xbd:
	case 0xbe:
	case 0xbf: {
		u8 value = read_r8(opcode);
		setNegativeFlag(true);
		setZeroFlag(registers.a - value == 0);
		setHalfCarryFlag((registers.a & 0xF) < (value & 0xF));
		setCarryFlag(registers.a < value);
		break;
	}

	// or a, r8
	case 0xb0:
	case 0xb1:
	case 0xb2:
	case 0xb3:
	case 0xb4:
	case 0xb5:
	case 0xb6:
	case 0xb7:
		registers.a |= read_r8(opcode);
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(false);
		break;

	// add a, imm8
	case 0xc6: {
		u8 value = imm8();
		setHalfCarryFlag((registers.a & 0xF) + (value & 0xF) > 0xF);
		setCarryFlag(registers.a + value > 0xFF);
		registers.a += value;
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		break;
	}

	// adc a, imm8
	case 0xce: {
		u8 value = imm8();
		u8 carry = (registers.f & CARRY) ? 1 : 0;
		setHalfCarryFlag((registers.a & 0xF) + (value & 0xF) + carry > 0xF);
		setCarryFlag(registers.a + value + carry > 0xFF);
		registers.a += value + carry;
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		break;
	}

	// sub a, imm8
	case 0xd6: {
		u8 value = imm8();
		setNegativeFlag(true);
		setHalfCarryFlag((registers.a & 0xF) < (value & 0xF));
		setCarryFlag(registers.a < value);
		registers.a -= value;
		setZeroFlag(registers.a == 0);
		break;
	}

	// sbc a, imm8
	case 0xde: {
		u8 value = imm8();
		u8 carry = (registers.f & CARRY) ? 1 : 0;
		setNegativeFlag(true);
		setHalfCarryFlag((registers.a & 0xF) < (value & 0xF) + carry);
		setCarryFlag(registers.a < value + carry);
		registers.a -= value + carry;
		setZeroFlag(registers.a == 0);
		break;
	}

	// and a, imm8
	case 0xe6:
		registers.a &= imm8();
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		setHalfCarryFlag(true);
		setCarryFlag(false);
		break;

	// xor a, imm8
	case 0xee:
		registers.a ^= imm8();
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(false);
		break;

	// or a, imm8
	case 0xf6:
		registers.a |= imm8();
		setZeroFlag(registers.a == 0);
		setNegativeFlag(false);
		setHalfCarryFlag(false);
		setCarryFlag(false);
		break;

	// cp a, imm8
	case 0xfe: {
		u8 value = imm8();
		setNegativeFlag(true);
		setZeroFlag(registers.a - value == 0);
		setHalfCarryFlag((registers.a & 0xF) < (value & 0xF));
		setCarryFlag(registers.a < value);
		break;
	}

	// ret cond
	case 0xC0:
	case 0xD0:
	case 0xC8:
	case 0xD8: {
		ticks += 4; // I don't know why but it takes an extra 4 cycles
		            // https://gist.github.com/SonoSooS/c0055300670d678b5ae8433e20bea595#ret-cc
		if (cond(opcode >> 3)) {
			u8 low  = pop();
			u8 high = pop();
			set_r16(registers.pc, low | (high << 8));
		}
		break;
	}

	// ret
	case 0xC9: {
		u8 low  = pop();
		u8 high = pop();
		set_r16(registers.pc, low | (high << 8));
		break;
	}

	// reti
	case 0xD9: {
		u8 low  = pop();
		u8 high = pop();
		set_r16(registers.pc, low | (high << 8));
		ime = 1;
		break;
	}

	// jp cond, imm16
	case 0xc2:
	case 0xd2:
	case 0xca:
	case 0xda: {
		u16 address = imm16();
		if (cond(opcode >> 3)) {
			set_r16(registers.pc, address);
		}
		break;
	}

	// jp imm16
	case 0xc3: {
		u16 address = imm16();
		set_r16(registers.pc, address);
		break;
	}

	// jp hl
	case 0xe9:
		registers.pc = registers.hl;
		break;

	// call cond, imm16
	case 0xc4:
	case 0xd4:
	case 0xcc:
	case 0xdc: {
		u16 address = imm16();
		if (cond(opcode >> 3)) {
			push(registers.pc >> 8);
			push(registers.pc);
			set_r16(registers.pc, address);
		}
		break;
	}

	// call imm16
	case 0xcd: {
		u16 address = imm16();
		push(registers.pc >> 8);
		push(registers.pc);
		set_r16(registers.pc, address);
		break;
	}

	// rst tgt3
	case 0xC7:
	case 0xD7:
	case 0xE7:
	case 0xF7:
	case 0xCF:
	case 0xDF:
	case 0xEF:
	case 0xFF:
		push(registers.pc >> 8);
		push(registers.pc);
		set_r16(registers.pc, opcode & 0b00111000);
		break;

	// pop r16stk
	case 0xC1:
	case 0xD1:
	case 0xE1:
	case 0xF1: {
		u8   low  = pop();
		u8   high = pop();
		u16 &reg  = r16stk(opcode >> 4);
		reg       = low | (high << 8);
		if (opcode == 0xF1) // pop af: lower 4 bits of F are always 0
			registers.f &= 0xF0;
		break;
	}

	// push r16stk
	case 0xC5:
	case 0xD5:
	case 0xE5:
	case 0xF5: {
		u16 value = r16stk(opcode >> 4);
		push(value >> 8);
		push(value);
		break;
	}

	case 0xcb: {
		opcode = imm8();

		switch (opcode) {
		// rlc r8
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07: {
			u8   reg    = read_r8(opcode);
			bool carry  = (reg & 0x80) != 0;
			u8   result = (reg << 1) | (carry ? 1 : 0);
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(carry);
			break;
		}

		// rrc r8
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D:
		case 0x0E:
		case 0x0F: {
			u8   reg    = read_r8(opcode);
			bool carry  = (reg & 0x01) != 0;
			u8   result = (reg >> 1) | (carry ? 0x80 : 0);
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(carry);
			break;
		}

		// rl r8
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17: {
			u8   reg    = read_r8(opcode);
			bool carry  = (reg & 0x80) != 0;
			u8   result = (reg << 1) | (registers.f & CARRY ? 1 : 0);
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(carry);
			break;
		}

		// rr r8
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1E:
		case 0x1F: {
			u8   reg    = read_r8(opcode);
			bool carry  = reg & 1;
			u8   result = (reg >> 1) | (registers.f & CARRY ? 0x80 : 0);
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(carry);
			break;
		}

		// sla r8
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27: {
			u8   reg    = read_r8(opcode);
			bool carry  = (reg & 0x80) != 0;
			u8   result = reg << 1;
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(carry);
			break;
		}

		// sra r8
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2D:
		case 0x2E:
		case 0x2F: {
			u8   reg    = read_r8(opcode);
			bool carry  = (reg & 0x01) != 0;
			u8   result = (reg >> 1) | (reg & 0x80);
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(carry);
			break;
		}

		// swap r8
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37: {
			u8 reg    = read_r8(opcode);
			u8 result = ((reg & 0x0F) << 4) | ((reg & 0xF0) >> 4);
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(false);
			break;
		}

		// srl r8
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
		case 0x3C:
		case 0x3D:
		case 0x3E:
		case 0x3F: {
			u8   reg    = read_r8(opcode);
			bool carry  = (reg & 0x01) != 0;
			u8   result = reg >> 1;
			write_r8(opcode, result);
			setZeroFlag(result == 0);
			setNegativeFlag(false);
			setHalfCarryFlag(false);
			setCarryFlag(carry);
			break;
		}

		// bit b3, r8
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4A:
		case 0x4B:
		case 0x4C:
		case 0x4D:
		case 0x4E:
		case 0x4F:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C:
		case 0x6D:
		case 0x6E:
		case 0x6F:
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x7C:
		case 0x7D:
		case 0x7E:
		case 0x7F:
			setHalfCarryFlag(true);
			setNegativeFlag(false);
			setZeroFlag(!(read_r8(opcode) & b3(opcode >> 3)));
			break;

		// res b3, r8
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
		case 0x8C:
		case 0x8D:
		case 0x8E:
		case 0x8F:
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
		case 0x98:
		case 0x99:
		case 0x9A:
		case 0x9B:
		case 0x9C:
		case 0x9D:
		case 0x9E:
		case 0x9F:
		case 0xA0:
		case 0xA1:
		case 0xA2:
		case 0xA3:
		case 0xA4:
		case 0xA5:
		case 0xA6:
		case 0xA7:
		case 0xA8:
		case 0xA9:
		case 0xAA:
		case 0xAB:
		case 0xAC:
		case 0xAD:
		case 0xAE:
		case 0xAF:
		case 0xB0:
		case 0xB1:
		case 0xB2:
		case 0xB3:
		case 0xB4:
		case 0xB5:
		case 0xB6:
		case 0xB7:
		case 0xB8:
		case 0xB9:
		case 0xBA:
		case 0xBB:
		case 0xBC:
		case 0xBD:
		case 0xBE:
		case 0xBF: {
			u8 value = read_r8(opcode);
			write_r8(opcode, value & ~b3(opcode >> 3));
			break;
		}

		// set b3, r8
		case 0xC0:
		case 0xC1:
		case 0xC2:
		case 0xC3:
		case 0xC4:
		case 0xC5:
		case 0xC6:
		case 0xC7:
		case 0xC8:
		case 0xC9:
		case 0xCA:
		case 0xCB:
		case 0xCC:
		case 0xCD:
		case 0xCE:
		case 0xCF:
		case 0xD0:
		case 0xD1:
		case 0xD2:
		case 0xD3:
		case 0xD4:
		case 0xD5:
		case 0xD6:
		case 0xD7:
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDD:
		case 0xDE:
		case 0xDF:
		case 0xE0:
		case 0xE1:
		case 0xE2:
		case 0xE3:
		case 0xE4:
		case 0xE5:
		case 0xE6:
		case 0xE7:
		case 0xE8:
		case 0xE9:
		case 0xEA:
		case 0xEB:
		case 0xEC:
		case 0xED:
		case 0xEE:
		case 0xEF:
		case 0xF0:
		case 0xF1:
		case 0xF2:
		case 0xF3:
		case 0xF4:
		case 0xF5:
		case 0xF6:
		case 0xF7:
		case 0xF8:
		case 0xF9:
		case 0xFA:
		case 0xFB:
		case 0xFC:
		case 0xFD:
		case 0xFE:
		case 0xFF: {
			u8 value = read_r8(opcode);
			write_r8(opcode, value | b3(opcode >> 3));
			break;
		}

		default: {
			std::stringstream ss;
			ss << "Unknown CB instruction: 0x" << std::hex << std::setw(2) << std::setfill('0')
			   << (int)opcode;
			throw std::runtime_error(ss.str());
		}
		}
		break;
	}

	// ldh [c], a
	case 0xE2:
		write_byte(0xFF00 + registers.c, registers.a);
		break;

	// ldh [imm8], a
	case 0xE0:
		write_byte(0xFF00 + imm8(), registers.a);
		break;

	// ld [imm16], a
	case 0xEA:
		write_byte(imm16(), registers.a);
		break;

	// ldh a, [c]
	case 0xF2:
		registers.a = read_byte(0xFF00 + registers.c);
		break;

	// ldh a, [imm8]
	case 0xF0:
		registers.a = read_byte(0xFF00 + imm8());
		break;

	// ld a, [imm16]
	case 0xFA:
		registers.a = read_byte(imm16());
		break;

	// add sp, imm8
	case 0xE8: {
		ticks += 4;
		s8  offset = static_cast<s8>(imm8());
		u16 result = registers.sp + offset;
		setZeroFlag(false);
		setNegativeFlag(false);
		setHalfCarryFlag((registers.sp & 0x0F) + (offset & 0x0F) > 0x0F);
		setCarryFlag((registers.sp & 0xFF) + (offset & 0xFF) > 0xFF);
		set_r16(registers.sp, static_cast<u16>(result));
		break;
	}

	// ld hl, sp + imm8
	case 0xF8: {
		s8  offset = static_cast<s8>(imm8());
		u16 result = registers.sp + offset;
		setZeroFlag(false);
		setNegativeFlag(false);
		setHalfCarryFlag((registers.sp & 0x0F) + (offset & 0x0F) > 0x0F);
		setCarryFlag((registers.sp & 0xFF) + (offset & 0xFF) > 0xFF);
		set_r16(registers.hl, static_cast<u16>(result));
		break;
	}

	// ld sp, hl
	case 0xF9:
		set_r16(registers.sp, registers.hl);
		break;

	// di
	case 0xF3:
		ime = 0;
		break;

	// ei
	case 0xFB:
		enable_interrupt_delay = true;
		break;

	// No instructions
	case 0xD3:
	case 0xDB:
	case 0xDD:
	case 0xE3:
	case 0xE4:
	case 0xEB:
	case 0xEC:
	case 0xED:
	case 0xF4:
	case 0xFC:
	case 0xFD:
		break;

	default: {
		std::stringstream ss;
		ss << "Unknown instruction: 0x" << std::hex << std::setw(2) << std::setfill('0')
		   << (int)opcode;
		throw std::runtime_error(ss.str());
	}
	}
}

u8 CPU::readIO(u16 address)
{
	switch (address) {
	case 0xff0f:
		return interrupt_flags;
	case 0xffff:
		return interrupt_enable;
	default:
		return 0xff;
	}
}

void CPU::writeIO(u16 address, u8 value)
{
	switch (address) {
	case 0xff0f:
		interrupt_flags = value;
		break;
	case 0xffff:
		interrupt_enable = value;
		break;
	}
}
