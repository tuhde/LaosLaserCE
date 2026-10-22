/*
 * firmware_update.h
 * Self-update of the LPC1768 flash from a firmware image (.bin) on the SD card.
 *
 *   This file is part of the LaOS project (see: http://wiki.laoslaser.org)
 *   LaOS is free software under the GNU General Public License, version 3 or later.
 *
 * Flash layout (see mbed_app.json5, the application is limited to the lower half):
 *   0x00000 - 0x3FFFF  running firmware
 *   0x40000 - 0x7FFFF  staging area for the new image
 *
 * The image is first copied to the staging area and verified while the normal
 * firmware (and the SD driver) is still running. The final copy to 0x0 is done by
 * a function that runs from RAM with interrupts disabled, since the code that is
 * executing gets erased. Power loss during that last copy leaves the board
 * without valid firmware; it can then only be recovered through SWD/USB flashing.
 */
#ifndef _FIRMWARE_UPDATE_H_
#define _FIRMWARE_UPDATE_H_

#include <stdio.h>
#include <stdint.h>

#define FW_APP_MAX_SIZE 0x40000u    // maximum image size (and size of the application region)

enum {
    FW_OK = 0,
    FW_ERR_SIZE = -1,       // image empty, unaligned or too large
    FW_ERR_HEADER = -2,     // invalid vector table or checksum
    FW_ERR_FLASH = -3,      // erase/program of the staging area failed
    FW_ERR_VERIFY = -4,     // staged image differs from the file
    FW_ERR_READ = -5        // could not read the image
};

/** Validate the vector table of an image in memory (first 32 bytes needed). */
int firmware_check_header(const uint8_t *image, uint32_t size);

/** Read the image from fp (from its current position to EOF), validate it and write it to
 *  the staging area. Returns FW_OK or a negative FW_ERR_* code. */
int firmware_stage(FILE *fp);

/** Copy the staged image to the application region and reset. Does not return.
 *  Call only after firmware_stage() returned FW_OK. */
void firmware_apply(void) __attribute__((noreturn));

#endif
