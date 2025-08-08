#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

int main() {
    signal(SIGPIPE, SIG_IGN);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n";
    std::cout << "Sending header one byte at a time...\n";

    for (size_t i = 0; i < request.size(); ++i) {
        ssize_t sent = send(sock, &request[i], 1, 0);
        if (sent <= 0) {
            perror("send failed");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // 1.5s per byte
    }

    // Final CRLF to complete the headers
    send(sock, "\r\n", 2, 0);

    std::cout << "Finished sending headers.\n";

    // Read response and assert it matches the expected 408 timeout
    std::string response;
    char        buffer[1024];
    int         bytes;

    while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytes);
    }

    if (response.empty()) {
        perror("recv");
        std::cerr << "❌ No response or connection closed unexpectedly.\n";
        close(sock);
        return 1;
    }

    std::cout << "Server response:\n" << response << std::endl;

    if (response.find("HTTP/1.1 408") != std::string::npos) {
        std::cout << "✅ Correctly got 408 Request Timeout\n";
        close(sock);
        return 0;
    }

    std::cerr << "❌ Unexpected response (expected 408):\n" << response << std::endl;
    close(sock);
    return 1;

    close(sock);
    return 0;
}
