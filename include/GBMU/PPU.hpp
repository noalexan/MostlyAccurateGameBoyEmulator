#pragma once

#include <array>
#include <span>
#include <types.h>

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 144
#define WINDOW_SCALE  4

namespace GBMU {

class GameBoy;

class PPU {
private:
	GameBoy                                      &gb;

	std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer;

	enum Mode { HBLANK = 0, VBLANK = 1, OAM_SEARCH = 2, PIXEL_TRANSFER = 3 };

	enum LCDC {
		BG_ENABLE       = 1 << 0,
		OBJ_ENABLE      = 1 << 1,
		OBJ_HEIGHT      = 1 << 2,
		BG_TILE_MAP     = 1 << 3,
		BG_TILE_DATA    = 1 << 4,
		WINDOW_ENABLE   = 1 << 5,
		WINDOW_TILE_MAP = 1 << 6,
		PPU_ENABLE      = 1 << 7
	};

	enum STAT { MODE0 = 1 << 3, MODE1 = 1 << 4, MODE2 = 1 << 5, LYC = 1 << 6 };

	int        cycles            = 0;
	bool       scanline_rendered = false;

	u8         lcdc              = 0x91;             // LCDC - LCD Control
	u8         stat              = Mode::OAM_SEARCH; // STAT - LCD Status
	u8         scy               = 0x00;             // SCY - Scroll Y
	u8         scx               = 0x00;             // SCX - Scroll X
	u8         ly                = 0x00;             // LY - LCD Y-Coordinate
	u8         lyc               = 0x00;             // LYC - LY Compare
	u8         dma               = 0x00;             // DMA - OAM DMA Transfer
	u8         bgp               = 0xFC;             // BGP - BG Palette Data
	u8         obp0              = 0xFF;             // OBP0 - Object Palette 0 Data
	u8         obp1              = 0xFF;             // OBP1 - Object Palette 1 Data
	u8         wy                = 0x00;             // WY - Window Y Position
	u8         wx                = 0x00;             // WX - Window X Position minus 7

	inline u16 compute_tile_address(u8 tile_index);

	struct Sprite {
		u8 y, x;
		u8 index;
		u8 attr;
	} __attribute__((packed));

	std::span<struct Sprite> sprites;

	void                     perform_dma();

	int                      i = 6; // my favorite <3

public:
	PPU(GameBoy &);
	virtual ~PPU();

	void                   tick();

	u8                     read_byte(u16 address);
	void                   write_byte(u16 address, u8 value);

	const u32             *getFramebuffer() const { return framebuffer.data(); }
	u32                   *getFramebuffer() { return framebuffer.data(); }

	std::array<u8, 0x2000> vram;
	std::array<u8, 0xA0>   oam;

	void                   rotate_palette();
};

} // namespace GBMU
