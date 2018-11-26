#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <sys/uio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mfi_arp.h"

#define endret(x) {ret = x; goto end;}
char gRandomLinklocalIPaddress[64] = {0};
int mfi_arp_cable_reconnect_testing = 0;


int quit_on_reply = 0;
char *device = "wlan0";
int unsolicited, advert;
int unicasting;
int broadcast_only;

struct timeval start, last;

int sent, brd_sent;
int received, brd_recv, req_recv;


int send_pack(int s, struct in_addr src, struct in_addr dst,
              struct sockaddr_ll *ME, struct sockaddr_ll *HE)
{
    int err;
    struct timeval now;
    unsigned char buf[256];
    struct arphdr *ah = (struct arphdr*)buf;
    unsigned char *p = (unsigned char *)(ah + 1);

    ah->ar_hrd = htons(ME->sll_hatype);
    if(ah->ar_hrd == htons(ARPHRD_FDDI))
        ah->ar_hrd = htons(ARPHRD_ETHER);
    ah->ar_pro = htons(ETH_P_IP);
    ah->ar_hln = ME->sll_halen;
    ah->ar_pln = 4;
    ah->ar_op  = advert ? htons(ARPOP_REPLY) : htons(ARPOP_REQUEST);

    memcpy(p, &ME->sll_addr, ah->ar_hln);
    p += ME->sll_halen;

    memcpy(p, &src, 4);
    p += 4;

    if(advert)
        memcpy(p, &ME->sll_addr, ah->ar_hln);
    else
        memcpy(p, &HE->sll_addr, ah->ar_hln);
    p += ah->ar_hln;

    memcpy(p, &dst, 4);
    p += 4;

    gettimeofday(&now, NULL);
    err = sendto(s, buf, p - buf, 0, (struct sockaddr*)HE, sizeof(*HE));
    if(err == p - buf)
    {
        last = now;
        sent++;
        if(!unicasting)
            brd_sent++;
    }
    return err;
}


static int recv_pack(unsigned char *buf, int len, struct sockaddr_ll *FROM, struct sockaddr_ll *me, struct in_addr *src, struct in_addr *dst, int dad)
{
    struct timeval tv;
    struct arphdr *ah = (struct arphdr*)buf;
    unsigned char *p = (unsigned char *)(ah + 1);
    struct in_addr src_ip, dst_ip;

    gettimeofday(&tv, NULL);

    /* Filter out wild packets */
    if(FROM->sll_pkttype != PACKET_HOST &&
            FROM->sll_pkttype != PACKET_BROADCAST &&
            FROM->sll_pkttype != PACKET_MULTICAST) {
        return 0;
    }

    /* Only these types are recognised */
    if(ah->ar_op != htons(ARPOP_REQUEST) &&
            ah->ar_op != htons(ARPOP_REPLY)) {
        return 0;
    }

    /* ARPHRD check and this darned FDDI hack here :-( */
    if(ah->ar_hrd != htons(FROM->sll_hatype) &&
            (FROM->sll_hatype != ARPHRD_FDDI || ah->ar_hrd != htons(ARPHRD_ETHER))) {
        return 0;
    }

    /* Protocol must be IP. */
    if(ah->ar_pro != htons(ETH_P_IP)){
        return 0;
    }
    
    if(ah->ar_pln != 4) {
        return 0;
     }
    
    if(ah->ar_hln != me->sll_halen) {
        return 0;
    }
    
    if(len < sizeof(*ah) + 2 * (4 + ah->ar_hln)){
        return 0;
    }

    memcpy(&src_ip, p + ah->ar_hln, 4);
    memcpy(&dst_ip, p + ah->ar_hln + 4 + ah->ar_hln, 4);

    if(!dad)
    {
        if(src_ip.s_addr != dst->s_addr){
            return 0;
        }
        
        if(src->s_addr != dst_ip.s_addr){
            return 0;
        }
        
        if(memcmp(p + ah->ar_hln + 4, &me->sll_addr, ah->ar_hln)){
            return 0;
        }
    }
    else
    {
        //printf("recvd src ip %s, dst ip %s, [%u], [%u]\n", inet_ntoa(src_ip), inet_ntoa(*dst), src_ip.s_addr, dst->s_addr);
        
       // if(src_ip.s_addr != dst->s_addr)
       if (0 != strcmp(inet_ntoa(src_ip), inet_ntoa(*dst)))
       {
            return 0;
        }
        
        if(memcmp(p, &me->sll_addr, me->sll_halen) == 0){
            return 0;
        }

        if(src->s_addr && src->s_addr != dst_ip.s_addr){
            return 0;
        }
    }
    
    received++;
    
    if(FROM->sll_pkttype != PACKET_HOST)
        brd_recv++;
    if(ah->ar_op == htons(ARPOP_REQUEST))
        req_recv++;

    printf("\n[%s-%d] received [%d] brd_recv [%d] req_recv [%d]\n", __func__, __LINE__, received, brd_recv, req_recv);

//    if(quit_on_reply)
    //    finish();

/*    if(!broadcast_only)
    {
        memcpy(he.sll_addr, p, me->sll_halen);
        unicasting = 1;
    }*/
    return 1;
}


