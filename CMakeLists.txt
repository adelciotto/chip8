cmake_minimum_required(VERSION 3.5.1)
project(chip8)

set(CMAKE_C_STANDARD 99)

set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/SDL2)

file(GLOB HEADERS ${PROJECT_SOURCE_DIR}/src/*.h)
file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/src/*.c)
add_executable(chip8 ${HEADERS} ${SOURCES})
target_link_libraries(chip8 SDL2main SDL2-static)

add_custom_command(TARGET chip8 POST_BUILD COMMAND
    ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/roms/ $<TARGET_FILE_DIR:${PROJECT_NAME}>)
