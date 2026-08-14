# ACE host SDL chipset backend

**Full documentation:** [docs/programming/ace_host.md](../docs/programming/ace_host.md)
(Windows port + development: architecture, CMake flags, env vars, HUD).

Windows port of ACE (and a PC development backend). Same copper, blit, extview,
and viewport managers; AMIGA and ACE_HOST are both defined. Chipset time is
DMA-slot based (Agnus), not instant blit.

## Dependencies

C compiler (MinGW GCC or MSVC), CMake 3.14+, and SDL2 via find_package or FetchContent (SDL 2.30.8).

On Windows with MSYS2 UCRT: install mingw-w64-ucrt-x86_64-gcc, mingw-w64-ucrt-x86_64-SDL2, and ninja.
Point CMake at SDL2 with -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64.

## Showcase

From the ACE repo root:

cmake -S showcase -B build-host -G Ninja -DACE_HOST=ON -DACE_DEBUG=ON -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build-host --parallel

Run from the build directory so data/ is found: cd build-host && ./showcase.exe

Distribute folder (Release, no ACE log spam, SDL2.dll + data/ + zip):

cmake -S showcase -B build-host-dist -G Ninja -DACE_HOST=ON -DACE_DEBUG=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build-host-dist --target dist --parallel

Output: dist/showcase-host/ and dist/showcase-host.zip

Asset conversion runs when ACE tools are present in tools/bin/:

cmake -S tools -B tools/build-host -G Ninja
cmake --build tools/build-host --parallel

Then reconfigure and rebuild showcase.

Amiga cross builds are unchanged: leave ACE_HOST off. Without AMIGA or ACE_HOST, CMake still errors.

## CMake flags

- ACE_HOST (OFF): enable host SDL chipset backend
- ACE_HOST_MACHINE: A500, A500_512_512 (OCS default), A500_1MB, A600, A1200 (AGA default), A1200_4MB, A1200_8MB, A4000, CUSTOM
- ACE_HOST_MEM_MODE: STRICT (fail at cap) or VIRTUAL (grow with OVER warnings)
- ACE_HOST_CHIP_SIZE / ACE_HOST_FAST_SIZE: CUSTOM machine sizes in bytes
- ACE_HOST_DEBUG (ON): on-screen HUD
- ACE_HOST_USE_VIRTUAL_JOYSTICK (OFF): WASD / numpad / SDL gamepad → JOY1DAT (port 2) + FIR1. Arrows stay CIA keys so menu `keyUse || joyUse` does not double-step. Ctrl/Z fire, Alt/X fire2. Gamepads hotplug; first SDL GameController maps to JOY1.
- ACE_DEBUG: ACE log/safety checks

## Keys

F10 toggle timing log (also `ACE_HOST_TIMING=1`), F11 memory HUD (needs ACE_HOST_DEBUG), F12 DMA/copper/alloc overlay. Mouse moves JOY0DAT (port 1); LMB/RMB/MMB map to CIA FIR0 / POTINP.

With `-DACE_HOST_USE_VIRTUAL_JOYSTICK=ON`: WASD or numpad 8462 drive JOY1DAT; arrows remain keyboard. Plug/unplug a gamepad at any time.

Keyboard goes through CIA-A SDR + SPMODE handshake so ACE `key.c` `onKeyInterrupt` runs (3-scanline wait via `getRayPos()`).

g_pCustom is mapped into a 16MiB low-4GB bus window (0xDFF000). CHIP DMA pointers are 32-bit. Native ACE copper/blit sources are used; SDL presents Denise RGB565 (640-wide, lores is pixel-doubled). PAL timing is 227 CCK × 313 lines. Copper uses leftover DMA slots (2 CCK per instruction); the blitter occupies BBUSY slots (C+D per line pixel). CIA E-clock ticks every 5 CCK.

HUD line `LN313 CW<y> BLIT<n> OK` is a timing self-check: lines per PAL frame, last copper WAIT Y, blit slots last frame.
