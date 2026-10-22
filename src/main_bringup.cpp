/*
 * Temporary bring-up main for the Mbed CE port (step 2).
 * Replaced by main.cpp once menu, SD and network are ported.
 */
#include "mbed.h"
#include "global.h"
#include "pins.h"
#include "LaosMotion.h"

LocalFileSystem local("local");
GlobalConfig *cfg;

int main()
{
  printf(VERSION_STRING "\r\nBRING-UP\r\n");
  cfg = new GlobalConfig("config.txt");
  printf("CONFIG OK: xscale=%d yscale=%d nodisplay=%d\r\n", cfg->xscale, cfg->yscale, cfg->nodisplay);
  new LaosMotion();
  printf("MOTION OK\r\n");
  while (true) {
    led1 = !led1;
    ThisThread::sleep_for(500ms);
  }
}
