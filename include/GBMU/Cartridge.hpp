#pragma once

#include <cstddef>
#include <string>
#include <types.h>
#include <vector>

namespace GBMU {

class Cartridge {
private:
	std::vector<u8>          rom_data;
	size_t                   rom_size;
	u8                      *sram = nullptr;
	size_t                   sram_size;

	u8                       rom_bank     = 1;
	u8                       sram_bank     = 0;
	bool                     sram_enabled  = false;
	u8                       banking_mode = 0;

	static const std::string saves_folder_path;
	std::string              save_file_path;

	static const std::string CARTRIDGE_TYPES[256];

public:
	Cartridge(const std::string &filename);
	virtual ~Cartridge();

	std::string getTitle() const;
	u8          getCartridgeType() const;
	std::string getCartridgeTypeString() const;
	u8          getRomSize() const;
	u8          getRamSize() const;
	u8          getLicenseCode() const;
	u8          getHeaderChecksum() const;
	u16         getGlobalChecksum() const;

	const char *getSaveFilePath();

	u8         *getRomData();
	const u8   *getRomData() const;

	size_t      getRomDataSize() const;
	size_t      getRamDataSize() const;

	u8          read_byte(u16 address);
	void        write_byte(u16 address, u8 value);
};

} // namespace GBMU