static unsigned int gen_random_num(unsigned int min, unsigned int max)
{
    struct timeval tm;
    int ret;
    gettimeofday(&tm, NULL);
    srand(tm.tv_sec + tm.tv_usec);
    ret = rand()%max;
    if( ret < min )
        ret = min;
    return ret;
}


int mfi_arp_gen_random_ipaddress(char *outIPAddress, int MaxOutLen)
{    
    snprintf(outIPAddress, MaxOutLen, MFI_ARP_LINK_LOCAL_IPADDRESS_FORMAT, gen_random_num(0,255), gen_random_num(1, 254));
    printf("\n [%s-%d], new random ipaddress [%s].\n", __func__, __LINE__, outIPAddress);

    return 0;
}


int mfi_arp_set_random_link_local_ip(void)
{
    char IPAddrBuff[32] = {0}, cmd[128] = {0};
    
    mfi_arp_gen_random_ipaddress(IPAddrBuff, sizeof(IPAddrBuff));
    
    snprintf(cmd, sizeof(cmd), "ifconfig wlan0 %s", IPAddrBuff);

    memset(gRandomLinklocalIPaddress, 0, sizeof(gRandomLinklocalIPaddress));
    strncpy(gRandomLinklocalIPaddress, IPAddrBuff, sizeof(gRandomLinklocalIPaddress) - 1);

    system(cmd);
    
    return 0;
}



int mfi_arp_ipaddress_convert(char *inLinklocalIPAddress, struct in_addr *dst)
{
    
    if(inet_aton(inLinklocalIPAddress, dst) != 1)
    {
        struct hostent *hp;
        hp = gethostbyname2(inLinklocalIPAddress, AF_INET);
        if(!hp)
        {
            printf("arping: unknown host %s\n", inLinklocalIPAddress);
            return MFI_ARP_SOCKET_HOSTNAME_ERROR;
        }
        
        memcpy(&dst, hp->h_addr, 4);
    }

    return 0;
}


