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
#include <unistd.h>

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

class EchoServer
{
private:
    //std::vector<int> fds;
    int acceptor;

public:
    EchoServer();

    void run();
    void terminate();
};

EchoServer::EchoServer()
{
    struct addrinfo hints, *serv_info;

    std::memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(
        nullptr, 
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
        acceptor = socket(
            p->ai_family, 
            p->ai_socktype, 
            p->ai_protocol
        );

        if (acceptor == -1)
        {
            std::cerr << "Error: could not create acceptor socket.\n";
            continue;            
        }

        if (setsockopt(
            acceptor, 
            SOL_SOCKET, 
            SO_REUSEADDR, 
            &yes, 
            sizeof(int)
        ) == -1)
            throw std::runtime_error("Error: could not set acceptor socket options.\n");

        if (bind(
            acceptor,
            p->ai_addr,
            p->ai_addrlen
        ) == -1)
        {
            close(acceptor);
            std::cerr << "Error: server could not bind.\n";
            continue;
        }

        break;
    }

    freeaddrinfo(serv_info);

    if (p == nullptr)
        throw std::runtime_error("Error: server could not bind.\n");

    if (listen(acceptor, NUM_CONNECTIONS) == -1)
        throw std::runtime_error("Error: server could not prepare to listen for connections.\n");

    struct sigaction sig_action;

    sig_action.sa_handler = [](int s) -> void
    {
        (void)s; 

        int saved_errno = errno;

        while(waitpid(-1, NULL, WNOHANG) > 0);

        errno = saved_errno;
    };
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sig_action, nullptr) == -1)
        throw std::runtime_error("Error: sigaction.\n");
}

void EchoServer::run()
{
    char s[INET6_ADDRSTRLEN];
    char message[MAX_DATA_SIZE];
    
    while (true)
    {
        struct sockaddr_storage client_addr;
        socklen_t size = sizeof(client_addr);

        int client_fd = accept(acceptor, (struct sockaddr*)&client_addr, &size);

        if (client_fd == -1)
        {
            std::cerr << "Error: could not accept client.\n";
            continue;
        }

        inet_ntop(
            client_addr.ss_family, 
            get_in_addr((struct sockaddr*)&client_addr), 
            s, 
            sizeof(s)
        );

        std::cout << std::format("Server got connection from {}.\n", s);
        
        pid_t pid = fork();
        while (pid == 0)
        {
            close(acceptor);


            ssize_t msg_len = recv(
            client_fd,
            message,
            1024,
            0
            );

            if (msg_len == -1)
            {
                std::cout << "Error: could not recieve.\n";
                continue;
            }
            else if (msg_len == 0)
            {
                std::cout << std::format("Connection from {} closed.\n", s);
                close(client_fd);
                exit(EXIT_SUCCESS);
                break;
            }

            message[msg_len] = '\0';

            send(
                client_fd,
                message,
                msg_len,
                0
            );
        }
       
        close(client_fd);
    }
}

void EchoServer::terminate()
{
    close(acceptor);
}

int main()
{
    try
    {
        EchoServer server;

        server.run();
        server.terminate();
    }
    catch (std::runtime_error& excpt)
    {
        std::cerr << excpt.what() << "\n";
    }
}