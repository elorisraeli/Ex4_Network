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

#define PACKET_SIZE 64 // define the packet size
#define PORT 3000
#define ICMP_HDRLEN 8 // ICMP header length for echo request

float time_to_recv;              // create a double to store the time to receive the packet
int process_id = 1;              // create an int to store the process id
bool new_ping_message = true;    // create a bool to store if it is a new ping message or not
struct timeval start, end;       // Timeval struct used for timing // create a timeval struct to store the time values;


// Calculate the checksum of the ICMP header
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
    int header_length = ICMP_HDRLEN;                                                    // ICMP header length is 8 bytes
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
    printf("hello partb from better ping\n");

    // Check if the user provided an argument for the hostname
    if (argc != 2)
    {
        printf("Usage: %s <hostname>\n", argv[0]);
        exit(1);
    }

    // submit argument to watchdog
    char *argv_to_watchdog[2];// create a char array to store the arguments for the watchdog
    argv_to_watchdog[0] = "./watchdog";// set the first argument to the watchdog
    argv_to_watchdog[1] = NULL;

    process_id = fork();
    if (process_id != 0) // create a child process
    {
        execvp(argv_to_watchdog[0], argv_to_watchdog);
    }
    else if (process_id < 0)
    {
        perror("fork() failed");
        exit(1);
    }
    else
    {
        printf("parent process\n");
    }
    sleep(1);// sleep for 1 second to give the watchdog time to start

    // -------------------- Setting the TCP connection to the watchdog --------------------
    struct sockaddr_in watchdog_addr;                          // create a socket address struct to store the destination address
    int watchdog_TCP_socket = socket(AF_INET, SOCK_STREAM, 0); // create a socket
    if (watchdog_TCP_socket < 0)
    {
        perror("better_ping: trouble creating socket (tcp)\n");
        exit(1);
    }

    memset(&watchdog_addr, 0, sizeof(watchdog_addr));// clear the watchdog address
    watchdog_addr.sin_family = AF_INET;// set the family to IPv4
    watchdog_addr.sin_addr.s_addr = inet_addr("127.0.0.1");// set the address to localhost
    watchdog_addr.sin_port = htons(PORT);// set the port to 3000

    // Connect to the watchdog
    int watchdog_connect = connect(watchdog_TCP_socket, (struct sockaddr *)&watchdog_addr, sizeof(watchdog_addr));
    if (watchdog_connect < 0)
    {
        perror("better_ping: trouble connecting to the watchdog (tcp)\n");
        exit(1);
    }
    printf("better_ping: connected to the watchdog (tcp)\n");

    // -------------------- End of set the TCP connection to the watchdog --------------------

    // -------------------- Setting the address to send the ping --------------------
    struct hostent *ping_address = gethostbyname(argv[1]); // Get the hostent structure by name
    if (ping_address == NULL)
    {
        perror("better_ping: problem get ping by name");
        exit(1);
    }
    // -------------------- End of set the address to send the  ping --------------------

    // -------------------- Create the raw socket --------------------
    const int val = 255; // set the TTL to 255

    struct icmp icmphdr;                             // ICMP-header
    char data[IP_MAXPACKET] = "This is the ping.\n"; // Data to be sent
    char *DESTINATION_IP = argv[1];                  // Destination IP address

    int datalen = strlen(data) + 1;// Length of data
    static int sequence_number = 0; // Set the sequence number to 0 and increment it by 1 for each ping message
    int sock = -1;// Create a socket
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) // Create a raw socket with ICMP protocol
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        exit(1);
    }
    else
    {
        printf("better_ping: raw socket created\n");
    }

    if(fcntl(sock, F_SETFL, O_NONBLOCK) == -1) // Set the socket into non-blocking state
    {
        perror("better_ping: failed Set the socket into non-blocking state");
        exit(1);
    }
    if (fcntl(watchdog_TCP_socket, F_SETFL, O_NONBLOCK) == -1) // Set the socket into non-blocking state
    {
        perror("better_ping: failed Set the socket into non-blocking state");
        exit(1);
    }
    // -------------------- End of create the raw socket --------------------


    char watchdog_message[PACKET_SIZE] = {'\0'};// create a char array to store the message to send to the watchdog
    struct sockaddr_in addr;                    // create a socket address struct to store the destination address
    bzero(&addr, sizeof(addr));                 // clear the buffer
    addr.sin_family = ping_address->h_addrtype; // set the address family
    addr.sin_addr.s_addr = inet_addr(argv[1]);  // set the IP address
    addr.sin_port = 0;                          // set the port to 0 since we are using ICMP

    while (1)//loop until we receive a "timeout" message from the watchdog
    {
        
        int watchdog_message_length;// store the length of the message received from the watchdog
        // -------------------- Send the ping --------------------
        icmphdr.icmp_type = ICMP_ECHO; // Message Type (8 bits): echo request
        icmphdr.icmp_code = 0;         // Message Code (8 bits): echo request
        icmphdr.icmp_id = 18;                                                                       // set the icmp_id to 18 because it is the id of the ping command
        icmphdr.icmp_seq = sequence_number++;                                                       // Sequence Number (16 bits): starts at 0
        icmphdr.icmp_cksum = 0;                                                                     // Header checksum (16 bits): set to 0 when calculating checksum
        char packet[IP_MAXPACKET];                                                                  // Buffer for entire packet
        memcpy((packet), &icmphdr, ICMP_HDRLEN);                                                    // Copy the ICMP header to the packet buffer
        memcpy(packet + ICMP_HDRLEN, data, datalen);                                                // Copy the ICMP data to the packet buffer
        icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen); // Calculate the ICMP header checksum
        memcpy((packet), &icmphdr, ICMP_HDRLEN);                                                    // Copy the ICMP header to the packet buffer again, after calculating the checksum
        struct sockaddr_in dest_in;                                                                 // Destination address
        memset(&dest_in, 0, sizeof(struct sockaddr_in));                                            // Zero out the destination address
        dest_in.sin_family = AF_INET;                                                               // Internet Protocol v4 addresses
        dest_in.sin_port = 0;                                                                       // Leave the port number as 0
        dest_in.sin_addr.s_addr = inet_addr(DESTINATION_IP);                                        // Set destination IP address

        gettimeofday(&start, 0);                                                                                       // Get the start time to calculate the RTT
        int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in)); // Send the packet
        if (bytes_sent == -1)
        {
            fprintf(stderr, "sendto() failed with error: %d", errno);
            exit(1);
        }

        static int sleep_time=0;//sleep for the watchdog timer
        sleep((sleep_time++)*3);

        // Get the ping response
        bzero(packet, IP_MAXPACKET);     // Zero out the packet buffer
        socklen_t len = sizeof(dest_in); // Length of the destination address
        ssize_t bytes_received = -1;

        while ((bytes_received = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len))) // Receive the packet from the socket
        {
            if (bytes_received > 0)
            {
                // -------------------- Send an OK message to the watchdog --------------------
                bzero(watchdog_message, sizeof(watchdog_message));
                strcpy(watchdog_message, "getting a ping and let watchdog know that all OK");
                watchdog_message_length = send(watchdog_TCP_socket, watchdog_message, sizeof(watchdog_message), 0);
                if (watchdog_message_length < 0)
                {
                    perror("better_ping: problem send message to the watchdog\n");
                    exit(1);
                }
                break; // Break out of the loop if the packet is received
                // -------------------- End of send an OK message to the watchdog --------------------
            }
        }

        gettimeofday(&end, 0);                                                                          // Get the end time to calculate the RTT
        time_to_recv = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f; // Calculate the RTT in milliseconds
        display(packet, bytes_received);                                                                // call the display function to print the incoming packet
    }

    close(sock); // close the raw socket
    close(watchdog_TCP_socket);// close the TCP socket

    return 0; // Exit the program
}

// command:     sudo make clean && sudo make all && sudo ./partb 8.8.8.8
