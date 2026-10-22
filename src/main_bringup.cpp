/*
 * Temporary bring-up main for the Mbed CE port (steps 2-3).
 * Replaced by main.cpp once the network (TFTP) is ported.
 */
#include "mbed.h"
#include "global.h"
#include "pins.h"
#include "LaosMotion.h"
#include "LaosMenu.h"
#include "laosfilesystem.h"

LocalFileSystem local("local");
LaosFileSystem sd(p11, p12, p13, p14, "sd");
GlobalConfig *cfg;
LaosDisplay *dsp;
LaosMenu *mnu;
LaosMotion *mot;

static void sd_selftest()
{
  int err = sd.mount();
  if (err) {
    printf("SD mount failed (%d), formatting...\r\n", err);
    err = sd.reformat();
    if (err) {
      printf("SD format failed (%d)\r\n", err);
      return;
    }
  }
  char name[] = "test file.txt";   // long name, goes through the longname.sys table
  FILE *fp = sd.openfile(name, "wb");
  if (!fp) { printf("SD: create failed\r\n"); return; }
  fputs("hello\n", fp);
  fclose(fp);
  fp = sd.openfile(name, "rb");
  char buf[16] = "";
  if (fp) { fgets(buf, sizeof(buf), fp); fclose(fp); }
  printf("SD: read back '%s'\r\n", buf);
  printdir();
  removefile(name);
  printdir();
  printf("SD: selftest %s\r\n", strncmp(buf, "hello", 5) ? "FAILED" : "OK");
}

int main()
{
  printf(VERSION_STRING "\r\nBRING-UP\r\n");
  sd_selftest();
  cfg = new GlobalConfig("config.txt");
  printf("CONFIG OK: xscale=%d yscale=%d nodisplay=%d\r\n", cfg->xscale, cfg->yscale, cfg->nodisplay);
  mot = new LaosMotion();
  dsp = new LaosDisplay();
  mnu = new LaosMenu(dsp);
  printf("MENU OK\r\n");
  mnu->SetScreen(1);
  while (true) {
    mnu->Handle();
  }
}