int mfi_arp_initiate_socket(int *ret_sockfd, struct sockaddr_ll *ret_sock_addr, int recv_timeout_seconds)
{
    int ret = MFI_ARP_RET_OK;
    int ifindex;
    struct sockaddr_ll me;

    int sockfd = socket(PF_PACKET, SOCK_DGRAM, 0);

    if(sockfd  < 0)
    {
        //perror("arping: socket");
        endret(MFI_ARP_SOCKET_INIT_ERROR);
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, MFI_ARP_INTF_NAME, IFNAMSIZ - 1);
    if(ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0)
    {
        printf("arping: unknown iface %s\n", MFI_ARP_INTF_NAME);
        endret(MFI_ARP_SOCKET_IOCTL_ERROR);
    }

    struct timeval tv;
    tv.tv_sec = recv_timeout_seconds;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    ifindex = ifr.ifr_ifindex;

    if(ioctl(sockfd, SIOCGIFFLAGS, (char*)&ifr))
    {
        perror("ioctl(SIOCGIFFLAGS)");
        endret(MFI_ARP_SOCKET_IOCTL_ERROR);
    }

    if(!(ifr.ifr_flags & IFF_UP))
    {
        printf("Interface \"%s\" is down\n", MFI_ARP_INTF_NAME);
        endret(MFI_ARP_SOCKET_UNAVAILABLE_ERROR);
    }
    
    if(ifr.ifr_flags & (IFF_NOARP | IFF_LOOPBACK))
    {
        printf("Interface \"%s\" is not ARPable\n", MFI_ARP_INTF_NAME);
        endret(MFI_ARP_SOCKET_UNAVAILABLE_ERROR);
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
#endif

    me.sll_family = AF_PACKET;
    me.sll_ifindex = ifindex;
    me.sll_protocol = htons(ETH_P_ARP);

    if(bind(sockfd, (struct sockaddr*)&me, sizeof(me)) == -1)
    {
        perror("bind");
        endret(MFI_ARP_SOCKET_BIND_ERROR);
    }

    int alen = sizeof(me);

    if(getsockname(sockfd, (struct sockaddr*)&me, &alen) == -1)
    {
        perror("getsockname");
        endret(MFI_ARP_SOCKET_NAME_ERROR);
    }

    if(me.sll_halen == 0)
    {
        printf("Interface \"%s\" is not ARPable (no ll address)\n", MFI_ARP_INTF_NAME);
        endret(MFI_ARP_SOCKET_UNAVAILABLE_ERROR);
    }

end:

    if (MFI_ARP_RET_OK == ret) 
    {
        *ret_sockfd = sockfd;
        
        if (ret_sock_addr)
        {
            memcpy(ret_sock_addr, &me, sizeof(struct sockaddr_ll));
        }   
    }
    else 
    {
        if (sockfd > 0)
            close(sockfd);
    }
    
    return ret;
}


static int __mfi_arp_send_recv_packets(int sockfd, struct in_addr src, struct in_addr dst, struct sockaddr_ll *me,  struct sockaddr_ll *he, int do_send, int dad)
{    
    if (MFI_ARP_DO_SEND_PKTS == do_send)
    {
        send_pack(sockfd, src, dst, me, he);
        return 0;
    }

    if (MFI_ARP_DO_SEND_RECV_PKTS == do_send)
    {
        send_pack(sockfd, src, dst, me, he);
    }
    
    char packet[4096];
    struct sockaddr_ll from;
    int alen = sizeof(from);
    int cc;

    //printf("\n [%s-%d], Entering arp pkts recving.\n", __func__, __LINE__);
    
    cc = recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&from, &alen);

    if (cc < 0)
    {
        //perror("recv packets error:");
        printf("recv arp reply timeout\n");
        return -1;
    }

    recv_pack(packet, cc, &from, me, &src, &dst, dad);
    
    //printf("\n [%s-%d], Exiting arp pkts recving.\n", __func__, __LINE__);

    return 0;
}


int mfi_arp_send_recv_packets(int pkt_type, int sockfd, struct in_addr dst, struct sockaddr_ll *me,  struct sockaddr_ll *he, int do_send, int dad)
{
    struct in_addr src;
     
    if (MFI_ARP_PKT_TYPE_PROBE == pkt_type)
        memset(&src, 0, sizeof(src));
    else if (MFI_ARP_PKT_TYPE_ANNOUNCEMENT == pkt_type)
        memcpy(&src, &dst, sizeof(src));
    else {
        printf("%s, Invalid pkt types\n", __func__);
        return -1;
    }
    
    return __mfi_arp_send_recv_packets(sockfd, src, dst, me, he, do_send, dad);
}


/*
** The device is powered on.  The test tool verifies that the device sends an ARP probe for its chosen address. 
** For arp probe packets, we donot need specifiy source ipaddres but dst ipaddress.
*/
int mfi_arp_sending_initial_probe(char *inLinklocalIPAddress)
{
    int ret = MFI_ARP_RET_OK;
    struct in_addr dst;
    int sockfd = -1;
    struct sockaddr_ll me;
    struct sockaddr_ll he;
    int dad = 1, sock_recv_timeout_seconds = 3;
    
    if(inLinklocalIPAddress == NULL)
    {
        printf("invalid args\n");
        endret(MFI_ARP_INVALID_ARGUMENTS);
    }
        
    ret = mfi_arp_initiate_socket(&sockfd, &me, sock_recv_timeout_seconds);

    if (ret != MFI_ARP_RET_OK)
        endret(ret);
    
    memcpy(&he, &me, sizeof(struct sockaddr_ll));

    /* broadcast ipaddress 255.255.255.255 */
    memset(he.sll_addr, -1, he.sll_halen);

    mfi_arp_ipaddress_convert(inLinklocalIPAddress, &dst);
    
    printf("\n[%s-%d], Sending initial arp probe\n", __func__, __LINE__);
        
    mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_PROBE, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_RECV_PKTS, dad);

