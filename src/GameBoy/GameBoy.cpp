#include <GBMU/GameBoy.hpp>
#include <iomanip>
#include <iostream>
#include <string>

using namespace GBMU;

GameBoy::GameBoy(const std::string &filename)
    : cartridge(filename), mmu(*this), apu(*this), ppu(*this), cpu(*this), serial(*this),
      timer(*this), joypad(*this)
{
	std::cerr << "\033[1;33m" << cartridge.getTitle() << "\033[0m" << std::endl
	          << "  Type: " << cartridge.getCartridgeTypeString() << std::endl
	          << "  ROM Size: 0x" << std::hex << std::setw(2) << std::setfill('0')
	          << (int)cartridge.getRomSize() << std::dec << " ("
	          << cartridge.getRomDataSize() / 1024 << " KB)" << std::endl
	          << "  RAM Size: 0x" << std::hex << std::setw(2) << std::setfill('0')
	          << (int)cartridge.getRamSize() << std::dec << " ("
	          << cartridge.getRamDataSize() / 1024 << " KB)" << std::endl;
}

GameBoy::~GameBoy() {}

void GameBoy::compute_frame()
{
	for (int i = 0; i < 70224; i++) {
		cpu.tick();
		ppu.tick();
		timer.tick();
	}
}
