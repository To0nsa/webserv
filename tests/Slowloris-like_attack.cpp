#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <thread>

int main() {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return 1;
	}

	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("connect");
		return 1;
	}

	std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n";
	std::cout << "Sending header one byte at a time...\n";

	for (size_t i = 0; i < request.size(); ++i) {
		send(sock, &request[i], 1, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // 1.5s per byte
	}

	// Final CRLF to complete the headers
	send(sock, "\r\n", 2, 0);

	std::cout << "Finished sending headers.\n";

	// Read response (if any)
	char buffer[1024];
	int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes > 0) {
		buffer[bytes] = '\0';
		std::cout << "Server response:\n" << buffer << std::endl;
	} else {
		std::cout << "No response or connection closed.\n";
	}

	close(sock);
	return 0;
}
