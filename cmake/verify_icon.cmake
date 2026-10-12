cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ICON)
  set(ICON "${CMAKE_CURRENT_LIST_DIR}/../packaging/icon.png")
endif()
# PNG signature + IHDR: 128x128, 8-bit indexed colour, no interlacing.
# RGBA with 8 bits PER CHANNEL is not the required 8-bit indexed PNG.
file(READ "${ICON}" HEADER LIMIT 29 HEX)
if(NOT HEADER STREQUAL "89504e470d0a1a0a0000000d4948445200000080000000800803000000")
  message(FATAL_ERROR "Vita icon must be a 128x128, 8-bit indexed, non-interlaced PNG: ${ICON}")
endif()
message(STATUS "ICON_OK: 128x128 indexed PNG")
