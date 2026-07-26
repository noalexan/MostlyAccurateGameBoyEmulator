#pragma once

#include <GBMU/Cartridge.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <types.h>

namespace GBMU {

class GameBoy;

class MMU {
public:
	using ReadHandler  = std::function<u8(u16)>;
	using WriteHandler = std::function<void(u16, u8)>;

private:
	GameBoy &gb;

	std::array<ReadHandler, 0x10000>  read_handlers;
	std::array<WriteHandler, 0x10000> write_handlers;

	u8 bios_disabled = 0;
	u8 wram[0x2000];
	u8 eram[0x2000];
	u8 io_registers[0x80];
	u8 hram[0x7F];

public:
	MMU(GameBoy &);
	virtual ~MMU();

	u8   read_byte(u16 address);
	void write_byte(u16 address, u8 value);

	void register_handler(u16 address, ReadHandler read_handler, WriteHandler write_handler);
	void register_handler_range(u16 start, u16 end, ReadHandler read_handler,
	                            WriteHandler write_handler);
};

} // namespace GBMU
