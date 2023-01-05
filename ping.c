
/* ping.c is a simple program to send ICMP ECHO_REQUEST packets to a destination host.
   This program sends ping in infinity loop and prints the time it took to receive the ping packet.
   command line : sudo make clean && sudo make all && sudo ./parta 8.8.8.8
   by: Elor Israeli & Roni Michaeli
   id's: 315465260 & 209233873
    */
    
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>

#define ICMP_HDRLEN 8// ICMP header length for echo request

float time_to_recv;           // create a double to store the time to receive the packet
bool new_ping_message = true; // create a bool to store if it is a new ping message or not
struct timeval start, end; // Timeval struct used for timing // create a timeval struct to store the time values;



// Function to calculate the checksum of the ICMP header
unsigned short calculate_checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Function to display the ping message
void display(void *buf, int bytes) 
{
    struct iphdr *ip = buf;                                                             // get the IP header from the incoming packet
    struct icmphdr *icmp = buf + ip->ihl * 4;                                           // get the ICMP header from the incoming packet
    int header_length = ICMP_HDRLEN;                                                              // ICMP header length is 8 bytes
    char sourceIPAddrReadable[32] = {'\0'};                                             // create a string to store the source IP address in a readable format
    inet_ntop(AF_INET, &ip->saddr, sourceIPAddrReadable, sizeof(sourceIPAddrReadable)); // convert the source IP address to a readable format
    if (new_ping_message)
    {
        new_ping_message = false;
        // print exactly like the example in the task
        printf("PING %s(%s): %d data bytes\n", sourceIPAddrReadable, sourceIPAddrReadable, bytes - header_length);
    }
    // print exactly like the example in the task
    printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.03f ms\n", bytes, sourceIPAddrReadable, icmp->un.echo.sequence, ip->ttl, time_to_recv);
}

int main(int argc, char *argv[])
{
    printf("hello parta\n");

    // Check if the user provided an argument for the hostname
    if (argc != 2)
    {
        printf("Usage: %s <hostname>\n", argv[0]);
        exit(1);
    }
    struct icmp icmphdr;                             // ICMP-header
    char data[IP_MAXPACKET] = "This is the ping.\n"; // Data to be sent
    char *DESTINATION_IP = argv[1];                  // Destination IP address

    int datalen = strlen(data) + 1;
    static int sequence_number = 0; // Set the sequence number to 0 and increment it by 1 for each ping message
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) // Create a raw socket with ICMP protocol
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        exit(1);
    }

    while (1) // Send pings continuously
    {
        sleep(1);                      // sleep for 1 second
        icmphdr.icmp_type = ICMP_ECHO; // Message Type (8 bits): echo request
        icmphdr.icmp_code = 0;         // Message Code (8 bits): echo request
        // Identifier (16 bits): some number to trace the response.
        // It will be copied to the response packet and used to map response to the request sent earlier.
        // Thus, it serves as a Transaction-ID when we need to make "ping"
        icmphdr.icmp_id = 18;// set the icmp_id to 18 because it is the id of the ping command
        icmphdr.icmp_seq = sequence_number++;                                                                          // Sequence Number (16 bits): starts at 0
        icmphdr.icmp_cksum = 0;                                                                                        // Header checksum (16 bits): set to 0 when calculating checksum
        char packet[IP_MAXPACKET];    // Buffer for entire packet
        memcpy((packet), &icmphdr, ICMP_HDRLEN);                                                                       // Copy the ICMP header to the packet buffer
        memcpy(packet + ICMP_HDRLEN, data, datalen);                                                                   // Copy the ICMP data to the packet buffer
        icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);                    // Calculate the ICMP header checksum
        memcpy((packet), &icmphdr, ICMP_HDRLEN);                                                                       // Copy the ICMP header to the packet buffer again, after calculating the checksum
        struct sockaddr_in dest_in;                                                                                    // Destination address
        memset(&dest_in, 0, sizeof(struct sockaddr_in));                                                               // Zero out the destination address
        dest_in.sin_family = AF_INET;// Internet Protocol v4 addresses
        dest_in.sin_port = 0;// Leave the port number as 0
        dest_in.sin_addr.s_addr = inet_addr(DESTINATION_IP);// Set destination IP address
        gettimeofday(&start, 0);// Get the start time to calculate the RTT
        int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in)); // Send the packet
        if (bytes_sent == -1)
        {
            fprintf(stderr, "sendto() failed with error: %d", errno);
            exit(1);
        }

        // Get the ping response
        bzero(packet, IP_MAXPACKET);     // Zero out the packet buffer
        socklen_t len = sizeof(dest_in); // Length of the destination address
        ssize_t bytes_received = -1;
        while ((bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len))) // Receive the packet from the socket
        {
            if (bytes_received > 0)
            {
                // Check the IP header
                struct iphdr *iphdr = (struct iphdr *)packet;                            // IP header
                struct icmphdr *icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4)); // ICMP header
                break;                                                                   // Break out of the loop if the packet is received
            }
        }
        gettimeofday(&end, 0);                                                                          // Get the end time to calculate the RTT
        time_to_recv = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f; // Calculate the RTT in milliseconds
        display(&packet, bytes_received);                                                               // Display the ping response
    }
    close(sock); // Close the socket
}