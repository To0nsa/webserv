# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Slowreading.py                                     :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: irychkov <irychkov@student.hive.fi>        +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/05/12 14:38:20 by irychkov          #+#    #+#              #
#    Updated: 2025/08/07 13:03:32 by irychkov         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

import socket
import time

HOST = '127.0.0.1'  # Replace with your server's IP if remote
PORT = 8080         # Adjust to match your server port
REQUEST = b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"

# How many bytes to read per chunk
READ_CHUNK = 1  # Very small to simulate a slow reader
READ_DELAY = 3  # Delay between reads in seconds

def slow_read_client():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        print(f"[+] Connected to {HOST}:{PORT}")
        s.sendall(REQUEST)
        print("[>] Sent HTTP request")

        while True:
            try:
                data = s.recv(READ_CHUNK)
                if not data:
                    print("[x] Server closed the connection")
                    break
                print(f"[<] Received {len(data)} bytes")
                time.sleep(READ_DELAY)
            except socket.error as e:
                print(f"[!] Socket error: {e}")
                break

if __name__ == "__main__":
    slow_read_client()


#std::string big_body(200000000, 'A');
#response.setBody(big_body);
