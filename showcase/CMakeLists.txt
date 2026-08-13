cmake_minimum_required(VERSION 3.14)
project(showcase)

option(ACE_HOST "Build showcase with ACE host SDL chipset backend" OFF)

# Lowercase project name for binaries and packaging
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

# Version number in format X.Y.Z
set(VER_X  0)
set(VER_Y 0)
set(VER_Z 1)
set(VER_FULL "${VER_X}.${VER_Y}.${VER_Z}")

if(NOT AMIGA AND NOT ACE_HOST)
	message(SEND_ERROR "This project only compiles for Amiga (or ACE_HOST=ON)")
endif()

set(CMAKE_C_STANDARD 11)
if(ACE_HOST)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAMIGA -DACE_HOST")
	if(NOT MSVC)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g -fno-omit-frame-pointer")
	endif()
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAMIGA -Wall -Wextra -fomit-frame-pointer")
endif()
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DACE_DEBUG")
file(GLOB_RECURSE SOURCES src/*.c)
file(GLOB_RECURSE HEADERS src/*.h)


include_directories(${PROJECT_SOURCE_DIR}/src)

set(RES_DIR ${CMAKE_CURRENT_LIST_DIR}/res)
set(DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/data)

# ACE
add_subdirectory(.. ace)
include_directories(../include)
if(ACE_HOST)
	include_directories(../host/include)
endif()

# Linux/other UNIX get a lower-case binary name
set(TARGET_NAME ${PROJECT_NAME_LOWER})

if(ELF2HUNK AND NOT ACE_HOST)
  # Add elf2hunk step for Bartman compiler
  set(GAME_EXECUTABLE ${TARGET_NAME}.elf) # Intermediate executable
  set(GAME_OUTPUT_EXECUTABLE ${TARGET_NAME}.exe) # Use this to launch the game
  add_executable(${GAME_EXECUTABLE} ${SOURCES} ${HEADERS})
  add_custom_command(
    TARGET ${GAME_EXECUTABLE} POST_BUILD
    COMMAND ${ELF2HUNK} ${GAME_EXECUTABLE} ${GAME_OUTPUT_EXECUTABLE}
  )
else()
  # Just produce the executable with Bebbo compiler
  SET(GAME_EXECUTABLE ${TARGET_NAME})
  SET(GAME_OUTPUT_EXECUTABLE ${TARGET_NAME})
  add_executable(${GAME_EXECUTABLE} ${SOURCES} ${HEADERS})
endif()

target_link_libraries(${GAME_EXECUTABLE} ace)

if(ACE_HOST AND NOT MSVC AND NOT APPLE)
	target_link_options(${GAME_EXECUTABLE} PRIVATE
		"LINKER:--wrap,malloc"
		"LINKER:--wrap,calloc"
		"LINKER:--wrap,realloc"
		"LINKER:--wrap,free"
		"LINKER:--wrap,strdup"
		"LINKER:--wrap,_strdup"
		"LINKER:--wrap,_msize"
	)
endif()

if(ACE_HOST AND TARGET SDL2::SDL2main)
	target_link_libraries(${GAME_EXECUTABLE} SDL2::SDL2main)
endif()

set(RES_DIR ${CMAKE_CURRENT_LIST_DIR}/res)
set(DATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${DATA_DIR})

set(_ACE_HOST_HAS_TOOLS FALSE)
if(EXISTS "${ACE_DIR}/tools/bin/font_conv.exe" OR EXISTS "${ACE_DIR}/tools/bin/font_conv")
	set(_ACE_HOST_HAS_TOOLS TRUE)
endif()

if(NOT ACE_HOST OR _ACE_HOST_HAS_TOOLS)
	set(AMIDB32_PLT_PATH ${DATA_DIR}/amidb32.plt)
	convertPalette(${GAME_EXECUTABLE} ${RES_DIR}/amidb32.gpl ${AMIDB32_PLT_PATH})
	convertPalette(${GAME_EXECUTABLE} ${RES_DIR}/blitToSmall.gpl ${DATA_DIR}/blitToSmall.plt)

	convertBitmaps(
		TARGET ${GAME_EXECUTABLE} PALETTE ${AMIDB32_PLT_PATH}
		SOURCES ${RES_DIR}/32c_pal.png DESTINATIONS ${DATA_DIR}/32c_pal.bm
	)
	convertBitmaps(
		TARGET ${GAME_EXECUTABLE} PALETTE ${AMIDB32_PLT_PATH} INTERLEAVED
		SOURCES ${RES_DIR}/32c_pal_interleaved.png DESTINATIONS ${DATA_DIR}/32c_pal_interleaved.bm
	)

	convertFont(
	  TARGET ${GAME_EXECUTABLE}
		SOURCE ${RES_DIR}/silkscreen DESTINATION ${DATA_DIR}/silkscreen.fnt
	)
elseif(ACE_HOST)
	message(STATUS "[showcase] ACE_HOST: skipping asset conversion (build tools: cmake -S tools -B tools/build-host && cmake --build tools/build-host)")
endif()

if(ACE_HOST)
	set(ACE_HOST_DIST_DIR "${ACE_DIR}/dist/showcase-host" CACHE PATH "Host showcase distribute folder")
	set(_SDL2_DLL "")
	if(TARGET SDL2::SDL2)
		get_target_property(_SDL2_DLL SDL2::SDL2 IMPORTED_LOCATION)
		if(NOT _SDL2_DLL)
			get_target_property(_SDL2_DLL SDL2::SDL2 IMPORTED_LOCATION_RELEASE)
		endif()
		if(_SDL2_DLL MATCHES "\\.dll\\.a$")
			get_filename_component(_sdl_libdir "${_SDL2_DLL}" DIRECTORY)
			get_filename_component(_sdl_root "${_sdl_libdir}" DIRECTORY)
			set(_SDL2_DLL "${_sdl_root}/bin/SDL2.dll")
		endif()
	endif()
	if(NOT _SDL2_DLL OR NOT EXISTS "${_SDL2_DLL}")
		find_file(_SDL2_DLL NAMES SDL2.dll PATHS ${CMAKE_PREFIX_PATH} ENV PATH PATH_SUFFIXES bin NO_CACHE)
	endif()
	if(NOT _SDL2_DLL OR NOT EXISTS "${_SDL2_DLL}")
		foreach(_sdl_guess
			"${CMAKE_PREFIX_PATH}/bin/SDL2.dll"
			"C:/msys64/ucrt64/bin/SDL2.dll"
		)
			if(EXISTS "${_sdl_guess}")
				set(_SDL2_DLL "${_sdl_guess}")
				break()
			endif()
		endforeach()
	endif()
	find_program(_STRIP_EXE strip)

	add_custom_target(dist
		DEPENDS ${GAME_EXECUTABLE}
		COMMAND ${CMAKE_COMMAND} -E rm -rf "${ACE_HOST_DIST_DIR}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${ACE_HOST_DIST_DIR}/data"
		COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${GAME_EXECUTABLE}>" "${ACE_HOST_DIST_DIR}/"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${DATA_DIR}" "${ACE_HOST_DIST_DIR}/data"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/README-HOST-DIST.txt" "${ACE_HOST_DIST_DIR}/README.txt"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/run-host.bat" "${ACE_HOST_DIST_DIR}/run.bat"
		COMMENT "Packaging host showcase into ${ACE_HOST_DIST_DIR}"
	)
	if(_SDL2_DLL)
		add_custom_command(TARGET dist POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy "${_SDL2_DLL}" "${ACE_HOST_DIST_DIR}/"
			COMMENT "Copy SDL2.dll into dist"
		)
	else()
		message(WARNING "[showcase] SDL2.dll not found — dist will need it next to showcase.exe")
	endif()
	if(_STRIP_EXE)
		add_custom_command(TARGET dist POST_BUILD
			COMMAND "${_STRIP_EXE}" --strip-unneeded "${ACE_HOST_DIST_DIR}/$<TARGET_FILE_NAME:${GAME_EXECUTABLE}>"
			COMMENT "Strip dist showcase.exe"
		)
	endif()
	add_custom_command(TARGET dist POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E tar cf "${ACE_DIR}/dist/showcase-host.zip" --format=zip -- showcase-host
		WORKING_DIRECTORY "${ACE_DIR}/dist"
		COMMENT "Zip dist/showcase-host.zip"
	)
endif()
