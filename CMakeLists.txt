cmake_minimum_required(VERSION 3.16)

project(vita_sdl2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(VITA)
  include("${VITASDK}/share/vita.cmake" REQUIRED)
endif()

find_package(SDL2 REQUIRED)

add_executable(${PROJECT_NAME}
  src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)

if(VITA)
  # Change these before distributing a game. A title ID must be exactly 9
  # uppercase letters/digits and must not collide with another installed app.
  set(VITA_APP_NAME "Vita SDL2 Starter")
  set(VITA_TITLEID "VITASDL02")
  set(VITA_VERSION "01.00")

  vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
  vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    NAME "${VITA_APP_NAME}"
    VERSION "${VITA_VERSION}"
  )
endif()
