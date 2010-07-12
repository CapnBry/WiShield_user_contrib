/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack
 *
 * @(#)$Id: dhcpc.c,v 1.2 2006/06/11 21:46:37 adam Exp $
 */

#include "uip.h"

#ifdef UIP_DHCP

#define STATE_INITIAL         0
#define STATE_SENDING         1
#define STATE_OFFER_RECEIVED  2
#define STATE_CONFIG_RECEIVED 3

static struct dhcp_state s;

// DHCP LIGHT appears to be broken
// #define UIP_CONF_DHCP_LIGHT 1

struct dhcp_msg {
  u8_t op, htype, hlen, hops;
  u8_t xid[4];
  u16_t secs, flags;
  u8_t ciaddr[4];
  u8_t yiaddr[4];
  u8_t siaddr[4];
  u8_t giaddr[4];
  u8_t chaddr[16];
#ifndef UIP_CONF_DHCP_LIGHT
  u8_t sname[64];
  u8_t file[128];
#endif // !UIP_CONF_DHCP_LIGHT
  u8_t options[312];
};


#define BOOTP_BROADCAST 0x8000

#define DHCP_REQUEST        1
#define DHCP_REPLY          2
#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6
#define DHCP_MSG_LEN      236

#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68

#define DHCPDISCOVER  1
#define DHCPOFFER     2
#define DHCPREQUEST   3
#define DHCPDECLINE   4
#define DHCPACK       5
#define DHCPNAK       6
#define DHCPRELEASE   7

#define DHCP_OPTION_SUBNET_MASK   1
#define DHCP_OPTION_ROUTER        3
#define DHCP_OPTION_DNS_SERVER    6
#define DHCP_OPTION_REQ_IPADDR   50
#define DHCP_OPTION_LEASE_TIME   51
#define DHCP_OPTION_MSG_TYPE     53
#define DHCP_OPTION_SERVER_ID    54
#define DHCP_OPTION_REQ_LIST     55
#define DHCP_OPTION_END         255

static const u8_t xid[4] = {0xad, 0xde, 0x12, 0x23};
static const u8_t magic_cookie[4] = {99, 130, 83, 99};
/*---------------------------------------------------------------------------*/
//static u8_t *
u8_t* add_msg_type(u8_t *optptr, u8_t type)
{
  *optptr++ = DHCP_OPTION_MSG_TYPE;
  *optptr++ = 1;
  *optptr++ = type;
  return optptr;
}
/*---------------------------------------------------------------------------*/
//static u8_t *
u8_t* add_server_id(u8_t *optptr)
{
  *optptr++ = DHCP_OPTION_SERVER_ID;
  *optptr++ = 4;
  memcpy(optptr, s.serverid, 4);
  return optptr + 4;
}
/*---------------------------------------------------------------------------*/
//static u8_t *
u8_t* add_req_ipaddr(u8_t *optptr)
{
  *optptr++ = DHCP_OPTION_REQ_IPADDR;
  *optptr++ = 4;
  memcpy(optptr, s.ipaddr, 4);
  return optptr + 4;
}
/*---------------------------------------------------------------------------*/
//static u8_t *
u8_t* add_req_options(u8_t *optptr)
{
  *optptr++ = DHCP_OPTION_REQ_LIST;
  *optptr++ = 3;
  *optptr++ = DHCP_OPTION_SUBNET_MASK;
  *optptr++ = DHCP_OPTION_ROUTER;
  *optptr++ = DHCP_OPTION_DNS_SERVER;
  return optptr;
}
/*---------------------------------------------------------------------------*/
//static u8_t *
u8_t* add_end(u8_t *optptr)
{
  *optptr++ = DHCP_OPTION_END;
  return optptr;
}
/*---------------------------------------------------------------------------*/
//static void
void create_msg(register struct dhcp_msg *m)
{
  m->op = DHCP_REQUEST;
  m->htype = DHCP_HTYPE_ETHERNET;
  m->hlen = s.mac_len;
  m->hops = 0;
  memcpy(m->xid, xid, sizeof(m->xid));
  m->secs = 0;
  m->flags = HTONS(BOOTP_BROADCAST); /*  Broadcast bit. */
  /*  uip_ipaddr_copy(m->ciaddr, uip_hostaddr);*/
  memcpy(m->ciaddr, uip_hostaddr, sizeof(m->ciaddr));
  memset(m->yiaddr, 0, sizeof(m->yiaddr));
  memset(m->siaddr, 0, sizeof(m->siaddr));
  memset(m->giaddr, 0, sizeof(m->giaddr));
  memcpy(m->chaddr, s.mac_addr, s.mac_len);
  memset(&m->chaddr[s.mac_len], 0, sizeof(m->chaddr) - s.mac_len);
#ifndef UIP_CONF_DHCP_LIGHT
  memset(m->sname, 0, sizeof(m->sname));
  memset(m->file, 0, sizeof(m->file));
#endif // !UIP_CONF_DHCP_LIGHT

  memcpy(m->options, magic_cookie, sizeof(magic_cookie));
}
/*---------------------------------------------------------------------------*/
static void
send_discover(void)
{
  u8_t *end;
  struct dhcp_msg *m = (struct dhcp_msg *)uip_appdata;

  create_msg(m);

  end = add_msg_type(&m->options[4], DHCPDISCOVER);
  end = add_req_options(end);
  end = add_end(end);

  uip_send(uip_appdata, end - (u8_t *)uip_appdata);
}
/*---------------------------------------------------------------------------*/
static void
send_request(void)
{
  u8_t *end;
  struct dhcp_msg *m = (struct dhcp_msg *)uip_appdata;

  create_msg(m);
  
  end = add_msg_type(&m->options[4], DHCPREQUEST);
  end = add_server_id(end);
  end = add_req_ipaddr(end);
  end = add_end(end);
  
  uip_send(uip_appdata, end - (u8_t *)uip_appdata);
}
/*---------------------------------------------------------------------------*/
static u8_t
parse_options(u8_t *optptr, int len)
{
  u8_t *end = optptr + len;
  u8_t type = 0;

  while(optptr < end) {
    switch(*optptr) {
    case DHCP_OPTION_SUBNET_MASK:
      //printx("parse_options DHCP_OPTION_SUBNET_MASK");
      memcpy(s.netmask, optptr + 2, 4);
      break;
    case DHCP_OPTION_ROUTER:
      //printx("parse_options DHCP_OPTION_ROUTER");
      memcpy(s.default_router, optptr + 2, 4);
      break;
    case DHCP_OPTION_DNS_SERVER:
      //printx("parse_options DHCP_OPTION_DNS_SERVER");
      memcpy(s.dnsaddr, optptr + 2, 4);
      break;
    case DHCP_OPTION_MSG_TYPE:
      //printx("parse_options DHCP_OPTION_MSG_TYPE");
      type = *(optptr + 2);
      break;
    case DHCP_OPTION_SERVER_ID:
      //printx("parse_options DHCP_OPTION_SERVER_ID");
      memcpy(s.serverid, optptr + 2, 4);
      break;
    case DHCP_OPTION_LEASE_TIME:
      //printx("parse_options DHCP_OPTION_LEASE_TIME");
      memcpy(s.lease_time, optptr + 2, 4);
      break;
    case DHCP_OPTION_END:
      //printx("parse_options DHCP_OPTION_END");
      return type;
    }

    optptr += optptr[1] + 2;
  }
  
  //printx("parse_options end return");
  return type;
}
/*---------------------------------------------------------------------------*/
static u8_t
parse_msg(void)
{
  struct dhcp_msg *m = (struct dhcp_msg *)uip_appdata;
  
  if(m->op == DHCP_REPLY) {
    //printx("parse_msg YES 1");
    if(memcmp(m->xid, xid, sizeof(xid)) == 0) {
      //printx("parse_msg YES 2");
      if(memcmp(m->chaddr, s.mac_addr, s.mac_len) == 0) {
        //printx("parse_msg YES 3");
        memcpy(s.ipaddr, m->yiaddr, 4);
        return parse_options(&m->options[4], uip_datalen());
      }
    }
  }

  if(m->op == DHCP_REPLY &&
     memcmp(m->xid, xid, sizeof(xid)) == 0 &&
     memcmp(m->chaddr, s.mac_addr, s.mac_len) == 0) {
    //printx("parse_msg YES");
    memcpy(s.ipaddr, m->yiaddr, 4);
    return parse_options(&m->options[4], uip_datalen());
  }

  char buf[32];
  sprintf(buf, "parse_msg NO: %d", m->op);
  //printx(buf);
  return 0;
}
/*---------------------------------------------------------------------------*/
PT_THREAD(uip_dhcp_run(void))
{
  PT_BEGIN(&s.pt);
  
  /* try_again:*/
  s.state = STATE_SENDING;
  s.ticks = CLOCK_SECOND;

  do {
    send_discover();
    timer_set(&s.timer, s.ticks);
    PT_YIELD_UNTIL(&s.pt, uip_newdata() || timer_expired(&s.timer));

    //printx("after wait");

    if(uip_newdata() && parse_msg() == DHCPOFFER) {
      //printx("STATE_OFFER_RECEIVED");
      s.state = STATE_OFFER_RECEIVED;
      break;
    }

    if(s.ticks < CLOCK_SECOND * 10) {
      s.ticks *= 2;
      //printx("boost clock");
    }
  } while(s.state != STATE_OFFER_RECEIVED);
  
  s.ticks = CLOCK_SECOND;

  do {
    send_request();
    timer_set(&s.timer, s.ticks);
    PT_YIELD_UNTIL(&s.pt, uip_newdata() || timer_expired(&s.timer));

    //printx("after wait");

    if(uip_newdata() && parse_msg() == DHCPACK) {
      //printx("STATE_CONFIG_RECEIVED");
      s.state = STATE_CONFIG_RECEIVED;
      break;
    }

    if(s.ticks < CLOCK_SECOND * 10) {
      s.ticks *= 2;
      //printx("boost clock");
    }
  } while(s.state != STATE_CONFIG_RECEIVED);
  
  //printx("broke free");

#if 0
  printf("Got IP address %d.%d.%d.%d\n",
	 uip_ipaddr1(s.ipaddr), uip_ipaddr2(s.ipaddr),
	 uip_ipaddr3(s.ipaddr), uip_ipaddr4(s.ipaddr));
  printf("Got netmask %d.%d.%d.%d\n",
	 uip_ipaddr1(s.netmask), uip_ipaddr2(s.netmask),
	 uip_ipaddr3(s.netmask), uip_ipaddr4(s.netmask));
  printf("Got DNS server %d.%d.%d.%d\n",
	 uip_ipaddr1(s.dnsaddr), uip_ipaddr2(s.dnsaddr),
	 uip_ipaddr3(s.dnsaddr), uip_ipaddr4(s.dnsaddr));
  printf("Got default router %d.%d.%d.%d\n",
	 uip_ipaddr1(s.default_router), uip_ipaddr2(s.default_router),
	 uip_ipaddr3(s.default_router), uip_ipaddr4(s.default_router));
  printf("Lease expires in %ld seconds\n",
	 ntohs(s.lease_time[0])*65536ul + ntohs(s.lease_time[1]));
#endif
/*
  printx("IP obtained:");
  printx(s.ipaddr));
  printx(uip_ipaddr2(s.ipaddr));
  printx(uip_ipaddr3(s.ipaddr));
  printx(uip_ipaddr4(s.ipaddr));

  printx("Netmask obtained:");
  printx(uip_ipaddr1(s.netmask));
  printx(uip_ipaddr2(s.netmask));
  printx(uip_ipaddr3(s.netmask));
  printx(uip_ipaddr4(s.netmask));

  printx("DNS obtained:");
  printx(uip_ipaddr1(s.dnsaddr));
  printx(uip_ipaddr2(s.dnsaddr));
  printx(uip_ipaddr3(s.dnsaddr));
  printx(uip_ipaddr4(s.dnsaddr));

  printx("Default router obtained:");
  printx(uip_ipaddr1(s.default_router));
  printx(uip_ipaddr2(s.default_router));
  printx(uip_ipaddr3(s.default_router));
  printx(uip_ipaddr4(s.default_router));

  printx("Lease expiration:");
  printx(ntohs(s.lease_time[0])*65536ul + ntohs(s.lease_time[1]));*/
  uip_dhcp_callback(&s);
  
  /*  timer_stop(&s.timer);*/

  /*
   * PT_END restarts the thread so we do this instead. Eventually we
   * should reacquire expired leases here.
   */
  while(1) {
    PT_YIELD(&s.pt);
  }

  PT_END(&s.pt);
}
/*---------------------------------------------------------------------------*/
void
uip_dhcp_init(const void *mac_addr, int mac_len)
{
  uip_ipaddr_t addr;
  
  s.mac_addr = mac_addr;
  s.mac_len  = mac_len;

  s.state = STATE_INITIAL;
  uip_ipaddr(addr, 255,255,255,255);
  s.conn = uip_udp_new(&addr, HTONS(DHCP_SERVER_PORT));
  if(s.conn != NULL) {
    uip_udp_bind(s.conn, HTONS(DHCP_CLIENT_PORT));
  }
  PT_INIT(&s.pt);
}
/*---------------------------------------------------------------------------*/
void
uip_dhcp_request(void)
{
  u16_t ipaddr[2];
  
  if(s.state == STATE_INITIAL) {
    uip_ipaddr(ipaddr, 0,0,0,0);
    uip_sethostaddr(ipaddr);
    /*    dhcp_handle_dhcp(PROCESS_EVENT_NONE, NULL);*/
  }
}
/*---------------------------------------------------------------------------*/

#endif //UIP_DHCP