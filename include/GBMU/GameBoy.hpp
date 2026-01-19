#pragma once

#include <GBMU/APU.hpp>
#include <GBMU/CPU.hpp>
#include <GBMU/Cartridge.hpp>
#include <GBMU/Joypad.hpp>
#include <GBMU/MMU.hpp>
#include <GBMU/PPU.hpp>
#include <GBMU/Serial.hpp>
#include <GBMU/Timer.hpp>
#include <atomic>
#include <string>
#include <thread>

namespace GBMU {

class GameBoy {
private:
	Cartridge cartridge;
	MMU       mmu;
	APU       apu;
	PPU       ppu;
	CPU       cpu;
	Serial    serial;
	Timer     timer;
	Joypad    joypad;

public:
	GameBoy(const std::string &);
	virtual ~GameBoy();

	void       compute_frame();

	APU       &getAPU() { return apu; }
	PPU       &getPPU() { return ppu; }
	Cartridge &getCartridge() { return cartridge; }
	MMU       &getMMU() { return mmu; }
	CPU       &getCPU() { return cpu; }
	Serial    &getSerial() { return serial; }
	Timer     &getTimer() { return timer; }
	Joypad    &getJoypad() { return joypad; }
};

} // namespace GBMU
