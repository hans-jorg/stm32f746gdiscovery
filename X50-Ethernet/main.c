
/**
 * @file     main.c
 * @brief    Blink LEDs using counting delays and CMSIS (Heavy use of macros)
 * @version  V1.0
 * @date     06/10/2020
 *
 * @note     The blinking frequency depends on core frequency
 * @note     Direct access to registers
 * @note     No library used
 *
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "stm32f746xx.h"
#include "system_stm32f746.h"
#include "sdram.h"
#include "led.h"
#include "eth.h"


#include "lwip/init.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"
//#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"
#include "lwip/timeouts.h"
#include "arch/cc.h"
#include "arch/sys_arch.h"
#include "lwip/tcp.h"
#include "lwip/prot/ethernet.h"
#include "lwip/apps/tftp_server.h"
#include "stnetif.h"

#include "debugdump.h"
#include "debugmessages.h"


/**
 * @brief   verbose flag
 *
 * @note    Used in ethernetif
 */
#ifdef VERBOSE
int verbose = VERBOSE;
#endif

/** 
 * @brief Configuration
 */
///@{
//#define USE_HTTPD               1
#define USE_TFTP                  1
///@}


#if USE_HTTPD
/**
 * @brief IP_PORT for HTTP
 *
 * @note  Port number where the server listens
 */
#define IP_PORT              8080
#endif

//////////////////// Network configuration /////////////////////////////////////

static ip4_addr_t       ipaddr  = {0};
static ip4_addr_t       netmask = {0};
static ip4_addr_t       gateway = {0};

#ifndef LWIP_DHCP
// IP, Netmask, Gateway should be here but the lwip macros do not allow it
#endif


#define IFNAME0         'e'
#define IFNAME1         't'


#define HOSTNAME        "lwipt"

////////////////// Timing Functions  ///////////////////////////////////////////

static volatile uint32_t tick_ms = 0;
static volatile uint32_t delay_ms = 0;
static int led_initialized = 0;

#define INTERVAL 500
void SysTick_Handler(void) {

    if( !led_initialized ) {
        LED_Init();
        led_initialized = 1;
    }
    if( tick_ms >= INTERVAL ) {
       LED_Toggle();
       tick_ms = 0;
    } else {
       tick_ms++;
    }

    if( delay_ms > 0 ) delay_ms--;

    sys_count();

}

/**
 *  @brief  Delay
 *
 * @note    Delays *delay* milliseconds
 */

void Delay(uint32_t delay) {

    delay_ms = delay;
    while( delay_ms ) {}

}


/**
 * @brief   STOP
 *
 * @note    called when an error occurs
 */
void STOP(int code) {
static int static_code;

    static_code = code;
    while (1) {}
}


////////////////// Auxiliary Functions  /////////////////////////////////////////

/**
 * @brief convertbyte to string
 */
char *
convertbyte(uint8_t b, char *p) {

    if( b >= 100 ) {
        *p++ = '0'+(b/100);
        b %= 100;
        *p++ = '0'+(b/10);
        b %= 10;
        *p++ = '0'+b;
    } else if ( b >= 10 ) {
        *p++ = '0'+(b/10);
        b %= 10;
        *p++ = '0'+b;
    } else {
        *p++ = '0'+b;
    }
    *p = 0;
    return p;

}

/**
 * @brief   int2str
 *
  * @brief  Convert an integer to a string, avoiding string overflow
 *
 * @note    When an overflow is detected, the field is filled with asterisks
 *
 * @note    len must account for the null terminator
 */
int
int2str(int n, char *s, int len) {
unsigned x;
unsigned p10,p10ant;
int e10 = 1;

    if( n < 0 )
        x = (unsigned) (-n);
    else
        x = n;

    p10 = p10ant = 1;
    e10 = 0;
    while( (p10 < x) && (p10ant <= p10) ) {
        p10ant = p10;
        p10 *= 10;
        e10++;
    }
    if( e10 > len ) {
        char *q = s+len-1;
        while( s < q ) *s++ = '*';
        *s = 0;
        return -1;
    }
    p10 = p10ant;
    while( p10 > 1 ) {
        int d = 0;
        while( x >= p10 ) {
            x -= p10;
            d++;
        }
        *s++ = (d+'0');
        p10 /= 10;
    }
    *s++ = x+'0';
    *s = 0;
    return 0;
}




///////////////////// TFTP Functions ///////////////////////////////////////////


#define ONLY_FILE  ((void *) 1)
static void*
tftp_open(const char* fname, const char* mode, u8_t is_write) {

    if (is_write) {
        return NULL;                    // not yet
    } else {
        return ONLY_FILE;
    }
}

static void
tftp_close(void* handle) {

    return;
}

static int counter = 0;

static int
tftp_read(void* handle, void* buf, int len) {
int rc;

    if( handle != ONLY_FILE )
        return -1;

    rc = int2str(counter,buf,len);
    counter++;
    return rc;
}

static int
tftp_write(void* handle, struct pbuf* p) {

    if( handle != ONLY_FILE )
        return -1;

    while (p != NULL) {
        hexdump(p->payload,p->len,0);
        p = p->next;
    }
    return 0;
}

static const struct tftp_context
tftp_config = {
  tftp_open,
  tftp_close,
  tftp_read,
  tftp_write
};

//////////////////////// LWIP Data /////////////////////////////////////////////////////////////////

/**
 * brief network interface configuration
 */
