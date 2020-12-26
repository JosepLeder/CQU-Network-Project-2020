#include "utils.h"

uint8_t out_buf[1024];
short seq_num;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void print_mac(uint8_t *mac)
{
    int i = 0;
    for (i = 0; i < 5; i++)
    {
        printf("%02x:", mac[i]);
    }
    printf("%02x\n", mac[5]);
}

void print_buf(uint8_t* buf, int len)
{
    int i = 0;
    for (i = 0; i < len; i++)
    {
        printf("%c", buf[i]);
    }
}

void print_raw_buf(uint8_t *buf)
{
    int i = 0;
    for (i = 0; i < 60; i++)
    {
        printf("%02x:", buf[i]);
    }
    printf("%02x\n", buf[60]);
}

int recv_udp(uint8_t* frame, int portno, int datalen, int not_frag, int has_more_frag, int offset)
{
    struct udphdr udph;
    memcpy(&udph, frame, sizeof(udph));
    // Only parsing the packet to specified port.
    if (ntohs(udph.uh_dport) == (uint16_t)portno) 
    {
        if (not_frag)
        {
            print_buf(frame + sizeof(struct udphdr), datalen);
        }
        else if (has_more_frag)
        {
            memcpy(out_buf + offset * datalen, frame + sizeof(struct udphdr), datalen);
        }
        else if (seq_num != offset)
        {
            printf("%d: ", ntohs(udph.uh_sport));
            memcpy(out_buf + offset * datalen, frame + sizeof(struct udphdr), datalen);
            print_buf(out_buf, (offset + 1) * datalen);
            seq_num = 0;
        }
    }
    
    return ntohs(udph.uh_dport) == portno;
}

int recv_ip(uint8_t *frame, int portno)
{
    struct ip iph;
    memcpy(&iph, frame, sizeof(iph));
    if (iph.ip_p == IPPROTO_UDP)
    {
        unsigned short frag_field = ntohs(iph.ip_off);
        // Record ip packet seq number
        

        // Do not fragment flag (1 bit)
        int not_frag = (frag_field << 1) >> 15;

        // More fragments following flag (1 bit)
        int has_more_frag = (frag_field << 2) >> 15;

        // Fragmentation offset (13 bits)
        short offset = frag_field << 3;
        offset = offset >> 3;
        recv_udp(frame + 20 * sizeof(uint8_t), portno, ntohs(iph.ip_len) - sizeof(struct udphdr),
                 not_frag, has_more_frag, offset);
        seq_num = offset;
    }
    return 0;
}

int recv_eth(int sock, int portno)
{
    int n;
    uint8_t * buffer;
    buffer = allocate_ustrmem(256);
    memset(buffer, 0, 256 * sizeof(uint8_t));
    n = read(sock, buffer, 255);
    if (n < 0)
    {
        error("ERROR reading from socket");
        free(buffer);
        return -1;
    }
    else
    {
        struct ethhdr ethh;
        memcpy(&ethh, buffer, sizeof(struct ethhdr));
        // Only parsing IP packet
        if (ethh.h_proto == htons(ETH_P_IP))
        {
            recv_ip(buffer + sizeof(ethh), portno);
        }
        free(buffer);
        return n;
    }
}

void* receiver(void *arg)
{
    struct sockaddr_in serv_addr;
    struct ifreq ifr;
    int i, sd, portno;
    char *interface;

    portno = *(int *)arg;
    interface = allocate_strmem(40);

    int tsd;
    strcpy(interface, "lo");
    // Submit request for a socket descriptor to look up interface.
    if ((tsd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        perror("socket() failed to get socket descriptor for using ioctl() ");
        exit(EXIT_FAILURE);
    }

    // Use ioctl() to look up interface name and get its MAC address.
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface);

    // Initialize server socket.
    if ((sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        perror("socket() failed ");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    if (ioctl(sd, SIOCGIFINDEX, &ifr) == 0)
    {
        sll.sll_ifindex = ifr.ifr_ifindex;
        sll.sll_protocol = htons(ETH_P_ALL);
    }
    if (bind(sd, (struct sockaddr *)&sll, sizeof(sll)) < 0)
        error("ERROR on binding");

    while (recv_eth(sd, portno) > 0)
    {

    } /* end of while */
    close(sd);
    // Free allocated memory.
    free(interface);
}
