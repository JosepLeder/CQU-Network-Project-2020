#include "utils.h"

// Define some constants.
#define ETH_HDRLEN 14 // Ethernet header length
#define IP4_HDRLEN 20 // IPv4 header length
#define UDP_HDRLEN 8  // UDP header length, excludes data
#define MTU_SZ 1      // Unit is byte.

int sender(short src_port, short dst_port)
{
    int i, status, frame_length, sd, bytes, datalen, *ip_flags;
    char *interface, *target, *src_ip, *dst_ip, *send_data;
    char input_buf[1024];
    struct ip iphdr;
    struct udphdr udphdr;
    uint8_t *data, *src_mac, *dst_mac, *ether_frame;
    struct addrinfo hints, *res;
    struct sockaddr_in *ipv4;
    struct sockaddr_ll device;
    struct ifreq ifr;
    void *tmp;

    // Allocate memory for various arrays.
    src_mac = allocate_ustrmem(6);
    dst_mac = allocate_ustrmem(6);
    data = allocate_ustrmem(IP_MAXPACKET);
    ether_frame = allocate_ustrmem(IP_MAXPACKET);
    interface = allocate_strmem(40);
    target = allocate_strmem(40);
    src_ip = allocate_strmem(INET_ADDRSTRLEN);
    dst_ip = allocate_strmem(INET_ADDRSTRLEN);
    ip_flags = allocate_intmem(4);

    // Interface to send packet through.
    strcpy(interface, "lo");

    int tsd;
    // Submit request for a socket descriptor to look up interface.
    if ((tsd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        perror("socket() failed to get socket descriptor for using ioctl() ");
        exit(EXIT_FAILURE);
    }

    // Use ioctl() to look up interface name and get its MAC address.
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", interface);
    if (ioctl(tsd, SIOCGIFHWADDR, &ifr) < 0)
    {
        perror("ioctl() failed to get source MAC address ");
        return (EXIT_FAILURE);
    }

    // Copy source MAC address.
    memcpy(src_mac, ifr.ifr_hwaddr.sa_data, 6 * sizeof(uint8_t));

    // Report source MAC address to stdout.
    printf("MAC address for interface %s is ", interface);
    for (i = 0; i < 5; i++)
    {
        printf("%02x:", src_mac[i]);
    }
    printf("%02x\n", src_mac[5]);

    if (ioctl(tsd, SIOCGIFADDR, &ifr) < 0)
    {
        perror("can't get ip address");
        exit(1);
    }
    strcpy(src_ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    // Copy source IPv4 address
    printf("IP address for interface %s is : %s\n", interface, src_ip);

    close(tsd);

    // Find interface index from interface name and store index in
    // struct sockaddr_ll device, which will be used as an argument of sendto().
    memset(&device, 0, sizeof(device));
    if ((device.sll_ifindex = if_nametoindex(interface)) == 0)
    {
        perror("if_nametoindex() failed to obtain interface index ");
        exit(EXIT_FAILURE);
    }
    printf("Index for interface %s is %i\n", interface, device.sll_ifindex);

    // Set destination MAC address: you need to fill these out
    dst_mac[0] = 0xff;
    dst_mac[1] = 0xff;
    dst_mac[2] = 0xff;
    dst_mac[3] = 0xff;
    dst_mac[4] = 0xff;
    dst_mac[5] = 0xff;
    // memcpy(dst_mac, dst_mac, 6 * sizeof(uint8_t));
    // Destination URL or IPv4 address: you need to fill this out
    strcpy(target, src_ip);

    // Fill out hints for getaddrinfo().
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = hints.ai_flags | AI_CANONNAME;

    // Resolve target using getaddrinfo().
    if ((status = getaddrinfo(target, NULL, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }
    ipv4 = (struct sockaddr_in *)res->ai_addr;
    tmp = &(ipv4->sin_addr);
    if (inet_ntop(AF_INET, tmp, dst_ip, INET_ADDRSTRLEN) == NULL)
    {
        status = errno;
        fprintf(stderr, "inet_ntop() failed.\nError message: %s", strerror(status));
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);

    // Fill out sockaddr_ll.
    device.sll_family = AF_PACKET;
    memcpy(device.sll_addr, src_mac, 6 * sizeof(uint8_t));
    device.sll_halen = 6;

    while (1)
    {
        memset(input_buf, 0, 1024);
        fgets(input_buf, 1024, stdin);
        int msg_len = strlen(input_buf);
        for (int i = 0; i < msg_len; i = i + MTU_SZ)
        {
            // Copy UDP data
            int idx;
            send_data = input_buf + i;
            datalen = MTU_SZ;
            for (idx = 0; idx < datalen; idx++)
            {
                data[idx] = send_data[idx];
            }
            // IPv4 header

            // IPv4 header length (4 bits): Number of 32-bit words in header = 5
            iphdr.ip_hl = IP4_HDRLEN / sizeof(uint32_t);

            // Internet Protocol version (4 bits): IPv4
            iphdr.ip_v = 4;

            // Type of service (8 bits)
            iphdr.ip_tos = 0;

            // Total length of datagram (16 bits): IP header + UDP header + datalen
            iphdr.ip_len = htons(IP4_HDRLEN + UDP_HDRLEN + datalen);

            // ID sequence number (16 bits): unused, since single datagram
            iphdr.ip_id = htons(0);

            // Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

            // Zero (1 bit)
            ip_flags[0] = 0;

            // Do not fragment flag (1 bit)
            ip_flags[1] = 0;

            // More fragments following flag (1 bit)
            ip_flags[2] = (i + MTU_SZ < msg_len);

            // Fragmentation offset (13 bits)
            ip_flags[3] = i / MTU_SZ;

            iphdr.ip_off = htons((ip_flags[0] << 15) + (ip_flags[1] << 14) + (ip_flags[2] << 13) + ip_flags[3]);

            // Time-to-Live (8 bits): default to maximum value
            iphdr.ip_ttl = 255;

            // Transport layer protocol (8 bits): 17 for UDP
            iphdr.ip_p = IPPROTO_UDP;

            // Source IPv4 address (32 bits)
            if ((status = inet_pton(AF_INET, src_ip, &(iphdr.ip_src))) != 1)
            {
                fprintf(stderr, "inet_pton() failed.\nError message: %s", strerror(status));
                exit(EXIT_FAILURE);
            }

            // Destination IPv4 address (32 bits)
            if ((status = inet_pton(AF_INET, dst_ip, &(iphdr.ip_dst))) != 1)
            {
                fprintf(stderr, "inet_pton() failed.\nError message: %s", strerror(status));
                exit(EXIT_FAILURE);
            }

            // IPv4 header checksum (16 bits): set to 0 when calculating checksum
            iphdr.ip_sum = 0;
            iphdr.ip_sum = checksum((uint16_t *)&iphdr, IP4_HDRLEN);

            // UDP header

            // Source port number (16 bits): pick a number
            udphdr.source = htons(src_port);

            // Destination port number (16 bits): pick a number
            udphdr.dest = htons(dst_port);

            // Length of UDP datagram (16 bits): UDP header + UDP data
            udphdr.len = htons(UDP_HDRLEN + datalen);

            // UDP checksum (16 bits)
            udphdr.check = udp4_checksum(iphdr, udphdr, data, datalen);

            // Fill out ethernet frame header.

            // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + UDP header + UDP data)
            frame_length = 6 + 6 + 2 + IP4_HDRLEN + UDP_HDRLEN + datalen;

            // Destination and Source MAC addresses
            memcpy(ether_frame, dst_mac, 6 * sizeof(uint8_t));
            memcpy(ether_frame + 6, src_mac, 6 * sizeof(uint8_t));

            // Next is ethernet type code (ETH_P_IP for IPv4).
            // http://www.iana.org/assignments/ethernet-numbers
            ether_frame[12] = ETH_P_IP / 256;
            ether_frame[13] = ETH_P_IP % 256;

            // Next is ethernet frame data (IPv4 header + UDP header + UDP data).

            // IPv4 header
            memcpy(ether_frame + ETH_HDRLEN, &iphdr, IP4_HDRLEN * sizeof(uint8_t));

            // UDP header
            memcpy(ether_frame + ETH_HDRLEN + IP4_HDRLEN, &udphdr, UDP_HDRLEN * sizeof(uint8_t));

            // UDP data
            memcpy(ether_frame + ETH_HDRLEN + IP4_HDRLEN + UDP_HDRLEN, data, datalen * sizeof(uint8_t));

            // Submit request for a raw socket descriptor.
            if ((sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
            {
                perror("socket() failed ");
                exit(EXIT_FAILURE);
            }

            // Send ethernet frame to socket.
            if ((bytes = sendto(sd, ether_frame, frame_length, 0, (struct sockaddr *)&device, sizeof(device))) <= 0)
            {
                perror("sendto() failed");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Close socket descriptor.
    close(sd);

    // Free allocated memory.
    free(src_mac);
    free(dst_mac);
    free(data);
    free(ether_frame);
    free(interface);
    free(target);
    free(src_ip);
    free(dst_ip);
    free(ip_flags);

    return (EXIT_SUCCESS);
}