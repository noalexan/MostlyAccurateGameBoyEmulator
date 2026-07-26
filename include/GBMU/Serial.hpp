#pragma once

#include <cstdint>
#include <types.h>

namespace GBMU {

class GameBoy;

class Serial {
private:
	GameBoy &gb;

	u8 serial_data    = 0;
	u8 serial_control = 0;

public:
	Serial(GameBoy &);
	virtual ~Serial();

	u8   read_byte(u16 address);
	void write_byte(u16 address, u8 value);

	u8 &getSerialData() { return serial_data; }
	u8 &getSerialControl() { return serial_control; }
};

} // namespace GBMU
