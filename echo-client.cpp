#include <asm-generic/socket.h>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <format>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

static constexpr char PORT[5] = "5000";
static constexpr int NUM_CONNECTIONS = 10;
static constexpr int MAX_DATA_SIZE = 1024;

void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


struct Connection
{
    int file_descriptor;
    std::string buffer;
};

class EchoClient
{
private:
    //std::vector<int> fds;
    int socket_fd;

public:
    EchoClient();

    void run();
    void terminate();
};

EchoClient::EchoClient()
{
    struct addrinfo hints, *serv_info;
    char s[INET6_ADDRSTRLEN];

    std::memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(
        "localhost", 
        PORT,
        &hints,
        &serv_info
    );
    
    if (status != 0)
        throw std::runtime_error("Error: could not get address info.\n");

    int yes = 1;
    struct addrinfo* p;
    for (p = serv_info; p != nullptr; p = p->ai_next)
    {
        socket_fd = socket(
            p->ai_family, 
            p->ai_socktype, 
            p->ai_protocol
        );

        if (socket_fd == -1)
        {
            std::cerr << "Error: could not create socket_fd socket.\n";
            continue;            
        }

        inet_ntop(
            p->ai_family,
            get_in_addr((struct sockaddr*)p->ai_addr), 
            s, 
            sizeof(s)
        );

        std::cout << std::format("Attempting to connect to {}.\n", s);
                    

        if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            std::cerr << "Error: client could not connect.\n";
            close(socket_fd);
            continue; 
        }

        break;
    }

    freeaddrinfo(serv_info);

    if (p == nullptr)
        throw std::runtime_error("Error: server could not bind.\n");

    inet_ntop(
        p->ai_family,
        get_in_addr((struct sockaddr*)p->ai_addr), 
        s, 
        sizeof(s)
    );

    std::cout << std::format("Connected to {}.\n", s);
}

void EchoClient::run()
{
    std::string input;
    char send_message[MAX_DATA_SIZE];
    char recieved_message[MAX_DATA_SIZE];
    
    while (true)
    {
        std::memset(send_message, 0, sizeof(send_message));
        std::memset(recieved_message, 0, sizeof(recieved_message));

        std::getline(std::cin, input);

        std::memcpy(send_message, input.substr(0, MAX_DATA_SIZE).c_str(), sizeof(send_message));

        if (send(socket_fd, send_message, MAX_DATA_SIZE, 0) == -1)
        {
            std::cerr << "Error: could not send message.\n";
            continue;
        }

        ssize_t num_bytes = recv(socket_fd, recieved_message, MAX_DATA_SIZE, 0);
        if (num_bytes == -1)
        {
            std::cerr << "Error: could not recieve message.\n";
        }
        
        recieved_message[num_bytes] = '\0';
        std::cout << std::format("<Server>:{}\n", recieved_message);
    }
}

void EchoClient::terminate()
{
    close(socket_fd);
}

int main()
{
    try
    {
        EchoClient client;

        client.run();
        client.terminate();
    }
    catch(std::runtime_error& excpt)
    {
        std::cerr << excpt.what() << "\n";
    }
    
}