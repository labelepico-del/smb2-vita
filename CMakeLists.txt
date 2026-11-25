cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain file")
  else()
    message(FATAL_ERROR "VITASDK is not set. Install VitaSDK and export VITASDK, or pass -DCMAKE_TOOLCHAIN_FILE=...")
  endif()
endif()

project(smb2_vita CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "SMB2 Vita")
set(VITA_TITLEID  "SMB2VITA1")
set(VITA_VERSION  "01.00")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/rom_data.cpp")
  message(FATAL_ERROR
    "src/rom_data.cpp is missing.\n"
    "Build the ROM from the disassembly first:\n"
    "  Linux/macOS: tools/build_rom.sh\n"
    "  Windows:     powershell -ExecutionPolicy Bypass -File tools/build_rom.ps1")
endif()

set(QUICKNES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quicknes/nes_emu")

set(QUICKNES_SOURCES
  ${QUICKNES_DIR}/abstract_file.cpp
  ${QUICKNES_DIR}/apu_state.cpp
  ${QUICKNES_DIR}/Blip_Buffer.cpp
  ${QUICKNES_DIR}/Data_Reader.cpp
  ${QUICKNES_DIR}/Effects_Buffer.cpp
  ${QUICKNES_DIR}/emu2413.cpp
  ${QUICKNES_DIR}/emu2413_state.cpp
  ${QUICKNES_DIR}/Multi_Buffer.cpp
  ${QUICKNES_DIR}/Nes_Apu.cpp
  ${QUICKNES_DIR}/Nes_Buffer.cpp
  ${QUICKNES_DIR}/Nes_Cart.cpp
  ${QUICKNES_DIR}/Nes_Core.cpp
  ${QUICKNES_DIR}/Nes_Cpu.cpp
  ${QUICKNES_DIR}/nes_data.cpp
  ${QUICKNES_DIR}/Nes_Effects_Buffer.cpp
  ${QUICKNES_DIR}/Nes_Emu.cpp
  ${QUICKNES_DIR}/Nes_File.cpp
  ${QUICKNES_DIR}/Nes_Fme7_Apu.cpp
  ${QUICKNES_DIR}/Nes_Mapper.cpp
  ${QUICKNES_DIR}/Nes_Namco_Apu.cpp
  ${QUICKNES_DIR}/nes_ntsc.cpp
  ${QUICKNES_DIR}/Nes_Oscs.cpp
  ${QUICKNES_DIR}/Nes_Ppu.cpp
  ${QUICKNES_DIR}/Nes_Ppu_Impl.cpp
  ${QUICKNES_DIR}/Nes_Ppu_Rendering.cpp
  ${QUICKNES_DIR}/Nes_State.cpp
  ${QUICKNES_DIR}/nes_util.cpp
  ${QUICKNES_DIR}/Nes_Vrc6_Apu.cpp
  ${QUICKNES_DIR}/Nes_Vrc7.cpp
)

add_executable(smb2_vita
  src/main.cpp
  src/emulator.cpp
  src/audio.cpp
  src/rom_data.cpp
  ${QUICKNES_SOURCES}
)

target_include_directories(smb2_vita PRIVATE
  src
  ${QUICKNES_DIR}
)

target_compile_options(smb2_vita PRIVATE
  -O3
  -fno-strict-aliasing
  -Wall
  -Wextra
  -Wno-unused-parameter
)

target_link_libraries(smb2_vita
  vita2d
  SceCtrl_stub
  SceAudio_stub
  ScePower_stub
  SceDisplay_stub
  SceGxm_stub
  SceKernelThreadMgr_stub
  SceProcessmgr_stub
  m
  stdc++
)

vita_create_self(smb2_vita.self smb2_vita)

vita_create_vpk(smb2_vita.vpk ${VITA_TITLEID} smb2_vita.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE assets/sce_sys/icon0.png sce_sys/icon0.png
  FILE assets/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE assets/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE assets/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
