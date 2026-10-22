/*
 * mac_address.cpp
 * Ethernet MAC address derived from the serial number of the LPC1768.
 *
 * Mbed OS only derives a per-board MAC from the interface chip when semihosting is
 * enabled; otherwise every board would use the same default address. This replaces
 * the (weak) mbed_mac_address() with one that reads the unique ID of the MCU
 * through the boot ROM IAP call, so no debug host is involved.
 */
#include "mbed.h"
#include "platform/mbed_mpu_mgmt.h"

#define IAP_ENTRY            0x1FFF1FF1u
#define IAP_READ_SERIAL      58

typedef void (*iap_fn_t)(uint32_t *cmd, uint32_t *res);

extern "C" void mbed_mac_address(char *mac)
{
    uint32_t cmd[5] = {IAP_READ_SERIAL, 0, 0, 0, 0};
    uint32_t res[5] = {0, 0, 0, 0, 0};

    // the boot ROM lives in the address range that the MPU marks execute-never by default
    mbed_mpu_manager_lock_ram_execution();
    core_util_critical_section_enter();
    ((iap_fn_t)IAP_ENTRY)(cmd, res);
    core_util_critical_section_exit();
    mbed_mpu_manager_unlock_ram_execution();

    // res[0] is the status, res[1..4] the 128 bit serial number. Fold it (FNV-1a) to 24 bits.
    uint32_t h = 2166136261u;
    for (int i = 1; i < 5; i++) {
        for (int b = 0; b < 32; b += 8) {
            h ^= (res[i] >> b) & 0xFF;
            h *= 16777619u;
        }
    }
    if (res[0] != 0)    // IAP failed: keep the default low bytes
        h = 0xF00000;

    mac[0] = 0x00;      // mbed / ARM OUI 00:02:F7
    mac[1] = 0x02;
    mac[2] = 0xF7;
    mac[3] = (h >> 16) & 0xFF;
    mac[4] = (h >> 8) & 0xFF;
    mac[5] = h & 0xFF;
}
