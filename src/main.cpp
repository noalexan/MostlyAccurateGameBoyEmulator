#include <GBMU/GameBoy.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <thread>

std::atomic<bool> running = true;
std::atomic<bool> speedup = false;

static void       pollEvents(GBMU::GameBoy *gb)
{
#define handle_scancode(scancode, func)                                                            \
	case scancode:                                                                                 \
		func;                                                                                      \
		break;

	while (running) {
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {
#define handle_input(scancode, input) handle_scancode(scancode, gb->getJoypad().press(input))

					handle_input(SDL_SCANCODE_W, GBMU::Joypad::Input::UP);
					handle_input(SDL_SCANCODE_A, GBMU::Joypad::Input::LEFT);
					handle_input(SDL_SCANCODE_S, GBMU::Joypad::Input::DOWN);
					handle_input(SDL_SCANCODE_D, GBMU::Joypad::Input::RIGHT);
					handle_input(SDL_SCANCODE_Q, GBMU::Joypad::Input::SELECT);
					handle_input(SDL_SCANCODE_E, GBMU::Joypad::Input::START);
					handle_input(SDL_SCANCODE_SEMICOLON, GBMU::Joypad::Input::A);
					handle_input(SDL_SCANCODE_L, GBMU::Joypad::Input::B);

#undef handle_input

					handle_scancode(SDL_SCANCODE_SPACE, speedup = true);
					handle_scancode(SDL_SCANCODE_LALT, gb->getPPU().rotate_palette());

				default:
					break;
				}
				break;
			case SDL_KEYUP:
				switch (event.key.keysym.scancode) {
#define handle_input(scancode, input) handle_scancode(scancode, gb->getJoypad().release(input))

					handle_input(SDL_SCANCODE_W, GBMU::Joypad::Input::UP);
					handle_input(SDL_SCANCODE_A, GBMU::Joypad::Input::LEFT);
					handle_input(SDL_SCANCODE_S, GBMU::Joypad::Input::DOWN);
					handle_input(SDL_SCANCODE_D, GBMU::Joypad::Input::RIGHT);
					handle_input(SDL_SCANCODE_Q, GBMU::Joypad::Input::SELECT);
					handle_input(SDL_SCANCODE_E, GBMU::Joypad::Input::START);
					handle_input(SDL_SCANCODE_SEMICOLON, GBMU::Joypad::Input::A);
					handle_input(SDL_SCANCODE_L, GBMU::Joypad::Input::B);

#undef handle_input

					handle_scancode(SDL_SCANCODE_SPACE, speedup = false);

				default:
					break;
				}
				break;
			default:
				break;
			}
		}
	}
#undef handle_scancode
}

static void audioCallback(void *userdata, u8 *stream, int len)
{
	GBMU::APU *apu = reinterpret_cast<GBMU::APU *>(userdata);

	apu->compute_audio();
	SDL_memcpy(stream, apu->get_audio_buffer(), AUDIO_SAMPLES_BUFFER_SIZE * sizeof(s16));
}

static void run(GBMU::GameBoy *gb)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	std::string   title    = "GBMU - " + gb->getCartridge().getTitle();

	SDL_Window   *window   = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
	                                          SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH * WINDOW_SCALE,
	                                          SCREEN_HEIGHT * WINDOW_SCALE, SDL_WINDOW_SHOWN);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_Texture  *texture =
	    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
	                      SCREEN_WIDTH, SCREEN_HEIGHT);

	SDL_AudioSpec want, have;
	SDL_memset(&want, 0, sizeof(want));

	want.freq                      = AUDIO_SAMPLE_RATE;
	want.format                    = AUDIO_S16SYS;
	want.channels                  = AUDIO_CHANNELS;
	want.samples                   = AUDIO_SAMPLES;
	want.callback                  = audioCallback;
	want.userdata                  = &gb->getAPU();

	SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
	if (audio_device != 0) {
		SDL_PauseAudioDevice(audio_device, 0);
	}

	const auto  FRAME_TIME   = std::chrono::milliseconds(16);
	std::thread event_thread = std::thread(pollEvents, gb);

	while (running) {
		auto frame_start = std::chrono::high_resolution_clock::now();

		gb->compute_frame();

		SDL_UpdateTexture(texture, nullptr, gb->getPPU().getFramebuffer(), SCREEN_WIDTH * 4);
		SDL_RenderCopy(renderer, texture, nullptr, nullptr);
		SDL_RenderPresent(renderer);

		auto frame_elapsed = std::chrono::high_resolution_clock::now() - frame_start;
		if (speedup == false && frame_elapsed < FRAME_TIME) {
			std::this_thread::sleep_for(FRAME_TIME - frame_elapsed);
		}
	}

	if (event_thread.joinable()) {
		event_thread.join();
	}

	if (audio_device != 0) {
		SDL_CloseAudioDevice(audio_device);
	}

	if (texture)
		SDL_DestroyTexture(texture);
	if (renderer)
		SDL_DestroyRenderer(renderer);
	if (window)
		SDL_DestroyWindow(window);
}

int main(int argc, char *argv[])
{
	auto gb = std::make_unique<GBMU::GameBoy>(argv[1]);

	run(gb.get());

	return 0;
}
