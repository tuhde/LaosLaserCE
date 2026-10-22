/*
 * EthSetup.cpp
 * Setup ethernet interface, based on config file
 * Read IP, Gateway, DNS and port for server.
 * if IP is not defined, or dhcp is set, use dhcp for Ethernet
 *
 * Copyright (c) 2011 Peter Brier
 *
 *   This file is part of the LaOS project (see: http://wiki.protospace.nl/index.php/LaOS)
 *
 *   LaOS is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   LaOS is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FpITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with LaOS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "EthConfig.h"

// Ethernet connection status inputs (from phy) and outputs (to leds)
#define ETH_LINK (1<<25)
#define ETH_SPEED (1<<26)
const char ESC = 0x1b; 
PortIn eth_conn(Port1, ETH_LINK | ETH_SPEED);   // p25: link, 0=connected, 1=NC, p26: speed, 0=100mbit, 1=10mbit


/**
*** Return Speed status, true==100mbps
**/
bool EthSpeed(void)
{
  int s = eth_conn.read();
  return !(s & ETH_SPEED) && !(s & ETH_LINK);
}

/**
*** Return Link status, true==connected
**/
bool EthLink(void)
{
  int s = eth_conn.read();
  return !(s & ETH_LINK);
}

/**
*** EthConfig
**/
EthernetInterface * EthConfig()
 {
    extern GlobalConfig *cfg;
    EthernetInterface *eth = new EthernetInterface();
    eth->set_blocking(false);
    if ( cfg->dhcp )
    {
        printf("DHCP...\n\r");
        eth->set_dhcp(true);
    }
    else
    {
        printf("FIXED IP...\n\r");
        printf("IP: %s\n\r", cfg->ip);
        SocketAddress ip, nm, gw, dns;
        ip.set_ip_address(cfg->ip);
        nm.set_ip_address(cfg->nm);
        gw.set_ip_address(cfg->gw);
        dns.set_ip_address(cfg->dns);
        eth->set_dhcp(false);
        eth->set_network(ip, nm, gw);
        eth->add_dns_server(dns, NULL);
    }
    eth->connect();

    // wait for link and address (DHCP can take a while)
    Timer t;
    t.start();
    while (eth->get_connection_status() != NSAPI_STATUS_GLOBAL_UP && t.elapsed_time() < 15s)
        ThisThread::sleep_for(100ms);

    SocketAddress addr;
    if (eth->get_ip_address(&addr) == NSAPI_ERROR_OK && addr.get_ip_address()) {
        // tell global about current IP
        strncpy(cfg->ip, addr.get_ip_address(), sizeof(cfg->ip) - 1);
        cfg->ip[sizeof(cfg->ip) - 1] = 0;
        if (eth->get_netmask(&addr) == NSAPI_ERROR_OK && addr.get_ip_address()) {
            strncpy(cfg->nm, addr.get_ip_address(), sizeof(cfg->nm) - 1);
            cfg->nm[sizeof(cfg->nm) - 1] = 0;
        }
        if (eth->get_gateway(&addr) == NSAPI_ERROR_OK && addr.get_ip_address()) {
            strncpy(cfg->gw, addr.get_ip_address(), sizeof(cfg->gw) - 1);
            cfg->gw[sizeof(cfg->gw) - 1] = 0;
        }
        printf("IP Address is: %c[8;34;2m%s%c[0m  \n\r", ESC, cfg->ip, ESC);
    } else {
        printf("No IP address yet (no cable / no DHCP answer)\n\r");
    }
    return eth;
}