end:

    if (sockfd > 0)
        close(sockfd);

    return ret;
}


/*
** The test tool denies the device's initial probe by sending an ARP reply for the device's chosen address.  
** The tool verifies that the device picks a new address and probes again.Next, the test tool sends a conflicting probe for this new address, 
** simulating two devices probing for the same address.  The test tool verifies that the device also treats this as a conflict and picks a new address and probes again.
*/    
static int mfi_arp_probe_conflict_test()
{
    int ret = MFI_ARP_RET_OK;
    struct in_addr dst;
    int sockfd = -1;
    struct sockaddr_ll me;
    struct sockaddr_ll he;
    int dad = 1;

    printf("\n *********** Starting probe conflict Test *********** \n");
    
    ret = mfi_arp_initiate_socket(&sockfd, &me, 1);

    if (ret != MFI_ARP_RET_OK)
        endret(ret);
    
    memcpy(&he, &me, sizeof(struct sockaddr_ll));

    /* broadcast ipaddress 255.255.255.255 */
    memset(he.sll_addr, -1, he.sll_halen);

    int count = 0;

CONFLICT_TEST:
    
    dad = 1;/* broadcast probes */
    count ++;
    received = 0;
    
    mfi_arp_set_random_link_local_ip();

    printf("\n[%s-%d], Sending new arp probe with [%s], count [%d]\n", __func__, __LINE__, gRandomLinklocalIPaddress, count);
    
    mfi_arp_ipaddress_convert(gRandomLinklocalIPaddress, &dst);

    mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_PROBE, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_RECV_PKTS, dad);

    if (received > 0 && count <= MFI_ARP_CONFLICT_TEST_MAX_RETRIES) 
    {        
        usleep(1000000);

        goto CONFLICT_TEST;
    }
   
    if (mfi_arp_cable_reconnect_testing)
    {
        printf("\n[%s-%d], cable hot plug test end\n", __func__, __LINE__);

        //now we need send two probes in 2 seconds and then made a notify
        int i = 0;
        for (; i < 3; i ++)
        {
            printf("\n[%s-%d], sending arp probe [%d]\n", __func__, __LINE__, i);
            mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_PROBE, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_RECV_PKTS, dad);
            usleep(500000);
        }

        dad = 0;
        for (i = 0; i < 2; i ++)
        {
            printf("\n[%s-%d], sending arp anouncements [%d]\n", __func__, __LINE__, i);
            mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_ANNOUNCEMENT, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_RECV_PKTS, dad);
            usleep(500000);
        }

        mfi_arp_cable_reconnect_testing = 0;
        endret(MFI_ARP_CABLE_HOT_PLUG_TEST_SUCCESS);
    }


end:

    if (sockfd > 0)
        close(sockfd);

    return ret;
}


/*
** The test tool now allows the device to complete its probing. 
** Devices conforming to the IPv4 Link-Local draft 7 spec must send four probes followed by two announcements, 
** while devices conforming to draft 8 and later must send three probes followed by two announcements. 
** It is a failure if the time gap between consecutive probes is more than two seconds.
*/    
static int mfi_arp_probing_complete_test()
{
    int ret = MFI_ARP_RET_OK;
    struct in_addr dst;
    int sockfd = -1;
    struct sockaddr_ll me;
    struct sockaddr_ll he;
    int dad = 1, time_out = 1;

    printf("\n *********** Starting probing complete Test *********** \n");
        
    ret = mfi_arp_initiate_socket(&sockfd, &me, time_out);

    if (ret != MFI_ARP_RET_OK)
        endret(ret);
    
    he = me;

    //broadcast ipaddress 255.255.255.255 for he
    memset(he.sll_addr, -1, he.sll_halen);

    mfi_arp_ipaddress_convert(gRandomLinklocalIPaddress, &dst);
    
    //now we need send two probes in 2seconds and then made a notify
    int i = 0;
    for (i = 0; i < 3; i ++)
    {
        printf("\n[%s-%d], sending arp probe [%d]\n", __func__, __LINE__, i);
        mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_PROBE, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_PKTS, dad);
        usleep(500000);
    }
    
    dad = 0;
    for (i = 0; i < 2; i ++)
    {
        printf("\n[%s-%d], sending arp anouncements [%d]\n", __func__, __LINE__, i);
        mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_ANNOUNCEMENT, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_PKTS, dad);
        usleep(500000);
    }

