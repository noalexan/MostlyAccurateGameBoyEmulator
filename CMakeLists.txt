cmake_minimum_required(VERSION 3.10)

project(gbmu VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(OpenSSL REQUIRED)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_library(gbmu STATIC
	src/GameBoy/GameBoy.cpp
	src/GameBoy/Cartridge.cpp
	src/GameBoy/CPU.cpp
	src/GameBoy/MMU.cpp
	src/GameBoy/PPU.cpp
	src/GameBoy/APU.cpp
	src/GameBoy/Serial.cpp
	src/GameBoy/Timer.cpp
	src/GameBoy/Joypad.cpp
)

add_executable(emulator src/main.cpp)

target_link_libraries(gbmu OpenSSL::Crypto)
target_link_libraries(emulator gbmu ${SDL2_LIBRARIES})
