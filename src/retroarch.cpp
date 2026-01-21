#include "GBMU/PPU.hpp"
#include <GBMU/GameBoy.hpp>
#include <iostream>
#include <libretro.h>
#include <memory>

std::unique_ptr<GBMU::GameBoy>    gb;

static retro_video_refresh_t      video_cb;
static retro_audio_sample_t       audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t         input_poll_cb;
static retro_input_state_t        input_state_cb;
static retro_environment_t        environ_cb;

extern "C" void                   retro_set_environment(retro_environment_t cb) { environ_cb = cb; }

extern "C" void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }

extern "C" void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }

extern "C" void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }

extern "C" void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }

extern "C" void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

extern "C" void retro_set_controller_port_device(u32 port, u32 device) {}

extern "C" unsigned retro_api_version(void) { return RETRO_API_VERSION; }

extern "C" void     retro_get_system_info(struct retro_system_info *info)
{
	info->library_name     = "GBMU";
	info->library_version  = "1.0";
	info->valid_extensions = "gb|gbc";
	info->need_fullpath    = true;
	info->block_extract    = false;
}

extern "C" void retro_get_system_av_info(struct retro_system_av_info *info)
{
	info->geometry.base_width   = SCREEN_WIDTH;
	info->geometry.base_height  = SCREEN_HEIGHT;
	info->geometry.max_width    = SCREEN_WIDTH;
	info->geometry.max_height   = SCREEN_HEIGHT;
	info->geometry.aspect_ratio = 160.0 / 144.0;
	info->timing.fps            = 59.7;
	info->timing.sample_rate    = AUDIO_SAMPLE_RATE;
}

extern "C" u32    retro_get_region() { return 0; }

extern "C" void  *retro_get_memory_data(unsigned int) { return nullptr; }

extern "C" size_t retro_get_memory_size(u32) { return 0; }

extern "C" bool   retro_load_game(const struct retro_game_info *info)
{
	gb = std::make_unique<GBMU::GameBoy>(info->path);
	return true;
}

extern "C" void retro_init(void)
{
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
		std::cerr
		    << "Unable to define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT to RETRO_PIXEL_FORMAT_XRGB8888"
		    << std::endl;
	}
}

extern "C" void   retro_deinit(void) {}

extern "C" void   retro_reset(void) {}

extern "C" size_t retro_serialize_size(void) { return 0; }

extern "C" bool   retro_serialize(void *data, size_t len) { return false; }

extern "C" bool   retro_unserialize(const void *data, size_t len) { return false; }

extern "C" void   retro_cheat_set(unsigned index, bool enabled, const char *code) {}

extern "C" void   retro_cheat_reset() {}

extern "C" bool   retro_load_game_special(unsigned int, const retro_game_info *, size_t)
{
	return false;
}

extern "C" void    retro_unload_game() {}

static inline void handle_input(u32 retropad_input_id, enum GBMU::Joypad::Input input)
{
	if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, retropad_input_id))
		gb->getJoypad().press(input);
	else
		gb->getJoypad().release(input);
}

static void handle_inputs()
{
	handle_input(RETRO_DEVICE_ID_JOYPAD_A, GBMU::Joypad::Input::A);
	handle_input(RETRO_DEVICE_ID_JOYPAD_B, GBMU::Joypad::Input::B);
	handle_input(RETRO_DEVICE_ID_JOYPAD_START, GBMU::Joypad::Input::START);
	handle_input(RETRO_DEVICE_ID_JOYPAD_SELECT, GBMU::Joypad::Input::SELECT);
	handle_input(RETRO_DEVICE_ID_JOYPAD_UP, GBMU::Joypad::Input::UP);
	handle_input(RETRO_DEVICE_ID_JOYPAD_DOWN, GBMU::Joypad::Input::DOWN);
	handle_input(RETRO_DEVICE_ID_JOYPAD_LEFT, GBMU::Joypad::Input::LEFT);
	handle_input(RETRO_DEVICE_ID_JOYPAD_RIGHT, GBMU::Joypad::Input::RIGHT);
}

extern "C" void retro_run(void)
{
	input_poll_cb();

	handle_inputs();

	gb->compute_frame();
	video_cb(gb->getPPU().getFramebuffer(), SCREEN_WIDTH, SCREEN_HEIGHT,
	         SCREEN_WIDTH * sizeof(u32));

	gb->getAPU().compute_audio();
	audio_batch_cb(gb->getAPU().get_audio_buffer(), AUDIO_SAMPLES);
}
