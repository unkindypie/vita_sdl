set(OUTPUT "${TEST_OUTPUT}")
set(PREF_PATH "/example workspace/emulator/storage/")
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/prepare_vita3k.cmake")
file(READ "${OUTPUT}" config)
foreach(required IN ITEMS
    "pref-path: \"${PREF_PATH}\""
    "backend-renderer: OpenGL"
    "screen-filter: Nearest"
    "warn-missing-firmware: false"
    "hashless-texture-cache: false"
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
