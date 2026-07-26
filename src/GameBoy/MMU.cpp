#include <GBMU/GameBoy.hpp>
#include <GBMU/MMU.hpp>
#include <bios.h>

using namespace GBMU;

MMU::MMU(GameBoy &_gb) : gb(_gb)
{
	read_handlers.fill(nullptr);
	write_handlers.fill(nullptr);

	register_handler_range(
	    0x0000, 0x7fff, [this](u16 addr) { return gb.getCartridge().read_byte(addr); },
	    [this](u16 addr, u8 value) { gb.getCartridge().write_byte(addr, value); });
	register_handler_range(
	    0xa000, 0xbfff, [this](u16 addr) { return gb.getCartridge().read_byte(addr); },
	    [this](u16 addr, u8 value) { gb.getCartridge().write_byte(addr, value); });
	register_handler_range(
	    0xc000, 0xcfff, [this](u16 addr) { return wram[addr - 0xc000]; },
	    [this](u16 addr, u8 value) { wram[addr - 0xc000] = value; });
	register_handler_range(
	    0xd000, 0xdfff, [this](u16 addr) { return wram[addr - 0xc000]; },
	    [this](u16 addr, u8 value) { wram[addr - 0xc000] = value; });
	register_handler_range(
	    0xe000, 0xefff, [this](u16 addr) { return wram[addr - 0xe000]; },
	    [this](u16 addr, u8 value) { wram[addr - 0xe000] = value; });
	register_handler_range(
	    0xf000, 0xfdff, [this](u16 addr) { return wram[addr - 0xe000]; },
	    [this](u16 addr, u8 value) { wram[addr - 0xe000] = value; });
	register_handler(
	    0xff50, [this](u16) { return bios_disabled; },
	    [this](u16, u8 value) { bios_disabled = value; });
	register_handler_range(
	    0xff80, 0xfffe, [this](u16 addr) { return hram[addr - 0xff80]; },
	    [this](u16 addr, u8 value) { hram[addr - 0xff80] = value; });

	bios_disabled = 0x01;
}

MMU::~MMU() {}

u8 MMU::read_byte(u16 address)
{
	if (address < 0x100 && bios_disabled == 0)
		return dmg_bios[address];

	if (read_handlers[address])
		return read_handlers[address](address);

	return 0xff;
}

void MMU::write_byte(u16 address, u8 value)
{
	if (write_handlers[address])
		write_handlers[address](address, value);
}

void MMU::register_handler(u16 address, ReadHandler read_handler, WriteHandler write_handler)
{
	read_handlers[address]  = read_handler;
	write_handlers[address] = write_handler;
}

void MMU::register_handler_range(u16 start, u16 end, ReadHandler read_handler,
                                 WriteHandler write_handler)
{
	for (u16 addr = start; addr <= end; addr++) {
		read_handlers[addr]  = read_handler;
		write_handlers[addr] = write_handler;
	}
}
