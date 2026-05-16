/* net_ps4.c -- sceNet/sceNetCtl init; BSD socket code lives in qcommon/net_ip.c. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/Sysmodule.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#define NET_HEAP_SIZE (256 * 1024)  /* sceNet socket send buffers */

static int s_netMemId = -1;
static void *s_netMemory = NULL;

static uint32_t s_localIP        = 0;            /* host byte order */
static uint32_t s_subnetMask     = 0xFFFFFF00;
static uint32_t s_subnetBcast    = 0;            /* network byte order */

/* Must run before any socket calls. */
void PS4_NetInit(void)
{
	int ret;

	Com_Printf("PS4 Net: Initializing...\n");

	ret = sceNetInit();
	if (ret < 0 && ret != (int)0x80410004) { /* ALREADY_INIT */
		Com_Printf("WARNING: sceNetInit failed: 0x%08X\n", ret);
		return;
	}

	ret = sceNetPoolCreate("ioq3", NET_HEAP_SIZE, 0);
	if (ret < 0) {
		Com_Printf("WARNING: sceNetPoolCreate failed: 0x%08X\n", ret);
		s_netMemId = -1;
	} else {
		s_netMemId = ret;
	}

	ret = sceNetCtlInit();
	if (ret < 0 && ret != (int)0x80412102) { /* ALREADY_INIT */
		Com_Printf("WARNING: sceNetCtlInit failed: 0x%08X\n", ret);
	}

	{
		OrbisNetCtlInfo info;
		struct in_addr addr;
		memset(&info, 0, sizeof(info));
		if (sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info) == 0 &&
		    inet_pton(AF_INET, info.ip_address, &addr) == 1) {
			s_localIP = ntohl(addr.s_addr);
			Com_Printf("PS4 Net: local IP = %s\n", info.ip_address);
		}
		memset(&info, 0, sizeof(info));
		if (sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_NETMASK, &info) == 0 &&
		    inet_pton(AF_INET, info.netmask, &addr) == 1) {
			s_subnetMask = ntohl(addr.s_addr);
			Com_Printf("PS4 Net: netmask  = %s\n", info.netmask);
		}
		if (s_localIP) {
			uint32_t bcast = (s_localIP & s_subnetMask) | (~s_subnetMask);
			s_subnetBcast = htonl(bcast);
			struct in_addr ba; ba.s_addr = s_subnetBcast;
			Com_Printf("PS4 Net: broadcast = %s\n", inet_ntoa(ba));
		}
	}

	Com_Printf("PS4 Net: Initialized\n");
}

void PS4_NetShutdown(void)
{
	sceNetCtlTerm();
	sceNetTerm();
}

/* Accessors for net_ip.c. */
uint32_t PS4_GetLocalIP(void)       { return s_localIP; }
uint32_t PS4_GetSubnetMask(void)    { return s_subnetMask; }
uint32_t PS4_GetSubnetBroadcast(void) { return s_subnetBcast; }
int PS4_GetNetPoolId(void)          { return s_netMemId; }
