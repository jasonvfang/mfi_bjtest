#ifndef _MFI_ARP_H_
#define _MFI_ARP_H_

#define MFI_ARP_INTF_NAME "wlan0"
#define MFI_ARP_LINK_LOCAL_IPADDRESS_FORMAT "169.254.%u.%u"

#define MFI_ARP_CONFLICT_TEST_MAX_RETRIES  14

typedef enum
{
    MFI_ARP_PKT_TYPE_INVALID = -1,
    MFI_ARP_PKT_TYPE_PROBE = 0,
    MFI_ARP_PKT_TYPE_ANNOUNCEMENT,
    MFI_ARP_PKT_TYPE_MAX, /* add type before here */
}MFI_ARP_PKT_TYPE_E;

enum
{
    MFI_ARP_DO_RECV_PKTS = 0, /* recv only */
    MFI_ARP_DO_SEND_PKTS, /* send only */
    MFI_ARP_DO_SEND_RECV_PKTS, /* send and recv both */
};

typedef enum
{    
    MFI_ARP_RET_OK = 0,    
    MFI_ARP_INVALID_ARGUMENTS,

    MFI_ARP_SOCKET_INIT_ERROR,
    MFI_ARP_SOCKET_IOCTL_ERROR,
    MFI_ARP_SOCKET_UNAVAILABLE_ERROR,
    MFI_ARP_SOCKET_HOSTNAME_ERROR,
    MFI_ARP_SOCKET_BIND_ERROR,
    MFI_ARP_SOCKET_NAME_ERROR,
    
    MFI_ARP_REPLY_CONFLICT_ERROR,
    MFI_ARP_REPLY_NOT_RECEIVED,

    MFI_ARP_PROBE_CONFLICT_TEST_FAILED,
    MFI_ARP_SUBSEQUENT_CONFLICT_FAILED,
    MFI_ARP_SUBSEQUENT_CONFLICT_SUCCESS,
    MFI_ARP_CABLE_HOT_PLUG_TEST_SUCCESS,
}MFI_ARP_RET_E;

extern int mfi_arp_cable_reconnect_testing;
extern char gRandomLinklocalIPaddress[64];

//extern int mfi_arp_handling(char *inLinklocalIPAddress, int initial_probe);
extern int mfi_arp_set_random_link_local_ip(void);
extern int mfi_arp_packets_handling(void);
extern int mfi_arp_sending_initial_probe(char *inLinklocalIPAddress);
#endif /* _MFI_ARP_H_ */

