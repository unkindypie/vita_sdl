cmake_minimum_required(VERSION 3.16)
project(vita_sdl2 VERSION 0.3.0 LANGUAGES C CXX)
include("${CMAKE_SOURCE_DIR}/cmake/verify_icon.cmake")
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
find_package(SDL2 2.0.18 REQUIRED)
include(cmake/embed_shaders.cmake)
add_library(engine_render STATIC src/engine/render/scene_backend.cpp src/engine/render/geometry.cpp src/engine/render/camera.cpp)
target_include_directories(engine_render PUBLIC src)
target_compile_options(engine_render PRIVATE -O3 -Wall -Wextra -Wpedantic)
add_library(graphics_device STATIC src/platform/graphics/gl_device.cpp
  src/platform/graphics/gxm_device.cpp src/platform/graphics/vita_compiler.cpp)
target_include_directories(graphics_device PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(graphics_device PUBLIC engine_render SDL2::SDL2)
target_compile_options(graphics_device PRIVATE -O3 -Wall -Wextra -Wpedantic)
set(ENGINE_DIR third_party/asciicker)
add_library(asciicker_game STATIC
  ${ENGINE_DIR}/game.cpp ${ENGINE_DIR}/mainmenu.cpp ${ENGINE_DIR}/gamepad.cpp
  ${ENGINE_DIR}/font1.cpp ${ENGINE_DIR}/enemygen.cpp src/games/asciicker/render_bridge.cpp
  ${ENGINE_DIR}/terrain.cpp ${ENGINE_DIR}/world.cpp ${ENGINE_DIR}/inventory.cpp
  ${ENGINE_DIR}/physics.cpp ${ENGINE_DIR}/sprite.cpp ${ENGINE_DIR}/game_api.cpp ${ENGINE_DIR}/stb_vorbis.cpp
  src/platform/engine_hooks.cpp src/platform/config.cpp src/platform/input.cpp
  src/games/asciicker/scene.cpp src/games/asciicker/terrain_geometry.cpp src/games/asciicker/mesh_geometry.cpp
  src/games/asciicker/ascii_resources.cpp
  src/games/asciicker/picking.cpp
  src/games/asciicker/projectiles.cpp
  src/games/asciicker/scene_camera.cpp
  src/games/asciicker/gpu_scene.cpp
  src/games/asciicker/interactions.cpp
  src/games/asciicker/terrain_changes.cpp
  src/scripting/runtime.cpp
  src/audio/mixer.cpp src/audio/playback.cpp)
target_include_directories(asciicker_game PUBLIC ${ENGINE_DIR} src)
target_link_libraries(asciicker_game PUBLIC SDL2::SDL2 engine_render)
target_compile_definitions(asciicker_game PUBLIC GAME USE_SDL PORTABLE_SDL)
if(VITA)
  # Upstream uses POSIX/newlib extensions (M_PI and strdup) from C++17.
  target_compile_definitions(asciicker_game PRIVATE _DEFAULT_SOURCE)
endif()
# Optimize shared simulation/adapters; allow hardware sqrt without errno plumbing.
# Deliberately no fast-math: physics/visibility retain IEEE comparisons.
target_compile_options(asciicker_game PRIVATE -O3 -fno-math-errno)
set_source_files_properties(src/scripting/runtime.cpp src/audio/mixer.cpp
  src/audio/playback.cpp src/platform/config.cpp PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
set_source_files_properties(src/platform/input.cpp
  PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic")
add_library(image_codec STATIC ${ENGINE_DIR}/upng.c ${ENGINE_DIR}/tinfl.c ${ENGINE_DIR}/rgba8.cpp)
target_include_directories(image_codec PUBLIC ${ENGINE_DIR})
target_link_libraries(asciicker_game PUBLIC image_codec)
add_library(ascii_assets STATIC src/ui/cells.cpp)
target_include_directories(ascii_assets PUBLIC src ${ENGINE_DIR})
target_link_libraries(ascii_assets PUBLIC image_codec)
target_compile_options(ascii_assets PRIVATE -O3 -Wall -Wextra -Wpedantic)
target_link_libraries(asciicker_game PUBLIC ascii_assets)
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
target_link_libraries(asciicker_game PUBLIC quickjs)
target_compile_options(quickjs PRIVATE -O2 -fwrapv)
target_link_libraries(quickjs PUBLIC m)
if(NOT VITA)
  find_package(Threads REQUIRED)
  target_link_libraries(quickjs PUBLIC Threads::Threads)
endif()
add_library(script_probe STATIC src/scripting/probe.cpp)
target_link_libraries(script_probe PUBLIC quickjs)
target_include_directories(script_probe PUBLIC src)
add_executable(${PROJECT_NAME} src/main.cpp src/app/graphics_fixture.cpp src/app/ascii_fixture.cpp
  src/games/asciicker/world_fixture.cpp src/platform/shader_probe.cpp src/platform/native_touch.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE asciicker_game script_probe graphics_device)
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
  # Weak compiler imports let startup diagnose an absent external module
  # instead of failing to load the executable. Native GPU play requires it.
  # graphics_device has no game-specific dependencies.
  target_link_libraries(graphics_device PUBLIC SceShaccCg_stub_weak SceKernelModulemgr_stub
    SceGxm_stub SceSysmem_stub SceDisplay_stub)
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
    add_executable(geometry_tests tests/geometry_tests.cpp)
    add_executable(terrain_geometry_tests tests/terrain_geometry_tests.cpp)
    target_link_libraries(terrain_geometry_tests PRIVATE asciicker_game)
    target_compile_options(terrain_geometry_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME terrain_geometry_contract COMMAND terrain_geometry_tests "${CMAKE_SOURCE_DIR}/assets")
    target_link_libraries(geometry_tests PRIVATE engine_render)
    target_compile_options(geometry_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME geometry_contract COMMAND geometry_tests)
    add_test(NAME gpu_scene_fixture COMMAND ${PROJECT_NAME} --gpu-fixture --frames 12)
    add_test(NAME gpu_world_fixture COMMAND ${PROJECT_NAME} --gpu-fixture --frames 12 --assets "${CMAKE_SOURCE_DIR}/assets")
    add_test(NAME gpu_playable_parity COMMAND ${PROJECT_NAME} --gpu-scene --smoke --frames 600 --verify-gameplay)
    set_tests_properties(gpu_playable_parity PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1;SDL_AUDIODRIVER=dummy"
      TIMEOUT 120)
    set_tests_properties(gpu_world_fixture PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1" TIMEOUT 45)
    set_tests_properties(gpu_scene_fixture PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1" TIMEOUT 45)
    if(PORT_SANITIZE)
      # Fedora SDL2-compat/SDL3 offscreen EGL + llvmpipe retain 5856 bytes in
      # driver/JIT allocations after teardown. Keep ASan bounds/use-after-free
      # checks here; leak detection stays enabled in every non-GL test.
      set_property(TEST gpu_scene_fixture APPEND PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
      set_property(TEST gpu_world_fixture APPEND PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
      set_property(TEST gpu_playable_parity APPEND PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    endif()
    add_executable(engine_render_tests tests/engine_render_tests.cpp)
    add_executable(touch_contacts_tests tests/touch_contacts_tests.cpp)
    target_include_directories(touch_contacts_tests PRIVATE src)
    target_compile_options(touch_contacts_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME touch_contacts COMMAND touch_contacts_tests)
    target_link_libraries(engine_render_tests PRIVATE engine_render)
    target_compile_options(engine_render_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME engine_render_contract COMMAND engine_render_tests)
    add_executable(scene_backend_tests tests/scene_backend_tests.cpp)
    target_link_libraries(scene_backend_tests PRIVATE asciicker_game)
    target_compile_options(scene_backend_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME scene_backend_dispatch COMMAND scene_backend_tests)
    add_test(NAME native_shader_probe_requires_vita COMMAND ${PROJECT_NAME} --probe-gpu)
    set_tests_properties(native_shader_probe_requires_vita PROPERTIES WILL_FAIL TRUE TIMEOUT 10)
    add_test(NAME packaging_icon COMMAND ${CMAKE_COMMAND}
      -P ${CMAKE_SOURCE_DIR}/cmake/verify_icon.cmake)
    add_test(NAME packaging_icon_reject_jpeg COMMAND ${CMAKE_COMMAND}
      -DICON=${CMAKE_SOURCE_DIR}/packaging/IMG_4564.JPG
      -P ${CMAKE_SOURCE_DIR}/cmake/verify_icon.cmake)
    set_tests_properties(packaging_icon_reject_jpeg PROPERTIES WILL_FAIL TRUE)
    add_test(NAME vita3k_config COMMAND ${CMAKE_COMMAND}
      -DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/vita3k-config-test/config.yml
      -P ${CMAKE_SOURCE_DIR}/tests/vita3k_config.cmake)
    add_executable(port_tests tests/port_tests.cpp)
    target_link_libraries(port_tests PRIVATE asciicker_game script_probe)
    add_test(NAME port_unit_and_scene COMMAND port_tests "${CMAKE_SOURCE_DIR}/assets"
      "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" "${CMAKE_CURRENT_BINARY_DIR}/parity-settings.cfg")
    add_test(NAME launch_smoke COMMAND ${PROJECT_NAME} --assets "${CMAKE_SOURCE_DIR}/assets"
      --manifest "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" --smoke --frames 180 --verify-gameplay)
    set_tests_properties(launch_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1;SDL_AUDIODRIVER=dummy" TIMEOUT 90)
    add_test(NAME missing_assets COMMAND ${PROJECT_NAME} --assets /missing-asciicker-assets --frames 1)
    set_tests_properties(missing_assets PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy" WILL_FAIL TRUE TIMEOUT 10)
    add_test(NAME audio_unavailable COMMAND ${PROJECT_NAME} --assets "${CMAKE_SOURCE_DIR}/assets"
      --manifest "${CMAKE_CURRENT_BINARY_DIR}/asset_manifest.txt" --frames 3)
    set_tests_properties(audio_unavailable PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1;SDL_AUDIODRIVER=unavailable-test-driver"
      PASS_REGULAR_EXPRESSION "AUDIO_UNAVAILABLE" TIMEOUT 30)
    if(PORT_SANITIZE)
      set_property(TEST launch_smoke audio_unavailable APPEND PROPERTY ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    endif()
  endif()
endif()
