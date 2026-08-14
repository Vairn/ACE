cmake_minimum_required(VERSION 3.14.0)
project(ACE
	LANGUAGES C
	DESCRIPTION "Amiga C Engine"
	HOMEPAGE_URL "https://github.com/AmigaPorts/ACE"
)

# Host SDL chipset backend (must be set before the AMIGA guard)
option(ACE_HOST "Build ACE with the host SDL chipset backend (Windows/Linux), not Amiga." OFF)

# TODO: replace with PROJECT_IS_TOP_LEVEL with cmake 3.21+
get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
	# needed for helper fns
	set(ACE_DIR ${CMAKE_CURRENT_LIST_DIR} PARENT_SCOPE)
else()
	set(ACE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

# Adhere to GNU filesystem layout conventions
include(GNUInstallDirs)

# Lowercase project name for binaries and packaging
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

# Version number in format X.Y.Z
set(VER_X 0)
set(VER_Y 0)
set(VER_Z 1)
set(VER_FULL "${VER_X}.${VER_Y}.${VER_Z}")

if(NOT AMIGA AND NOT ACE_HOST)
	message(SEND_ERROR "[ACE] This project only compiles for Amiga (or ACE_HOST=ON)")
endif()

file(GLOB_RECURSE SOURCES src/ace/*.c src/fixmath/*.c)
if(ACE_HOST)
	list(FILTER SOURCES EXCLUDE REGEX ".*/system\\.c$")
endif()
file(GLOB HEADERS_ACE include/ace/*.h)
file(GLOB HEADERS_ACE_GENERIC include/ace/generic/*.h)
file(GLOB HEADERS_ACE_UTILS include/ace/utils/*.h)
file(GLOB HEADERS_ACE_MANAGERS include/ace/managers/*.h)
file(GLOB HEADERS_ACE_MANAGERS_VP include/ace/managers/viewport/*.h)
file(GLOB HEADERS_FIXMATH include/fixmath/*.h)
set(
	HEADERS
	${HEADERS_ACE} ${HEADERS_ACE_GENERIC} ${HEADERS_ACE_UTILS}
	${HEADERS_ACE_MANAGERS} ${HEADERS_ACE_MANAGERS_VP} ${HEADERS_FIXMATH}
)

include(cmake/ace_config.cmake)

# Linux/other UNIX get a lower-case binary name
set(TARGET_NAME ${PROJECT_NAME_LOWER})

add_library(${TARGET_NAME} ${ACE_LIBRARY_KIND} ${SOURCES} ${HEADERS})

set(CMAKE_C_STANDARD 11)
target_compile_definitions(${TARGET_NAME} PUBLIC AMIGA)
if(ACE_HOST)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_HOST)
	if(MSVC)
		target_compile_options(${TARGET_NAME} PRIVATE /W3 /wd4244 /wd4267 /wd4311 /wd4312 /wd4047 /wd4028)
		target_compile_definitions(${TARGET_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -g -fno-omit-frame-pointer)
	endif()
else()
	target_compile_options(${TARGET_NAME} PRIVATE $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>>:-Wall -Wextra>)
	target_compile_options(${TARGET_NAME} PUBLIC $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>>:-fomit-frame-pointer>)
endif()

if(ACE_DEBUG)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_DEBUG)
endif()

if(ACE_DEBUG_UAE)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_DEBUG_UAE)
endif()
if(ACE_DEBUG_SERIAL)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_DEBUG_SERIAL)
endif()

if(NOT ACE_BOB_WRAP_Y)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_NO_BOB_WRAP_Y)
endif()
if(ACE_BOB_ALWAYS_ON_SCROLL_BUFFER)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_BOB_ALWAYS_ON_SCROLL_BUFFER)
endif()
if(ACE_BOB_PRISTINE_BUFFER)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_BOB_PRISTINE_BUFFER)
endif()
if(ACE_USE_ECS_FEATURES)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_USE_ECS_FEATURES)
endif()
target_compile_definitions(${TARGET_NAME} PUBLIC ACE_TILEBUFFER_TILE_TYPE=${ACE_TILEBUFFER_TILE_TYPE})
if(ACE_SCROLLBUFFER_POT_BITMAP_HEIGHT)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_SCROLLBUFFER_POT_BITMAP_HEIGHT)
endif()
if(ACE_SCROLLBUFFER_ENABLE_SCROLL_X)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_SCROLLBUFFER_ENABLE_SCROLL_X)
endif()
if(ACE_SCROLLBUFFER_ENABLE_SCROLL_Y)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_SCROLLBUFFER_ENABLE_SCROLL_Y)
endif()
target_compile_definitions(${TARGET_NAME} PUBLIC ACE_SCROLLBUFFER_X_MARGIN_SIZE=${ACE_SCROLLBUFFER_X_MARGIN_SIZE})
target_compile_definitions(${TARGET_NAME} PUBLIC ACE_SCROLLBUFFER_Y_MARGIN_SIZE=${ACE_SCROLLBUFFER_Y_MARGIN_SIZE})
if(ACE_FILE_USE_ONLY_DISK)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_FILE_USE_ONLY_DISK)
endif()

if(ACE_USE_AGA_FEATURES)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_USE_ECS_FEATURES ACE_USE_AGA_FEATURES)
endif()

if(NOT ACE_HOST)
	if(M68K_COMPILER MATCHES "Bartman")
		include(cmake/CPM.cmake)
		CPMAddPackage(
			NAME bartman_gcc_support
			GITHUB_REPOSITORY AmigaPorts/bartman_gcc_support
			GIT_TAG latest
		)
		# Workaround for nested OBJECT library
		# https://stackoverflow.com/questions/71040175
		target_link_libraries(${TARGET_NAME} PUBLIC bartman_gcc_support "$<TARGET_OBJECTS:bartman_gcc_support>")
	endif()
	if(M68K_COMPILER MATCHES "Bartman" OR USE_MINI_STD)
		target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/mini_std)
		file(GLOB SOURCES_MINI_STD src/mini_std/*.c)
		file(GLOB HEADERS_MINI_STD include/mini_std/*.h)
		target_compile_definitions(${TARGET_NAME} PRIVATE PRINTF_DISABLE_SUPPORT_FLOAT PRINTF_DISABLE_SUPPORT_LONG_LONG)
		target_sources(${TARGET_NAME} PRIVATE ${SOURCES_MINI_STD} ${HEADERS_MINI_STD})
		message(STATUS "[ACE] Using mini std")
	endif()
endif()

if(ACE_HOST)
	# Default machine: A500 512/512 OCS; A1200 when AGA is on and user did not pick one.
	if(ACE_HOST_MACHINE STREQUAL "")
		if(ACE_USE_AGA_FEATURES)
			set(ACE_HOST_MACHINE "A1200")
		else()
			set(ACE_HOST_MACHINE "A500_512_512")
		endif()
	endif()
	string(TOUPPER "${ACE_HOST_MACHINE}" ACE_HOST_MACHINE_U)
	if(ACE_HOST_MACHINE_U STREQUAL "A500")
		set(_ACE_HOST_MACHINE_ENUM 0)
	elseif(ACE_HOST_MACHINE_U STREQUAL "A500_512_512")
		set(_ACE_HOST_MACHINE_ENUM 1)
	elseif(ACE_HOST_MACHINE_U STREQUAL "A500_1MB")
		set(_ACE_HOST_MACHINE_ENUM 2)
	elseif(ACE_HOST_MACHINE_U STREQUAL "A600")
		set(_ACE_HOST_MACHINE_ENUM 3)
	elseif(ACE_HOST_MACHINE_U STREQUAL "A1200")
		set(_ACE_HOST_MACHINE_ENUM 4)
	elseif(ACE_HOST_MACHINE_U STREQUAL "A1200_4MB")
		set(_ACE_HOST_MACHINE_ENUM 5)
	elseif(ACE_HOST_MACHINE_U STREQUAL "A1200_8MB")
		set(_ACE_HOST_MACHINE_ENUM 6)
	elseif(ACE_HOST_MACHINE_U STREQUAL "A4000")
		set(_ACE_HOST_MACHINE_ENUM 7)
	else()
		set(_ACE_HOST_MACHINE_ENUM 8)
	endif()
	string(TOUPPER "${ACE_HOST_MEM_MODE}" ACE_HOST_MEM_MODE_U)
	if(ACE_HOST_MEM_MODE_U STREQUAL "VIRTUAL")
		set(_ACE_HOST_MEM_MODE_ENUM 1)
	else()
		set(_ACE_HOST_MEM_MODE_ENUM 0)
	endif()

	target_compile_definitions(${TARGET_NAME} PRIVATE
		ACE_HOST_MACHINE_ENUM=${_ACE_HOST_MACHINE_ENUM}
		ACE_HOST_MEM_MODE_ENUM=${_ACE_HOST_MEM_MODE_ENUM}
		ACE_HOST_CHIP_SIZE=${ACE_HOST_CHIP_SIZE}
		ACE_HOST_FAST_SIZE=${ACE_HOST_FAST_SIZE}
	)
	if(ACE_HOST_DEBUG)
		target_compile_definitions(${TARGET_NAME} PUBLIC ACE_HOST_DEBUG)
	endif()
	if(ACE_HOST_USE_VIRTUAL_JOYSTICK)
		target_compile_definitions(${TARGET_NAME} PUBLIC ACE_HOST_USE_VIRTUAL_JOYSTICK)
	endif()

	if(NOT DEFINED ACE_HOST_MIXER_SW_CHANNELS)
		set(ACE_HOST_MIXER_SW_CHANNELS 3)
	endif()
	if(NOT DEFINED ACE_HOST_MIXER_PERIOD)
		set(ACE_HOST_MIXER_PERIOD 161)
	endif()
	if(NOT DEFINED ACE_HOST_MIXER_HW)
		set(ACE_HOST_MIXER_HW 8) # DMAF_AUD3
	endif()
	target_compile_definitions(${TARGET_NAME} PUBLIC
		ACE_HOST_MIXER_SW_CHANNELS=${ACE_HOST_MIXER_SW_CHANNELS}
		ACE_HOST_MIXER_PERIOD=${ACE_HOST_MIXER_PERIOD}
		ACE_HOST_MIXER_HW=${ACE_HOST_MIXER_HW}
	)

	target_sources(${TARGET_NAME} PRIVATE
		host/src/chipset/host_os.c
		host/src/chipset/memory.c
		host/src/chipset/stdlib_alloc.c
		host/src/chipset/chipset.c
		host/src/chipset/blitter.c
		host/src/chipset/cia.c
		host/src/chipset/paula.c
		host/src/system_host.c
		host/src/sdl/sdl_host.c
		host/src/debug/hud.c
		host/src/debug/crash.c
		host/src/mixer/mixer.c
	)
	target_include_directories(${TARGET_NAME} PUBLIC
		${PROJECT_SOURCE_DIR}/host/include
	)
	if(NOT MSVC AND NOT APPLE)
		# Route libc malloc through AllocMem(MEMF_ANY). SDL is pointed at the
		# real OS heap so the window/renderer do not eat CHIP/FAST.
		set(_ACE_MALLOC_WRAP
			"LINKER:--wrap,malloc"
			"LINKER:--wrap,calloc"
			"LINKER:--wrap,realloc"
			"LINKER:--wrap,free"
			"LINKER:--wrap,strdup"
			"LINKER:--wrap,_strdup"
			"LINKER:--wrap,_msize"
		)
		target_link_options(${TARGET_NAME} PUBLIC ${_ACE_MALLOC_WRAP})
	endif()

	find_package(SDL2 QUIET)
	if(NOT SDL2_FOUND)
		include(FetchContent)
		set(SDL_SHARED OFF CACHE BOOL "" FORCE)
		set(SDL_STATIC ON CACHE BOOL "" FORCE)
		set(SDL_TEST OFF CACHE BOOL "" FORCE)
		FetchContent_Declare(SDL2
			GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
			GIT_TAG release-2.30.8
			GIT_SHALLOW TRUE
		)
		FetchContent_MakeAvailable(SDL2)
	endif()
	if(TARGET SDL2::SDL2)
		target_link_libraries(${TARGET_NAME} PUBLIC SDL2::SDL2)
		target_compile_definitions(${TARGET_NAME} PRIVATE ACE_HOST_HAS_SDL)
	elseif(TARGET SDL2::SDL2-static)
		target_link_libraries(${TARGET_NAME} PUBLIC SDL2::SDL2-static)
		target_compile_definitions(${TARGET_NAME} PRIVATE ACE_HOST_HAS_SDL)
	elseif(TARGET SDL2-static)
		target_link_libraries(${TARGET_NAME} PUBLIC SDL2-static)
		target_compile_definitions(${TARGET_NAME} PRIVATE ACE_HOST_HAS_SDL)
	else()
		message(WARNING "[ACE] SDL2 not found — host library builds without video/audio. Install SDL2 or allow FetchContent.")
	endif()
	message(STATUS "[ACE] ACE_HOST machine=${ACE_HOST_MACHINE} mem=${ACE_HOST_MEM_MODE} virtual_joy=${ACE_HOST_USE_VIRTUAL_JOYSTICK}")
endif()

target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

include(cmake/ace_install.cmake)
include(cmake/ace_functions.cmake)

