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

#define PORT 3000        // define the port number to 3000
#define TIMEOUT 10000       // define the timeout to 10 seconds => 10,000 ms
#define BUFFER_SIZE 1024 // define the buffer size to 1024 bytes

int main(int argc, char *argv[])
{
    printf("hello partb watchdog\n");

    if (argc < 1)
    {
        printf("watchdog cant open well\n");
        exit(1);
    }
    char buffer[BUFFER_SIZE];        // buffer to store the message
    char buffer_replay[BUFFER_SIZE]; // buffer to store the message
    int bytes_res, bind_bytes, listen_bytes;
    int set_socket_addr = 1; // set the socket to reuse the address
    struct timeval start, end; // Timeval struct used for timing // create a timeval struct to store the time values;
    double time_past = 0;      // create a double to store the time past
    struct sockaddr_in server_addr;
    int socket_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0); // create a socket tcp socket
    if (socket_fd < 0)
    {
        perror("watchdog: ERROR opening socket");
        exit(1);
    }
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;         // AF_INET is the address family for IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // copy the server ip address to the server_addr
    server_addr.sin_port = htons(PORT);       // convert the port number to network byte order

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &set_socket_addr, sizeof set_socket_addr) == -1)// set the socket to reuse the address
    {
        perror("setsockopt");
        exit(1);
    }
    // bind the socket to the port
    bind_bytes = bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_bytes < 0)
    {
        perror("watchdog: ERROR on binding socket to port ");
        exit(1);
    }

    // listen for incoming connections
    listen_bytes = listen(socket_fd, 5);
    if (listen_bytes < 0)
    {
        perror("watchdog: ERROR on listening ");
        exit(1);
    }
    printf("watchdog: socket listening \n");

    // accept the connection from the server
    socklen_t server_addr_size = sizeof(server_addr);
    int better_ping_socket = accept(socket_fd, (struct sockaddr *)&server_addr, &server_addr_size);
    if (better_ping_socket < 0)
    {
        perror("watchdog: ERROR on accept");
        exit(1);
    }
    printf("watchdog: connection accepted \n");


     if(fcntl(socket_fd, F_SETFL, O_NONBLOCK) == -1) // Set the socket into non-blocking state
    {
        perror("watchdog: failed Set the socket into non-blocking state");
        exit(1);
    }
    if (fcntl(better_ping_socket, F_SETFL, O_NONBLOCK) == -1) // Set the socket into non-blocking state
    {
        perror("watchdog: failed Set the socket into non-blocking state");
        exit(1);
    }

    // start the loop to send and receive messages
    while (time_past < TIMEOUT)
    {
        bzero(buffer, BUFFER_SIZE); // set the buffer to zero
        gettimeofday(&start, 0);                // Get the start time to calculate the RTT
        bytes_res = 0;                                                   // time is used to get the current time of the system
        bytes_res = recv(better_ping_socket, buffer, sizeof(buffer), 0); // return -1 all the time, roni be quite.
        if (bytes_res > 0)
        {
            gettimeofday(&end, 0);                              // Get the end time to calculate the RTT
        time_past = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f; 
        }
        else
        {
            gettimeofday(&end, 0);    
            time_past += (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;  // time is used to get the current time of the system
        }
    }
    bzero(buffer_replay, BUFFER_SIZE); // set the buffer to zero
    printf("watchdog: timeout \n");
    memcpy(buffer_replay, "timeout", strlen("timeout"));
    bytes_res = send(socket_fd, buffer_replay, strlen(buffer_replay), 0); // send message to server to check if it is alive
    close(socket_fd);                                                     // close the socket
    close(better_ping_socket);                                            // close the socket
    exit(0);                                                              // done the program\

}

