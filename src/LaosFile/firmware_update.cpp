/*
 * firmware_update.cpp
 * Self-update of the LPC1768 flash, see firmware_update.h
 */
#include "mbed.h"
#include "FlashIAP.h"
#include "firmware_update.h"

#define STAGING_ADDR   0x40000u
#define PAGE_SIZE      1024u        // IAP "copy RAM to flash" granularity used here
#define IAP_ENTRY      0x1FFF1FF1u  // LPC17xx boot ROM IAP entry point
#define VECTOR_WORDS   8

// staged image size in bytes, rounded up to PAGE_SIZE
static uint32_t staged_size;

// one page buffer shared by staging and the RAM routine (never used at the same time)
static uint8_t ram_page[PAGE_SIZE] __attribute__((aligned(4)));

static bool ram_addr_ok(uint32_t sp)
{
    return (sp >= 0x10000000u && sp <= 0x10008000u) || (sp >= 0x2007C000u && sp <= 0x20084000u);
}

int firmware_check_header(const uint8_t *image, uint32_t size)
{
    if (size < VECTOR_WORDS * 4 || size > FW_APP_MAX_SIZE || (size & 3))
        return FW_ERR_SIZE;
    uint32_t v[VECTOR_WORDS];
    memcpy(v, image, sizeof(v));
    // the boot ROM only starts an image whose first 8 vectors sum to zero
    uint32_t sum = 0;
    for (int i = 0; i < VECTOR_WORDS; i++)
        sum += v[i];
    if (sum != 0)
        return FW_ERR_HEADER;
    if (!ram_addr_ok(v[0]))
        return FW_ERR_HEADER;
    if (!(v[1] & 1) || (v[1] & ~1u) >= size)    // reset handler: thumb address inside the image
        return FW_ERR_HEADER;
    return FW_OK;
}

int firmware_stage(FILE *fp)
{
    uint8_t *page = ram_page;

    // determine size
    if (fseek(fp, 0, SEEK_END) != 0)
        return FW_ERR_READ;
    long size = ftell(fp);
    rewind(fp);
    if (size <= 0 || size > (long)FW_APP_MAX_SIZE || (size & 3))
        return FW_ERR_SIZE;

    size_t n = fread(page, 1, PAGE_SIZE, fp);
    int err = firmware_check_header(page, (uint32_t)size);
    if (err || n < VECTOR_WORDS * 4)
        return err ? err : FW_ERR_READ;

    uint32_t rounded = ((uint32_t)size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    FlashIAP flash;
    if (flash.init() != 0)
        return FW_ERR_FLASH;
    // erase whole sectors covering the staging range
    uint32_t addr = STAGING_ADDR;
    uint32_t end = STAGING_ADDR + rounded;
    while (addr < end) {
        uint32_t sector = flash.get_sector_size(addr);
        if (flash.erase(addr, sector) != 0) {
            flash.deinit();
            return FW_ERR_FLASH;
        }
        addr += sector;
    }

    rewind(fp);
    for (uint32_t off = 0; off < rounded; off += PAGE_SIZE) {
        memset(page, 0xFF, PAGE_SIZE);
        size_t want = ((uint32_t)size - off) < PAGE_SIZE ? ((uint32_t)size - off) : PAGE_SIZE;
        if (fread(page, 1, want, fp) != want) {
            flash.deinit();
            return FW_ERR_READ;
        }
        if (flash.program(page, STAGING_ADDR + off, PAGE_SIZE) != 0) {
            flash.deinit();
            return FW_ERR_FLASH;
        }
    }
    flash.deinit();

    // verify the staged copy against the file
    rewind(fp);
    for (uint32_t off = 0; off < (uint32_t)size; off += PAGE_SIZE) {
        size_t want = ((uint32_t)size - off) < PAGE_SIZE ? ((uint32_t)size - off) : PAGE_SIZE;
        if (fread(page, 1, want, fp) != want)
            return FW_ERR_READ;
        if (memcmp(page, (const void *)(STAGING_ADDR + off), want) != 0)
            return FW_ERR_VERIFY;
    }
    staged_size = rounded;
    return FW_OK;
}

// ---- RAM resident part: nothing below may call into flash ------------------------------

#define RAMFUNC __attribute__((section(".data.laos_ramfunc"), noinline, optimize("O1,no-tree-loop-distribute-patterns")))

typedef void (*iap_fn_t)(uint32_t *cmd, uint32_t *res);

RAMFUNC static uint32_t sector_of(uint32_t addr)
{
    uint32_t n = addr >> 12;                    // 4 kB sectors 0..15
    return n >= 16 ? 14 + (n >> 3) : n;         // 32 kB sectors 16..29
}

RAMFUNC static uint32_t sector_start(uint32_t sector)
{
    return sector < 16 ? sector << 12 : 0x10000 + ((sector - 16) << 15);
}

RAMFUNC static uint32_t sector_end(uint32_t sector)
{
    return sector < 16 ? sector_start(sector) + 0x1000 : sector_start(sector) + 0x8000;
}

RAMFUNC static uint32_t iap(uint32_t cmd, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)
{
    volatile uint32_t in[5] = {cmd, p0, p1, p2, p3};
    volatile uint32_t out[5];
    ((iap_fn_t)IAP_ENTRY)((uint32_t *)in, (uint32_t *)out);
    return out[0];
}

RAMFUNC static void ram_apply(uint32_t size, uint32_t cclk_khz)
{
    __asm volatile("cpsid i" ::: "memory");
    volatile uint32_t *src;
    volatile uint32_t *dst;

    uint32_t addr = 0;
    while (addr < size) {
        uint32_t sector = sector_of(addr);
        uint32_t send = sector_end(sector);
        iap(50, sector, sector, 0, 0);                  // prepare for erase
        iap(52, sector, sector, cclk_khz, 0);           // erase
        while (addr < send && addr < size) {
            src = (volatile uint32_t *)(STAGING_ADDR + addr);
            dst = (volatile uint32_t *)ram_page;
            for (uint32_t i = 0; i < PAGE_SIZE / 4; i++)
                dst[i] = src[i];
            iap(50, sector, sector, 0, 0);              // prepare for write
            iap(51, addr, (uint32_t)ram_page, PAGE_SIZE, cclk_khz);   // copy RAM to flash
            addr += PAGE_SIZE;
        }
    }
    // reset, whether or not the copy succeeded: nothing better to do from here
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004;      // SCB->AIRCR: SYSRESETREQ
    for (;;) {
    }
}

void firmware_apply(void)
{
    // Mbed marks RAM as execute-never through the MPU; switch it off (MPU->CTRL) to run from RAM
    __disable_irq();
    *(volatile uint32_t *)0xE000ED94 = 0;
    __DSB();
    __ISB();
    // call through a volatile pointer: the target lives in RAM, out of range of a direct branch
    void (*volatile fn)(uint32_t, uint32_t) = ram_apply;
    fn(staged_size, SystemCoreClock / 1000);
    for (;;) {
    }
}
