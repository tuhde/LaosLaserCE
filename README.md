# LaOS laser controller firmware

Embedded software for the LaOS laser cutter controller (LAOS board, mbed LPC1768),
built on [Mbed CE](https://github.com/mbed-ce/mbed-os) with CMake and Ninja.
Configuration files for several machines are in `config/`.

## What it does

- Reads `config.txt` from the SD card and drives the X/Y/Z axes (grbl based planner,
  step pulses from a hardware timer), laser PWM, exhaust and the home/cover sensors.
- Receives job files (`*.lgc`) over Ethernet with TFTP (port from `net.port`), stores them
  on the SD card and runs them.
- Shows a menu on the 2x16 I2C display. Without a display, set `sys.nodisplay 1` (job files are
  then run as soon as they arrive) or `sys.nodisplay 0` to use the menu on the serial console.
- Updates itself from the SD card or over TFTP (see below).

## Build

Requirements: `gcc-arm-none-eabi` (tested with 14.2), `cmake` (3.19 or newer), `ninja`,
`python3` with `venv`, `git`. The Python tools (pyOCD etc.) are installed automatically into
`mbed-os/venv` on the first configure.

```
git clone --recursive https://github.com/LaosLaser/Firmware.git
cd Firmware
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Develop
ninja -C build laser iotest
```

Results: `build/laser.bin` (controller firmware) and `build/iotest.bin` (board I/O test, see
`iotest_board/main.cpp` for the keys). The firmware must stay below 256 KB (the upper half of the
flash is used to stage updates), the build prints the usage.

## Flash

Through the on-board CMSIS-DAP interface chip, with pyOCD:

```
ninja -C build flash-laser
```

Linux needs the udev rule once: `sudo cp tools/99-mbed-cmsis-dap.rules /etc/udev/rules.d/ &&
sudo udevadm control --reload && sudo udevadm trigger`.

`-DUPLOAD_METHOD=MBED` instead copies the file to the mbed USB drive. Note that pyOCD may report
"Target not detected" after programming (the reset fails); the image is written and starts after a
power cycle or a reset.

## SD card and configuration

Format the card FAT32. Copy one of the files from `config/` to the card as `config.txt` and adjust it.
Without the card the firmware reboots ("SD NOT READY"). Names with spaces are not supported.

`config.txt` can also be replaced over the network while the board runs headless
(`sys.nodisplay 1`): sending it with TFTP saves it and restarts the board.
At startup all files on the card are deleted except `config.txt` when `sys.cleandir` is 1.

## Send jobs and configuration with TFTP

```
tftp <ip> -c put job.lgc job.lgc
tftp <ip> -c get config.txt
```

The IP address comes from DHCP or the config file. The MAC address is derived from the chip serial
number (prefix `00:02:F7`), so each board keeps its own, stable address.

## Firmware update

Either copy a new `laser.bin` to the SD card (any name ending in `.bin`) and restart, or send it
with TFTP in menu mode. The file is checked (vector table checksum), staged in the upper flash half
and copied into place by a routine running from RAM. An invalid file is deleted and ignored.

## Serial console and menu simulation

The USB serial port (115200 baud, e.g. `picocom -b 115200 /dev/ttyACM0`) shows the log. If no display
answers on I2C and `sys.nodisplay` is 0, the console acts as the display and keypad: type
`8`/`2` (up/down), `4`/`6` (left/right), `5` (OK), `7` (cancel), `1` (origin), `9`/`3` (Z up/down).

## Debugging

```
arm-none-eabi-gdb build/laser.elf --eval-command \
    'target remote | openocd -c "gdb_port pipe" --file /usr/share/openocd/scripts/interface/cmsis-dap.cfg --file /usr/share/openocd/scripts/target/lpc1768.cfg -cinit -chalt'
```

Semihosting is disabled (the configuration is read from the SD card), so `reset halt` works.
At the end of a job in headless mode, memory and step interrupt statistics are printed.

## Releases

Old binaries built with mbed 2 are kept in `releases/` and are not compatible with the
SD card based configuration. See `CHANGELOG`.
