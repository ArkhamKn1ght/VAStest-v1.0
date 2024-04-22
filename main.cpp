#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <cerrno>
#include <vector>
//func to check if provided IP address at the start is valid
bool isValidIPAddress(const std::string& ipAddress) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr)) != 0;
}

int main(int argc, char* argv[]) {
    //checking for arguments
    if(argc != 4) {
        std::cerr << "Not enough arguments.\n Usage: <programm name> <IpAddress> <StartingPort> <EndingPort>" << std::endl;
        return 1;
    }
    //initializing needed variables
    const std::string passedIp    = std::string(argv[1]);
    const int portStart   = std::stoi(std::string(argv[2]));
    const int portEnd     = std::stoi(std::string(argv[3]));
    const int eventsCount = portEnd - portStart + 1;
    std::vector<int> descriptors; // storing socked fd for future cleanup


    if(portStart < 0) {
        std::cerr << "Invalid input(negative port)\n";
        return 1;
    }
    if(portEnd < portStart ) {
        std::cerr << "Invalid input(start port is higher than end port)\n";
        return 1;
    }
    if(portStart > 65535 || portEnd > 65535) {
        std::cerr << "Invalid input(Port exceeding the port limit)\n";
        return 1;
    }
    if(!isValidIPAddress(passedIp)) {
        std::cerr << "Invalid input(Invalid address)\n";
        return 1;
    }
    
    //creating EPOLL and storing its FD
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "epoll_create1 error" << std::endl;
        perror("epoll_create1");
        return 1;
    }

    //creating sockets and attaching them to EPOLL
    for (int i = portStart; i <= portEnd; i++) {
        //creating socket  for each port
        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == -1) {
            std::cerr << "socket error 1" << std::endl;
            perror("socket");
            return 1;
        }

        //creating an IP struct for each socket
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(i); // translating human-read port into machine-read port
        

        //setting up flags
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) {
            std::cout << "fcntl error 1" << std::endl;
            perror("fcntl");
            close(sockfd);
            return 1;
        }
        if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cout << "fcntl error 2" << std::endl;
            perror("fcntl");
            close(sockfd);
            return 1;
        }

        //we dont check for Ip Address' validity, since we do that at the beginning of script
        //if (inet_pton(AF_INET, passedIp.c_str(), &addr.sin_addr) != 1) {
        //std::cerr << "Invalid IP address: " << passedIp << std::endl;
        //return 1;
        //}

        inet_pton(AF_INET, passedIp.c_str(), &addr.sin_addr);
        

        //adding socket to epoll
        struct epoll_event event;
        event.events = EPOLLOUT;
        event.data.fd = sockfd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &event) == -1) {
            std::cout << "epoll_ctl error 1" << std::endl;
            perror("epoll_ctl");
            return 1;
        }
        



        //connect socket
        connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
        descriptors.push_back(sockfd); //add descriptor
        if(errno != EINPROGRESS) {
            perror("connect");
        }

        
    }

    

    struct epoll_event events[eventsCount];
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    // waiting for events
    sleep(2); // since timeout parameter of epoll_wait waits for the first event to occur
    // it doesnt make much sense to set a timeout in epoll_wait. 
    // We could possibly have over 50.000 of sockets, some of them are bound to make events
    // so we call sleep(2) and wait 2 seconds before calling epoll_wait;
    int num_events = epoll_wait(epoll_fd, events, eventsCount, -1);
    
    if(num_events == -1) {
        std::cerr << "epoll_wait error 1" << std::endl;
        perror("epoll_wait");
        return 1;
    }
    int openedPorts = 0;
    for(int i = 0; i < num_events; i++) {
        if((events[i].events & EPOLLOUT) && !(events[i].events & EPOLLERR)) {
            //getting port from the events
            getpeername(events[i].data.fd, 
            (struct sockaddr*)&addr, &len);
            struct sockaddr_in *s = (struct sockaddr_in*)&addr;
            std::cout << "Connection established to port: " <<
                                ntohs(s->sin_port) << std::endl;
            openedPorts++;
        }
    }

    std::cout << "Num events: " << num_events << std::endl;
    std::cout << "Open ports count:" << openedPorts << std::endl; 
    
    //cleanup
    for(int i = 0; i < eventsCount; i++){
        close(descriptors[i]);
    }
    
    close(epoll_fd);
    
    return 0;
}