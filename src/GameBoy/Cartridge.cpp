#include <GBMU/Cartridge.hpp>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <types.h>
#include <unistd.h>

using namespace GBMU;

const std::string Cartridge::CARTRIDGE_TYPES[256] = {
    "ROM ONLY",
    "MBC1",
    "MBC1+RAM",
    "MBC1+RAM+BATTERY",
    "MBC2",
    "MBC2+BATTERY",
    "ROM+RAM",
    "ROM+RAM+BATTERY",
    "MMM01",
    "MMM01+RAM",
    "MMM01+RAM+BATTERY",
    "MBC3+TIMER+BATTERY",
    "MBC3+TIMER+RAM+BATTERY",
    "MBC3",
    "MBC3+RAM",
    "MBC3+RAM+BATTERY",
    "MBC5",
    "MBC5+RAM",
    "MBC5+RAM+BATTERY",
    "MBC5+RUMBLE",
    "MBC5+RUMBLE+RAM",
    "MBC5+RUMBLE+RAM+BATTERY",
    "MBC6",
    "MBC7+SENSOR+RUMBLE+RAM+BATTERY",
    "MBC8",
    "MBC8+RAM",
    "MBC8+RAM+BATTERY",
};

Cartridge::Cartridge(const std::string &filename) : save_file_path(filename + ".sav")
{
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		throw std::runtime_error(strerror(errno));
	}

	struct stat sb;
	if (fstat(fd, &sb) < 0) {
		close(fd);
		throw std::runtime_error(strerror(errno));
	}

	u8 *mapped = reinterpret_cast<u8 *>(mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
	if (mapped == MAP_FAILED) {
		close(fd);
		throw std::runtime_error(strerror(errno));
	}

	rom_size = sb.st_size;
	rom_data.resize(rom_size);

	if (read(fd, rom_data.data(), rom_size) != rom_size) {
		close(fd);
		throw std::runtime_error(strerror(errno));
	}

	close(fd);

	if ((sram_size = getRamDataSize())) {
		std::cout << "Save file: " << save_file_path << std::endl;

		fd = open(save_file_path.c_str(), O_RDWR | O_CREAT, 0644);
		if (fd < 0) {
			throw std::runtime_error(strerror(errno));
		}

		std::cout << "Save RAM Size: 0x" << std::hex << std::setw(4) << sram_size << "\n";

		struct stat save_sb;

		if (fstat(fd, &save_sb) != 0) {
			throw std::runtime_error(strerror(errno));
		}

		std::cout << "st_size: 0x" << std::hex << std::setw(4) << save_sb.st_size << std::endl;

		if (save_sb.st_size != sram_size) {
			if (ftruncate(fd, sram_size) < 0) {
				close(fd);
				throw std::runtime_error(strerror(errno));
			}
		}

		sram = reinterpret_cast<u8 *>(
		    mmap(NULL, sram_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
		if (sram == MAP_FAILED) {
			sram = nullptr;
			close(fd);
			throw std::runtime_error(strerror(errno));
		}
	}
}

Cartridge::~Cartridge()
{
	if (sram) {
		msync(sram, sram_size, MS_SYNC);
		munmap(sram, sram_size);
	}
}

std::string Cartridge::getTitle() const
{
	if (rom_size < 0x144 || rom_data.empty())
		return "";

	std::string title;

	for (int i = 0; i < 16; i++) {
		char c = rom_data[0x134 + i];

		if (('A' > c || c > 'Z') && c != 0x20)
			break;

		title += c;
	}

	while (!title.empty() && title.back() == ' ')
		title.pop_back();

	return title;
}

u8 Cartridge::getCartridgeType() const
{
	if (rom_size < 0x148)
		return 0;
	return rom_data[0x147];
}

std::string Cartridge::getCartridgeTypeString() const
{
	return CARTRIDGE_TYPES[getCartridgeType()];
}

u8 Cartridge::getRomSize() const
{
	if (rom_size < 0x149)
		return 0;
	return rom_data[0x148];
}

u8 Cartridge::getRamSize() const
{
	if (rom_size < 0x14A)
		return 0;
	return rom_data[0x149];
}

u8 Cartridge::getLicenseCode() const
{
	if (rom_size < 0x14C)
		return 0;
	return rom_data[0x14B];
}

u8 Cartridge::getHeaderChecksum() const
{
	if (rom_size < 0x14E)
		return 0;
	return rom_data[0x14D];
}

u16 Cartridge::getGlobalChecksum() const
{
	if (rom_size < 0x150)
		return 0;
	return (rom_data[0x14E] << 8) | rom_data[0x14F];
}

u8 *Cartridge::getRomData() { return rom_data.data(); }

const u8 *Cartridge::getRomData() const { return rom_data.data(); }

size_t Cartridge::getRomDataSize() const { return rom_size; }

size_t Cartridge::getRamDataSize() const
{
	int ram_size = getRamSize();
	return ram_size ? (ram_size + 1) * 0x2000 : 0;
}

u8 Cartridge::read_byte(u16 address)
{
	if (address <= 0x3fff) {
		return rom_data[address];
	} else if (address >= 0x4000 && address <= 0x7fff) {
		size_t bank_offset = rom_bank * 0x4000;
		if (bank_offset + (address - 0x4000) < rom_size) {
			return rom_data[bank_offset + (address - 0x4000)];
		}
		return 0xff;
	} else if (address >= 0xa000 && address <= 0xbfff) {
		if (sram_enabled) {
			size_t sram_offset = sram_bank * 0x2000;
			return sram[sram_offset + (address - 0xa000)];
		}
		return 0xff;
	}
	return 0xff;
}

void Cartridge::write_byte(u16 address, u8 value)
{
	u8 mbc_type = getCartridgeType();

	if (address <= 0x1fff) {
		sram_enabled = (value & 0x0f) == 0x0a;
	} else if (address >= 0x2000 && address <= 0x3fff) {
		if (mbc_type >= 0x01 && mbc_type <= 0x03) {
			u8 bank = value & 0x1f;
			if (bank == 0)
				bank = 1;
			rom_bank = bank;
		} else if (mbc_type >= 0x0f && mbc_type <= 0x13) {
			u8 bank = value & 0x7f;
			if (bank == 0)
				bank = 1;
			rom_bank = bank;
		} else if (mbc_type >= 0x19 && mbc_type <= 0x1e) {
			rom_bank = (rom_bank & 0x100) | value;
			if (rom_bank == 0)
				rom_bank = 1;
		}
	} else if (address >= 0x4000 && address <= 0x5fff) {
		if (mbc_type >= 0x01 && mbc_type <= 0x03) {
			if (banking_mode == 0) {
				rom_bank = (rom_bank & 0x1f) | ((value & 0x03) << 5);
			} else {
				sram_bank = value & 0x03;
			}
		} else if (mbc_type >= 0x0f && mbc_type <= 0x13) {
			sram_bank = value & 0x0f;
		} else if (mbc_type >= 0x19 && mbc_type <= 0x1e) {
			rom_bank = (rom_bank & 0xff) | ((value & 0x01) << 8);
			if (rom_bank == 0)
				rom_bank = 1;
		}
	} else if (address >= 0x6000 && address <= 0x7fff) {
		if (mbc_type >= 0x01 && mbc_type <= 0x03) {
			banking_mode = value & 0x01;
		}
	} else if (address >= 0xa000 && address <= 0xbfff) {
		if (sram_enabled) {
			size_t sram_offset                     = sram_bank * 0x2000;
			sram[sram_offset + (address - 0xa000)] = value;
		}
	}
}
