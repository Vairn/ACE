cmake_minimum_required(VERSION 3.14.0)
project(ACE
	LANGUAGES C ASM
	DESCRIPTION "Amiga C Engine"
	HOMEPAGE_URL "https://github.com/AmigaPorts/ACE"
)

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

if(AMIGA)
	file(GLOB_RECURSE ACE_SOURCES src/ace/amiga/*.c src/ace/common/*.c)
	# common/managers mirrors several amiga/managers files; compiling both
	# duplicates symbols (e.g. blit wrappers, simpleBufferIsRectVisible, bartmanLog).
	foreach(_aceDup IN ITEMS
		"${PROJECT_SOURCE_DIR}/src/ace/common/managers/blit.c"
		"${PROJECT_SOURCE_DIR}/src/ace/common/managers/viewport/simplebuffer.c"
	)
		list(REMOVE_ITEM ACE_SOURCES "${_aceDup}")
	endforeach()
	list(REMOVE_ITEM ACE_SOURCES "${PROJECT_SOURCE_DIR}/src/ace/amiga/managers/log.c")
else()
	# Assume SDL - what else can it be?
	set(ACE_SDL ON)
	file(GLOB_RECURSE ACE_SOURCES src/ace/sdl/*.c src/ace/common/*.c)
	message(STATUS "[ACE] SDL mode is experimental and still incomplete!")
endif()

file(GLOB_RECURSE FIXMATH_SOURCES src/fixmath/*.c)
set(SOURCES ${ACE_SOURCES} ${FIXMATH_SOURCES})
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
if(AMIGA)
	target_compile_definitions(${TARGET_NAME} PUBLIC AMIGA)
else()
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_SDL)
endif()
target_compile_options(${TARGET_NAME} PRIVATE $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>>:-Wall -Wextra>)
target_compile_options(${TARGET_NAME} PUBLIC $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:C>>:-fomit-frame-pointer>)

if(ACE_DEBUG)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_DEBUG)
endif()

if(NOT ACE_BOB_WRAP_Y)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_NO_BOB_WRAP_Y)
endif()
if(ACE_BOB_PRISTINE_BUFFER)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_BOB_PRISTINE_BUFFER)
endif()
if(AMIGA)
	if(ACE_USE_ECS_FEATURES)
		target_compile_definitions(${TARGET_NAME} PUBLIC ACE_USE_ECS_FEATURES)
	endif()
else()
	if(ACE_USE_ECS_FEATURES)
		target_compile_definitions(${TARGET_NAME} PUBLIC ACE_USE_ECS_FEATURES)
	endif()
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

if(AMIGA)
	if(ACE_USE_AGA_FEATURES)
		target_compile_definitions(${TARGET_NAME} PUBLIC ACE_USE_ECS_FEATURES ACE_USE_AGA_FEATURES)
	endif()
else()
	if(ACE_USE_AGA_FEATURES)
		target_compile_definitions(${TARGET_NAME} PUBLIC ACE_USE_AGA_FEATURES)
	endif()
endif()

if(ACE_FILE_USE_ONLY_DISK)
	target_compile_definitions(${TARGET_NAME} PUBLIC ACE_FILE_USE_ONLY_DISK)
endif()

if(AMIGA)
	if(ACE_DEBUG_UAE)
		target_compile_definitions(${TARGET_NAME} PUBLIC ACE_DEBUG_UAE)
	endif()

	if(M68K_COMPILER MATCHES "Bartman")
		target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/bartman)
		file(GLOB SOURCES_BARTMAN src/mini_std/*.c src/bartman/*.c src/bartman/*.s)
		file(GLOB HEADERS_BARTMAN include/mini_std/*.h include/bartman/*.h)
		target_compile_definitions(${TARGET_NAME} PRIVATE PRINTF_DISABLE_SUPPORT_FLOAT PRINTF_DISABLE_SUPPORT_LONG_LONG)
		target_sources(${TARGET_NAME} PRIVATE ${SOURCES_BARTMAN} ${HEADERS_BARTMAN})
	endif()
	if(M68K_COMPILER MATCHES "Bartman" OR USE_MINI_STD)
		target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/mini_std)
		file(GLOB SOURCES_MINI_STD src/mini_std/*.c)
		file(GLOB HEADERS_MINI_STD include/mini_std/*.h)
		target_compile_definitions(${TARGET_NAME} PRIVATE PRINTF_DISABLE_SUPPORT_FLOAT PRINTF_DISABLE_SUPPORT_LONG_LONG)
		target_sources(${TARGET_NAME} PRIVATE ${SOURCES_MINI_STD} ${HEADERS_MINI_STD})
		message(STATUS "[ACE] Using mini std")
	endif()

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
endif()

if(ACE_SDL)
	option(ACE_USE_SYSTEM_SDL2 "Use find_package(SDL2) instead of CPM" OFF)
	if(ACE_USE_SYSTEM_SDL2)
		find_package(SDL2 CONFIG REQUIRED)
		if(TARGET SDL2::SDL2-static)
			target_link_libraries(${TARGET_NAME} SDL2::SDL2-static)
			if(NOT TARGET SDL2::SDL2)
				add_library(SDL2::SDL2 ALIAS SDL2-static)
			endif()
		elseif(TARGET SDL2::SDL2)
			target_link_libraries(${TARGET_NAME} SDL2::SDL2)
		else()
			message(FATAL_ERROR "ACE_USE_SYSTEM_SDL2: SDL2 CMake package has no SDL2::SDL2 or SDL2::SDL2-static")
		endif()
	else()
		include(cmake/CPM.cmake)

		set(SDL_DIRECTX OFF)
		set(SDL2_NO_MWINDOWS ON)
		CPMAddPackage(
			NAME SDL2
			VERSION 2.28.5
			URL https://libsdl.org/release/SDL2-2.28.5.zip
		)
		if(SDL2_ADDED)
			add_library(SDL2::SDL2 ALIAS SDL2)
		endif()

		target_link_libraries(${TARGET_NAME} SDL2-static)
	endif()
endif()

target_include_directories(${TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

include(cmake/ace_install.cmake)
include(cmake/ace_functions.cmake)
