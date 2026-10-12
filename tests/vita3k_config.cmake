set(OUTPUT "${TEST_OUTPUT}")
set(PREF_PATH "/example workspace/emulator/storage/")
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/prepare_vita3k.cmake")
file(READ "${OUTPUT}" config)
foreach(required IN ITEMS
    "pref-path: \"${PREF_PATH}\""
    "backend-renderer: OpenGL"
    "v-sync: true"
    "screen-filter: Bilinear"
    "warn-missing-firmware: false"
    "hashless-texture-cache: false"
    "texture-cache: true"
    "disable-surface-sync: true"
    "keyboard-leftstick-up: KeyW"
    "keyboard-leftstick-left: KeyA"
    "keyboard-leftstick-down: KeyS"
    "keyboard-leftstick-right: KeyD"
    "keyboard-button-l1: KeyQ"
    "keyboard-button-r1: KeyE"
    "keyboard-button-cross: KeyX"
    "keyboard-button-circle: KeyC"
    "keyboard-button-square: KeyZ"
    "keyboard-button-triangle: KeyV"
    "keyboard-button-start: Enter"
    "keyboard-button-select: ShiftRight")
  string(FIND "${config}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Missing emulator configuration: ${required}")
  endif()
endforeach()
set(SURFACE_SYNC ON)
set(HOST_VSYNC OFF)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/prepare_vita3k.cmake")
file(READ "${OUTPUT}" config)
string(FIND "${config}" "disable-surface-sync: false" found)
if(found EQUAL -1)
  message(FATAL_ERROR "GPU diagnostics require explicit surface synchronization")
endif()
string(FIND "${config}" "v-sync: false" found)
if(found EQUAL -1)
  message(FATAL_ERROR "Finite smoke must permit disabling host swap waits")
endif()