static struct netif     netif;

// Generate an 32-bit integer with network order of a network address or mask
#if BYTE_ORDER == LITTLE_ENDIAN
#define IPV4(A,B,C,D)   (((u32_t)(D)<<24)                   \
                        |((u32_t)(C)<<16)                   \
                        |((u32_t)(B)<<8)                    \
                        |((u32_t)(A)<<0))
#else
#define IPV4(A,B,C,D)   (((u32_t)(A)<<24)                   \
                        |((u32_t)(B)<<16)                   \
                        |((u32_t)(C)<<8)                    \
                        |((u32_t)(D)<<0))
#endif

///////////////////// Network Functions ////////////////////////////////////////

/**
 * @brief   Initialize lwIP
 * 
 * @note    Do all initialization for lwIP
 */

void Network_Init(void) {
err_t err;

    MESSAGE("Initialing lwip\n");
    lwip_init();

    MESSAGE("Initializing interface\n");

#if ! LWIP_DHCP
    IP4_ADDR(&ipaddr,192,168,0,190);
    IP4_ADDR(&netmask,255,255,255,0);
    IP4_ADDR(&gateway,192,168,0,1);
#endif
    netif_add(  &netif, 
                &ipaddr, 
                &netmask, 
                &gateway, 
                NULL, 
                stnetif_init, 
                ethernet_input
                );

    netif_set_default(&netif);
    netif_set_link_up(&netif);

    if( netif_is_link_up(&netif) ) {
        MESSAGE("Link is up\n");
        netif_set_up(&netif);
    } else {
        netif_set_down(&netif);
        MESSAGE("Link is down\n");
    }

    netif_set_status_callback(&netif, stnetif_status_callback);
    netif_set_link_callback(&netif, stnetif_link_callback);
    //netif_set_remove_callback(&netif,0);
    
    netif_set_link_up(&netif);
    netif_set_up(&netif);

#if LWIP_DHCP
    MESSAGE("Starting DHCP\n");
    // dchp_set_struct
    err = dhcp_start(&netif);
    Delay(100);
    if( err != ERR_OK ) MESSAGE("DHCP Error");
#endif

    MESSAGE("Ethernet interface up\n");
   // if( verbose ) {
   //     if( ip4_addr_isany_val(ipaddr) ) {
            char s[20];
            ipaddr_ntoa_r(&ipaddr,s,20);
            printf("IP Address = %s\n",s);
            ipaddr_ntoa_r(&netmask,s,20);
            printf("IP Network Mask = %s\n",s);
            ipaddr_ntoa_r(&gateway,s,20);
            printf("IP Gateway = %s\n",s);
    //    }
    // }


#if USE_TFTP 
    message("Starting TFTP server\n");
    tftp_init(&tftp_config);
#endif

#if USE_HTTPD
    // not tested yet!!! Not configured too.
    // It uses TCP!!!
    message("Starting HTTP server\n");
    httpd_init();
#endif

}

/**
 * @brief   LWIP processing in the main loop
 * 
 * @note    See lwip-2.1.2/doc/doxygen/output/html/group__lwip__nosys.html
 *          Needs a queue data structure
 */
void Network_Process(void) {

    // Print status (for debug)
    stnetif_printstatus();

    //LWIP_CheckLink();
    stnetif_link(&netif);

    stnetif_input(&netif);
            
    // Check timers
    sys_check_timeouts();

#if LWIP_NETIF_LOOPBACK
    netif_poll(&netif);
#endif

#if !LWIP_NETIF_LOOPBACK_MULTITHREADING
    // could call netif_poll twice for netif
    netif_poll_all();
#endif

    if( ip4_addr_isany_val(ipaddr) ) {
        char s[20];
        ipaddr_ntoa_r(&ipaddr,s,20);
        printf("IP Address = %s\n",s);
        #if 0
        ip2str(netmask.addr,s);
        printf("IP Network Mask = %s\n",s);
        ip2str(gateway.addr,s);
        printf("IP Gateway = %s\n",s);
        #endif
    }


}

///////////////////// Main Function ///////////////////////////////////////////////////////////////

/**
 * @brief   main
 *
 * @note    Initializes GPIO and SDRAM, blinks LED and test SDRAM access
 */
int main(void) {
err_t rc;

    // Disable buffering for stdout (Trying)
    //setvbuf(stdout, NULL,_IONBF, 0);

    message("Starting.at %ld KHz...\n",SystemCoreClock/1000);

    /* Set Clock to 200 MHz */
    SystemConfigMainPLL(&MainPLLConfiguration_200MHz);
    SystemSetCoreClock(CLOCKSRC_PLL,1);

    message("Now running at %ld KHz...\n",SystemCoreClock/1000);

    // Set SysTick to 1 ms
    SysTick_Config(SystemCoreClock/1000);

    printf("Starting SDRAM\n");
    SDRAM_Init();

    memset((uint8_t *) 0xC0000000, 0x12345678, 0x1000 );
    memcpy((uint8_t *) 0xC0000000, (uint8_t *) 0x20000000, 0x1000);

#if 1
    message("Initializing LWIP\n");
    Network_Init();
 #endif
    // Entering Main loop
    int cnt = 0;
    while(1) {

        Network_Process();

        // Application code here
        cnt++;
        if( cnt == 20 ) {
            char line[20];
            MESSAGE("PAUSE\n");
            fgets(line,20,stdin);
            cnt = 0;
        }
    }
}
