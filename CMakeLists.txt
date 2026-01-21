cmake_minimum_required(VERSION 3.10)

project(gbmu VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/GameBoy/GameBoy.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/Cartridge.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/CPU.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/MMU.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/PPU.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/APU.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/Serial.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/Timer.cpp
    ${CMAKE_SOURCE_DIR}/src/GameBoy/Joypad.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lib/libretro-common/include)

# libgbmu.so

## OpenSSL
find_package(OpenSSL REQUIRED)

add_library(gbmu SHARED ${SOURCES})
target_link_libraries(gbmu OpenSSL::Crypto)

# emulator

## SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(emulator ${CMAKE_SOURCE_DIR}/src/main.cpp)
target_link_libraries(emulator gbmu ${SDL2_LIBRARIES})

# gbmu_libretro.so

add_library(core SHARED EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/src/retroarch.cpp)
set_target_properties(core PROPERTIES OUTPUT_NAME "gbmu_libretro" PREFIX "")
target_link_libraries(core gbmu)
