cmake_minimum_required(VERSION 3.16)
project(vita_sdl2 VERSION 0.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
option(PORT_SANITIZE "Enable Linux AddressSanitizer validation" OFF)
if(PORT_SANITIZE AND NOT VITA)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()
if(VITA)
  include("${VITASDK}/share/vita.cmake" REQUIRED)
endif()
find_package(SDL2 REQUIRED)
set(ENGINE_DIR third_party/asciicker)
add_library(asciicker_core STATIC
  ${ENGINE_DIR}/game.cpp ${ENGINE_DIR}/mainmenu.cpp ${ENGINE_DIR}/gamepad.cpp
  ${ENGINE_DIR}/font1.cpp ${ENGINE_DIR}/enemygen.cpp ${ENGINE_DIR}/render.cpp
  ${ENGINE_DIR}/terrain.cpp ${ENGINE_DIR}/world.cpp ${ENGINE_DIR}/inventory.cpp
  ${ENGINE_DIR}/physics.cpp ${ENGINE_DIR}/sprite.cpp ${ENGINE_DIR}/game_api.cpp ${ENGINE_DIR}/stb_vorbis.cpp
  src/platform/engine_hooks.cpp src/platform/config.cpp src/app/scene.cpp src/scripting/runtime.cpp
  src/audio/mixer.cpp src/audio/playback.cpp)
target_include_directories(asciicker_core PUBLIC ${ENGINE_DIR} src)
target_link_libraries(asciicker_core PUBLIC SDL2::SDL2)
target_compile_definitions(asciicker_core PUBLIC GAME USE_SDL PORTABLE_SDL)
if(VITA)
  # Upstream uses POSIX/newlib extensions (M_PI and strdup) from C++17.
  target_compile_definitions(asciicker_core PRIVATE _DEFAULT_SOURCE)
endif()
target_compile_options(asciicker_core PRIVATE -O2)
set_source_files_properties(src/scripting/runtime.cpp src/audio/mixer.cpp
  src/audio/playback.cpp src/platform/config.cpp PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
add_library(image_codec STATIC ${ENGINE_DIR}/upng.c ${ENGINE_DIR}/tinfl.c ${ENGINE_DIR}/rgba8.cpp)
target_include_directories(image_codec PUBLIC ${ENGINE_DIR})
target_link_libraries(asciicker_core PUBLIC image_codec)
add_library(ascii_presenter STATIC src/ui/cells.cpp src/platform/input.cpp)
target_include_directories(ascii_presenter PUBLIC src ${ENGINE_DIR})
target_link_libraries(ascii_presenter PUBLIC SDL2::SDL2 image_codec)
# Pixel conversion is a hot loop even in the developer Debug build. Retain
# debug symbols while optimizing it, just as for the upstream scene renderer.
target_compile_options(ascii_presenter PRIVATE -O2 -Wall -Wextra -Wpedantic)
set(QJS_DIR third_party/quickjs)
add_library(quickjs STATIC ${QJS_DIR}/quickjs.c ${QJS_DIR}/libregexp.c
  ${QJS_DIR}/libunicode.c ${QJS_DIR}/cutils.c ${QJS_DIR}/dtoa.c)
target_include_directories(quickjs PRIVATE ${QJS_DIR})
# Upstream's inline helpers intentionally retain unused context parameters.
# Treat that third-party header as a system header for consumers, while keeping
# warnings enabled for our bridge and leaving the runtime's own flags unchanged.
target_include_directories(quickjs SYSTEM INTERFACE ${QJS_DIR})
file(STRINGS "${CMAKE_SOURCE_DIR}/${QJS_DIR}/VERSION" QJS_VERSION LIMIT_COUNT 1)
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE CONFIG_VERSION="${QJS_VERSION}"
  PORT_NO_ATOMICS)
target_link_libraries(asciicker_core PUBLIC quickjs)
target_compile_options(quickjs PRIVATE -O2 -fwrapv)
target_link_libraries(quickjs PUBLIC m)
if(NOT VITA)
  find_package(Threads REQUIRED)
  target_link_libraries(quickjs PUBLIC Threads::Threads)
endif()
add_library(script_probe STATIC src/scripting/probe.cpp)
target_link_libraries(script_probe PUBLIC quickjs)
target_include_directories(script_probe PUBLIC src)
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE asciicker_core ascii_presenter script_probe)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
# Shared manifest drives preflight, tests, and VPK packaging.
file(GLOB_RECURSE APP_ASSETS CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}/assets"
  "${CMAKE_SOURCE_DIR}/assets/*")
list(SORT APP_ASSETS)
set(MANIFEST_TEXT "")
set(VPK_ASSETS "")
foreach(ASSET IN LISTS APP_ASSETS)
  string(APPEND MANIFEST_TEXT "${ASSET}\n")
  list(APPEND VPK_ASSETS FILE "${CMAKE_SOURCE_DIR}/assets/${ASSET}" "assets/${ASSET}")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" "${MANIFEST_TEXT}")
if(VITA)
  target_link_libraries(quickjs PUBLIC pthread)
  set(VITA_APP_NAME "Asciicker Vita")
  set(VITA_TITLEID "ASCIIV001")
  set(VITA_VERSION "00.30")
  vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
  vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    NAME "${VITA_APP_NAME}" VERSION "${VITA_VERSION}"
    FILE "${CMAKE_SOURCE_DIR}/packaging/icon.png" "sce_sys/icon0.png"
    ${VPK_ASSETS}
    FILE "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" "assets/manifest.txt"
    FILE "${CMAKE_SOURCE_DIR}/third_party/asciicker/LICENSE" "licenses/asciicker.txt"
    FILE "${CMAKE_SOURCE_DIR}/third_party/quickjs/LICENSE" "licenses/quickjs.txt"
    FILE "${CMAKE_SOURCE_DIR}/third_party/SDL2-LICENSE.txt" "licenses/sdl2.txt"
    FILE "${CMAKE_SOURCE_DIR}/third_party/NOTICES.md" "licenses/NOTICES.md")
else()
  include(CTest)
  if(BUILD_TESTING)
    add_test(NAME vita3k_config COMMAND ${CMAKE_COMMAND}
      -DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/vita3k-config-test/config.yml
      -P ${CMAKE_SOURCE_DIR}/tests/vita3k_config.cmake)
    add_executable(port_tests tests/port_tests.cpp)
    target_link_libraries(port_tests PRIVATE asciicker_core ascii_presenter script_probe)
    add_test(NAME port_unit_and_scene COMMAND port_tests "${CMAKE_SOURCE_DIR}/assets"
      "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" "${CMAKE_CURRENT_BINARY_DIR}/parity-settings.cfg")
    add_test(NAME launch_smoke COMMAND ${PROJECT_NAME} --assets "${CMAKE_SOURCE_DIR}/assets"
      --manifest "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" --smoke --frames 180 --verify-gameplay)
    set_tests_properties(launch_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy" TIMEOUT 90)
    add_test(NAME missing_assets COMMAND ${PROJECT_NAME} --assets /missing-asciicker-assets --frames 1)
    set_tests_properties(missing_assets PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy" WILL_FAIL TRUE TIMEOUT 10)
    add_test(NAME audio_unavailable COMMAND ${PROJECT_NAME} --assets "${CMAKE_SOURCE_DIR}/assets"
      --manifest "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" --frames 3)
    set_tests_properties(audio_unavailable PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=unavailable-test-driver"
      PASS_REGULAR_EXPRESSION "AUDIO_UNAVAILABLE" TIMEOUT 30)
  endif()
endif()