end:

    if (sockfd > 0) 
    {
        close(sockfd);
        sockfd = -1;
    }
    
    return ret;
}


/*
** The test tool will wait ten seconds, and then issue two ARP replies, six seconds apart, for the address the device is using.  
** The test tool verifies that the device then picks a new address and probes/announces again.  
** If the device does not wait for the second ARP reply before choosing a new address, instead probing immediately after the first conflict, a warning is generated.
*/
int mfi_arp_subsquent_conflict_test()
{
    int ret = MFI_ARP_SUBSEQUENT_CONFLICT_FAILED;
    struct in_addr dst;
    int sockfd = -1;
    struct sockaddr_ll me;
    struct sockaddr_ll he;
    int dad = 1, time_out = 10, i = 0;

    printf("\n *********** Starting subsequent conflict Test *********** \n");
    
    //wait for 10s to receive 2 arp replys    
    ret = mfi_arp_initiate_socket(&sockfd, &me, time_out);

    if (ret != MFI_ARP_RET_OK)
        endret(ret);

    mfi_arp_ipaddress_convert(gRandomLinklocalIPaddress, &dst);
    
    he = me;

    //broadcast ipaddress 255.255.255.255 for he
    memset(he.sll_addr, -1, he.sll_halen);    

    received = 0;

    mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_PROBE, sockfd, dst, &me, &he, MFI_ARP_DO_RECV_PKTS, dad);
    
    for (i = 0; i < 3; i ++)
    {
        mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_PROBE, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_RECV_PKTS, dad);

       printf("\n[%s-%d], recvd arp reply for [%d] times\n", __func__, __LINE__, received);

       if (received >= 2)
        break;
    }

    /* we have received 2 arp replies, send arp probe with new ipaddress */
    if (received >= 2)
    {
        mfi_arp_set_random_link_local_ip();
        mfi_arp_ipaddress_convert(gRandomLinklocalIPaddress, &dst);

        int i = 0;
        for (i = 0; i < 3; i ++)
        {
            printf("\n[%s-%d], sending arp probe [%d]\n", __func__, __LINE__, i);
            mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_PROBE, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_PKTS, dad);
            usleep(500000);
        }
        
        dad = 0;
        for (i = 0; i < 2; i ++)
        {
            printf("\n[%s-%d], sending arp anouncements [%d]\n", __func__, __LINE__, i);
            mfi_arp_send_recv_packets(MFI_ARP_PKT_TYPE_ANNOUNCEMENT, sockfd, dst, &me, &he, MFI_ARP_DO_SEND_PKTS, dad);
            usleep(500000);
        }

        ret = MFI_ARP_RET_OK;
    }

end:

    if (sockfd > 0) 
    {
        close(sockfd);
        sockfd = -1;
    }

    return ret;
}

/*
** arp routine msg handler
*/
int mfi_arp_packets_handling(void)
{
    int ret = MFI_ARP_RET_OK;

    /*
    ** Step 2. probe conflict
    ** Step 3. Rate Limiting
    */    
    if (MFI_ARP_RET_OK != (ret = mfi_arp_probe_conflict_test())) 
    {
        if (MFI_ARP_CABLE_HOT_PLUG_TEST_SUCCESS == ret)
        {
            ret = MFI_ARP_RET_OK;
        }
        
        printf("[%s-%d], arp probe conflict test error !!\n", __func__, __LINE__);
        return ret;
    }

    /*
    ** Step 4. probing complete test
    */    
    if (MFI_ARP_RET_OK != (ret = mfi_arp_probing_complete_test())) 
    {
        printf("[%s-%d], arp probing complete test error !!\n", __func__, __LINE__);
        return ret;
    }

    /*
    ** Step 5. Subsequent Conflict
    */    
    if (MFI_ARP_RET_OK != (ret = mfi_arp_subsquent_conflict_test())) 
    {
        printf("[%s-%d], arp probing complete test error !!\n", __func__, __LINE__);
        return ret;
    }

    if (MFI_ARP_RET_OK == ret)
    {
        ret = MFI_ARP_SUBSEQUENT_CONFLICT_SUCCESS;/* expect result */
    }
    
    return ret;
}


