/*
 * main.cpp
 * Laos Controller, main function
 *
 * Copyright (c) 2011 Peter Brier & Jaap Vermaas
 *
 *   This file is part of the LaOS project (see: http://wiki.laoslaser.org)
 *
 *   LaOS is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   LaOS is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with LaOS.  If not, see <http://www.gnu.org/licenses/>.
 *
 * This program consists of a few parts:
 *
 * ConfigFile   Read configuration files
 * EthConfig    Initialize an ethernet stack, based on a configuration file (includes link status monitoring)
 * LaosDisplay  User interface functions (read keys, display text and menus on LCD)
 * LaosMenu     User interface stuctures (menu navigation)
 * LaosServer   TCP/IP server, accept connections read/write data
 * LaosMotion   Motion control functions (X,Y,Z motion, homing)
 * LaosIO       Input/Output functions
 * LaosFile     File based jobs (read/write/delete)
 *
 * Program functions:
 * 1) Read config file
 * 2) enable TCP/IP stack (Fixed ip or DHCP)
 * 3) Instantiate tcp/ip port and accept connections
 * 4) Show menus, peform user actions
 * 5) Controll laser
 * 6) Controll motion
 * 7) Set and read IO, check status (e.g. interlocks)
 *
 */

// uncomment this to get debugging output in file parser
// #define READ_FILE_DEBUG
// #define READ_FILE_DEBUG_VERBOSE
 
#include "pins.h"
#include "global.h"
#include "ConfigFile.h"
#include "EthConfig.h"
#include "TFTPServer.h"
#include "LaosMenu.h"
#include "LaosMotion.h"
#include "stepper.h"
#include "laosfilesystem.h"

// Status and communication
EthernetInterface *eth; // Ethernet, tcp/ip

// Filesystems
LaosFileSystem sd(p11, p12, p13, p14, "sd");

// Laos objects
LaosDisplay *dsp;
LaosMenu *mnu;
TFTPServer *srv;
LaosMotion *mot;
Timer systime;

// Config
GlobalConfig *cfg;

// print heap usage: current/peak use and total heap size (needs platform.heap-stats-enabled)
// size of the largest block malloc() can currently deliver (binary search)
static size_t largest_free_block(size_t limit)
{
  size_t lo = 0, hi = limit;
  while (hi - lo > 16) {
    size_t mid = (lo + hi) / 2;
    void *p = malloc(mid);
    if (p) { free(p); lo = mid; } else hi = mid;
  }
  return lo;
}

static void print_mem(const char *where)
{
  mbed_stats_heap_t h;
  mbed_stats_heap_get(&h);
  size_t largest = largest_free_block(h.reserved_size);
  mbed_stats_heap_get(&h);    // again: the probing above counts as allocation failures
  printf("MEM %s: heap used %lu, peak %lu, total %lu, free %lu, alloc failures %lu, largest block %lu\n\r", where,
         (unsigned long)h.current_size, (unsigned long)h.max_size, (unsigned long)h.reserved_size,
         (unsigned long)(h.reserved_size - h.current_size), (unsigned long)h.alloc_fail_cnt, (unsigned long)largest);
}

static void print_threads()
{
  mbed_stats_thread_t t[12];
  int n = mbed_stats_thread_get_each(t, 12);
  for (int i = 0; i < n; i++)
    printf("THREAD %-12s stack size %5lu, max used %5lu\n\r", t[i].name ? t[i].name : "?",
           (unsigned long)t[i].stack_size, (unsigned long)(t[i].stack_size - t[i].stack_space));
}

// Protos
void main_nodisplay();
void main_menu();

// for debugging:
extern void plan_get_current_position_xyz(float *x, float *y, float *z);
extern PwmOut pwm;

