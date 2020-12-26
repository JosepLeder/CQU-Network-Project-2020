#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // strcpy, memset(), and memcpy()

#include <unistd.h> 		 // close()
#include <netdb.h>			 // struct addrinfo
#include <sys/types.h>		 // needed for socket(), uint8_t, uint16_t, uint32_t
#include <sys/socket.h>		 // needed for socket()
#include <netinet/in.h>		 // IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/ip.h>		 // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>	 // struct udphdr
#include <arpa/inet.h>		 // inet_pton() and inet_ntop()
#include <sys/ioctl.h>		 // macro ioctl is defined
#include <bits/ioctls.h>	 // defines values for argument "request" of ioctl.
#include <net/if.h>			 // struct ifreq
#include <linux/if_ether.h>	 // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <linux/if_packet.h> // struct sockaddr_ll (see man 7 packet)
#include <net/ethernet.h>
#include <errno.h> // errno, perror()

// Function prototypes
uint16_t checksum(uint16_t *, int);
uint16_t udp4_checksum(struct ip, struct udphdr, uint8_t *, int);
char *allocate_strmem(int);
uint8_t *allocate_ustrmem(int);
int *allocate_intmem(int);

// Computing the internet checksum (RFC 1071).
// Note that the internet checksum does not preclude collisions.
uint16_t
checksum(uint16_t *addr, int len)
{
	int count = len;
	register uint32_t sum = 0;
	uint16_t answer = 0;

	// Sum up 2-byte values until none or only one byte left.
	while (count > 1)
	{
		sum += *(addr++);
		count -= 2;
	}

	// Add left-over byte, if any.
	if (count > 0)
	{
		sum += *(uint8_t *)addr;
	}

	// Fold 32-bit sum into 16 bits; we lose information by doing this,
	// increasing the chances of a collision.
	// sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
	while (sum >> 16)
	{
		sum = (sum & 0xffff) + (sum >> 16);
	}

	// Checksum is one's compliment of sum.
	answer = ~sum;

	return (answer);
}

// Build IPv4 UDP pseudo-header and call checksum function.
uint16_t
udp4_checksum(struct ip iphdr, struct udphdr udphdr, uint8_t *payload, int payloadlen)
{
	char buf[IP_MAXPACKET];
	char *ptr;
	int chksumlen = 0;
	int i;

	ptr = &buf[0]; // ptr points to beginning of buffer buf

	// Copy source IP address into buf (32 bits)
	memcpy(ptr, &iphdr.ip_src.s_addr, sizeof(iphdr.ip_src.s_addr));
	ptr += sizeof(iphdr.ip_src.s_addr);
	chksumlen += sizeof(iphdr.ip_src.s_addr);

	// Copy destination IP address into buf (32 bits)
	memcpy(ptr, &iphdr.ip_dst.s_addr, sizeof(iphdr.ip_dst.s_addr));
	ptr += sizeof(iphdr.ip_dst.s_addr);
	chksumlen += sizeof(iphdr.ip_dst.s_addr);

	// Copy zero field to buf (8 bits)
	*ptr = 0;
	ptr++;
	chksumlen += 1;

	// Copy transport layer protocol to buf (8 bits)
	memcpy(ptr, &iphdr.ip_p, sizeof(iphdr.ip_p));
	ptr += sizeof(iphdr.ip_p);
	chksumlen += sizeof(iphdr.ip_p);

	// Copy UDP length to buf (16 bits)
	memcpy(ptr, &udphdr.len, sizeof(udphdr.len));
	ptr += sizeof(udphdr.len);
	chksumlen += sizeof(udphdr.len);

	// Copy UDP source port to buf (16 bits)
	memcpy(ptr, &udphdr.source, sizeof(udphdr.source));
	ptr += sizeof(udphdr.source);
	chksumlen += sizeof(udphdr.source);

	// Copy UDP destination port to buf (16 bits)
	memcpy(ptr, &udphdr.dest, sizeof(udphdr.dest));
	ptr += sizeof(udphdr.dest);
	chksumlen += sizeof(udphdr.dest);

	// Copy UDP length again to buf (16 bits)
	memcpy(ptr, &udphdr.len, sizeof(udphdr.len));
	ptr += sizeof(udphdr.len);
	chksumlen += sizeof(udphdr.len);

	// Copy UDP checksum to buf (16 bits)
	// Zero, since we don't know it yet
	*ptr = 0;
	ptr++;
	*ptr = 0;
	ptr++;
	chksumlen += 2;

	// Copy payload to buf
	memcpy(ptr, payload, payloadlen);
	ptr += payloadlen;
	chksumlen += payloadlen;

	// Pad to the next 16-bit boundary
	for (i = 0; i < payloadlen % 2; i++, ptr++)
	{
		*ptr = 0;
		ptr++;
		chksumlen++;
	}

	return checksum((uint16_t *)buf, chksumlen);
}

// Allocate memory for an array of chars.
char *
allocate_strmem(int len)
{
	void *tmp;

	if (len <= 0)
	{
		fprintf(stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
		exit(EXIT_FAILURE);
	}

	tmp = (char *)malloc(len * sizeof(char));
	if (tmp != NULL)
	{
		memset(tmp, 0, len * sizeof(char));
		return (tmp);
	}
	else
	{
		fprintf(stderr, "ERROR: Cannot allocate memory for array allocate_strmem().\n");
		exit(EXIT_FAILURE);
	}
}

// Allocate memory for an array of unsigned chars.
uint8_t *
allocate_ustrmem(int len)
{
	void *tmp;

	if (len <= 0)
	{
		fprintf(stderr, "ERROR: Cannot allocate memory because len = %i in allocate_ustrmem().\n", len);
		exit(EXIT_FAILURE);
	}

	tmp = (uint8_t *)malloc(len * sizeof(uint8_t));
	if (tmp != NULL)
	{
		memset(tmp, 0, len * sizeof(uint8_t));
		return (tmp);
	}
	else
	{
		fprintf(stderr, "ERROR: Cannot allocate memory for array allocate_ustrmem().\n");
		exit(EXIT_FAILURE);
	}
}

// Allocate memory for an array of ints.
int *allocate_intmem(int len)
{
	void *tmp;

	if (len <= 0)
	{
		fprintf(stderr, "ERROR: Cannot allocate memory because len = %i in allocate_intmem().\n", len);
		exit(EXIT_FAILURE);
	}

	tmp = (int *)malloc(len * sizeof(int));
	if (tmp != NULL)
	{
		memset(tmp, 0, len * sizeof(int));
		return (tmp);
	}
	else
	{
		fprintf(stderr, "ERROR: Cannot allocate memory for array allocate_intmem().\n");
		exit(EXIT_FAILURE);
	}
}

#endif