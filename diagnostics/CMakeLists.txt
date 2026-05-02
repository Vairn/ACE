cmake_minimum_required(VERSION 3.14)
project(diagnostics LANGUAGES C)

string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

if(NOT AMIGA)
	message(SEND_ERROR "This project only compiles for Amiga")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAMIGA -Wall -Wextra -O2 -fomit-frame-pointer")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DACE_DEBUG")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS src/*.h)

include_directories(${PROJECT_SOURCE_DIR}/src)

set(RES_DIR ${CMAKE_CURRENT_LIST_DIR}/res)
set(DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/data)

set(ACE_USE_AGA_FEATURES ON CACHE BOOL "" FORCE)
add_subdirectory(.. ace)
include_directories(../include)

set(TARGET_NAME ${PROJECT_NAME_LOWER})

if(ELF2HUNK)
	set(GAME_LINKED ${TARGET_NAME}.elf)
	set(GAME_EXE ${TARGET_NAME}.exe)
	add_executable(${GAME_LINKED} ${SOURCES} ${HEADERS})
	add_custom_command(
		TARGET ${GAME_LINKED} POST_BUILD
		COMMAND ${ELF2HUNK} ${GAME_LINKED} ${GAME_EXE}
	)
else()
	set(GAME_LINKED ${TARGET_NAME})
	set(GAME_EXE ${TARGET_NAME})
	add_executable(${GAME_LINKED} ${SOURCES} ${HEADERS})
endif()

target_link_libraries(${GAME_LINKED} ace)

file(COPY "${RES_DIR}/data" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