/**
*** Main function
**/
int main() 
{
  systime.start();
  //float x, y, z;
  eth_speed = 1;
  
  dsp = new LaosDisplay();
  printf( VERSION_STRING "...\nBOOT...\n" ); 
  mnu = new LaosMenu(dsp);
  eth_speed=0;

 printf("TEST SD...\n"); 
  char testfile[] = "test.txt";
  FILE *fp = NULL;
  if (sd.mount() == 0)
    fp = sd.openfile(testfile, "wb");
  if ( fp == NULL )
  {
    mnu->SetScreen("SD NOT READY!"); 
    ThisThread::sleep_for(2s);
    NVIC_SystemReset();
  }
  else
  {
    printf("SD: READY...\n");
    fclose(fp);
    removefile(testfile);
  }
  
  // See if there's a .bin file on the SD
  // if so, put it on the MBED and reboot
  if (SDcheckFirmware()) NVIC_SystemReset();
  
  mnu->SetScreen(VERSION_STRING);
  printf("START...\n");
  cfg =  new GlobalConfig("config.txt");
  mnu->SetScreen("CONFIG OK...."); 
  printf("CONFIG OK...\n");
  if (!cfg->nodisplay)
    dsp->testI2C();
  
  printf("MOTION...\n"); 
  mot = new LaosMotion();
    
  eth = EthConfig();
  eth_speed=1;
      
  printf("SERVER...\n");
  srv = new TFTPServer(eth, cfg->port);
  mnu->SetScreen("SERVER OK...."); 
  ThisThread::sleep_for(500ms);
  mnu->SetScreen(10); // IP
  ThisThread::sleep_for(1s);
  
  printf("RUN...\n");
  print_mem("boot");
  print_threads();
  
  // Wait for key, and then home
  
  if ( cfg->autohome )
  {
    printf("WAIT FOR COVER...\n");
    ThisThread::sleep_for(1s);
  
  
  // Start homing
    mnu->SetScreen("WAIT FOR COVER....");
    //if ( cfg->waitforstart ) 
      while ( !mot->isStart() ) ThisThread::sleep_for(10ms);
    mnu->SetScreen("HOME....");
    printf("HOME...\n");

    mot->home(cfg->xhome,cfg->yhome, cfg->zhome);
    // if ( !mot->isHome ) exit(1);
    printf("HOME DONE. (%d,%d, %d)\n",cfg->xhome,cfg->yhome,cfg->zhome);
  }
  else
    printf("Homing skipped: %d\n", cfg->autohome);

  // clean sd card?
  if (cfg->cleandir) cleandir();
  mnu->SetScreen("");  

  if (cfg->nodisplay) {
    printf("No display set\n\r");
    main_nodisplay();
  } else {
    printf("Entering display\n\r");
    main_menu();
  }
}

void main_nodisplay() {
  float x, y, z = 0;
  led1=led2=led3=led4=0;
  
  // main loop  
   while(1) 
  {  
    int filecnt = srv->fileCnt();
    mnu->SetScreen("Wait for file ...");
    while (srv->State() == listen)
        srv->poll();
    if (srv->State() != listen) {
      mnu->SetScreen("Receive file");
      while ((! mnu->Cancel()) && (srv->State() != listen)) srv->poll();
    }
    if (filecnt < srv->fileCnt()) {
      mot->reset();
      plan_get_current_position_xyz(&x, &y, &z);
       printf("%f %f\n", x,y); 
       mnu->SetScreen("Laser BUSY..."); 
    
       char name[32];
       srv->getFilename(name);
       printf("Now processing file: '%s'\n\r", name);
       FILE *in = sd.openfile(name, "r");
       if (in == NULL) {
         printf("Cannot open job file '%s' (errno %d)\n\r", name, errno);
         print_mem("open failed");
         continue;
       }
       while (!feof(in))
       { 
         mot->waitReady();
         mot->write(readint(in));
       }
       fclose(in);
       removefile(name);
       // done
       printf("DONE!...\n");
       print_mem("after job");
       {
         uint32_t calls, avg, max;
         st_get_stats(&calls, &avg, &max);
         printf("STEP ISR: %lu calls, avg %lu cycles, max %lu cycles\n\r",
                (unsigned long)calls, (unsigned long)avg, (unsigned long)max);
       }
	   mot->waitReady();
       mot->moveToAbsolute(cfg->xrest, cfg->yrest, cfg->zrest);
    }
  }
}

void main_menu() {
  // main loop  
  led1=led2=led3=led4=0;
                
  mnu->SetScreen(1);
  while (1) {
    int filecnt = srv->fileCnt();
    mnu->Handle();
    srv->poll();
    if (srv->State() != listen) {
      mnu->SetScreen("Receive file");
	  while ((! mnu->Cancel()) && (srv->State() != listen)) srv->poll();
    }
    if (filecnt < srv->fileCnt()) {
      char myname[32];
      srv->getFilename(myname);
      if (isFirmware(myname)) {
        installFirmware(myname);
        mnu->SetScreen(1);
      } else {
        if (strcmp("config.txt", myname) == 0) {
          // it's a config file!
          mnu->SetScreen(1);
        } else {
          if (isLaosFile(myname)) {
            mnu->SetFileName(myname);
            mnu->SetScreen(2);
          }
        }
      }
    }           
  }
}
