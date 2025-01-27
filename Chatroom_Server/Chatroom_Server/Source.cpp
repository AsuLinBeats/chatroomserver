#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <conio.h>
#include <mutex>
#include <vector>
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFFER_SIZE 1024

std::atomic<bool> close = false;
// public chat room server
std::vector<SOCKET> allSockets; // save sockets from all client
std::mutex client_mutex; // mutex to lock client thread

void Send(SOCKET  client_socket) {
    int count = 0;
    while (!close) {
        if (_kbhit()) { // non-blocking keyboard input 
            std::cout << "Send(" << count++ << "): ";
            std::string sentence;

            std::getline(std::cin, sentence);

            if (sentence == "!bye") {
                close = true;
                std::cout << "Exiting\n";
            }

            // Send the sentence to the server
            if (send(client_socket, sentence.c_str(), static_cast<int>(sentence.size()), 0) == SOCKET_ERROR) {
                if (close) std::cout << "Terminating\n";
                else std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
                break;
            }
        }
    }
    closesocket(client_socket); // send does closing
}



void Receive(SOCKET client_socket) {
    int count = 0;
    while (!close) {
        // Receive the reversed sentence from the server
        char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
        int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the received data
            std::cout << "Received(" << count++ << "): " << buffer << std::endl;
        }
        else if (bytes_received == 0) {
            std::cout << "Connection closed by server." << std::endl;
        }
        else if (close) {
            std::cout << "Terminating connection\n";
        }

        else {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
        }
        if (strcmp(buffer, "!bye") == 0) {
            close = true;
        }
    }
}
// This function handle connection of client(for accepting multi client using multithreads)
void client(SOCKET client_socket) {
    char buffer[DEFAULT_BUFFER_SIZE]; // buffer of message
    while (true) {
        // receive message from client
        int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break;

        buffer[bytes_received] = '\0';
        std::string message = buffer;

        // 广播消息给所有客户端
        {
            std::lock_guard<std::mutex> lock(client_mutex);
            // get every client socket
            for (SOCKET socket : allSockets) {
                if (socket != client_socket) { // 不发给发送者自己
                    send(socket, message.c_str(), message.size(), 0);
                }
            }
        }
        if (message == "!bye") break;
    }
}


int server() {
    // Step 1: Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Step 2: Create a socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Step 3: Bind the socket
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(65432);  // Server port
    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Step 4: Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 65432..." << std::endl;

    // Step 5: Accept a connection
    sockaddr_in client_address = {};
    int client_address_len = sizeof(client_address);
    SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
    std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;
    std::lock_guard<std::mutex> lock(client_mutex);
    allSockets.push_back(client_socket); // add new client socket to vector


    // Receive(client_socket);
    // Send(client_socket);
    std::thread t1 = std::thread(Send, client_socket);
    std::thread t2 = std::thread(Receive, client_socket);
    std::thread(client, client_socket).detach();

    t1.join();
    t2.join();

    // Step 7: Clean up
  //  closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;

}

int main() {
    server();
}

//
//
//int server() {
//    // Step 1: Initialize WinSock
//    WSADATA wsaData;
//    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
//        return 1;
//    }
//
//    // Step 2: Create a socket
//    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    if (server_socket == INVALID_SOCKET) {
//        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
//        WSACleanup();
//        return 1;
//    }
//
//    // Step 3: Bind the socket
//    sockaddr_in server_address = {};
//    server_address.sin_family = AF_INET;
//    server_address.sin_port = htons(65432);  // Server port
//    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address
//
//    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
//        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
//        closesocket(server_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    // Step 4: Listen for incoming connections
//    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
//        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
//        closesocket(server_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    std::cout << "Server is listening on port 65432..." << std::endl;
//
//    // Step 5: Accept a connection
//    sockaddr_in client_address = {};
//    int client_address_len = sizeof(client_address);
//    SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
//    if (client_socket == INVALID_SOCKET) {
//        std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
//        closesocket(server_socket);
//        WSACleanup();
//        return 1;
//    }
//
//
//    char client_ip[INET_ADDRSTRLEN];
//    inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
//    std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;
//
//    bool isRunning = true;
//    while (isRunning) {
//        // Step 6: Communicate with the client
//        char buffer[1024] = { 0 };
//        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
//
//        //if (buffer == "bye") {
//        //    isRunning = false;
//        //}
//
//        if (bytes_received > 0) {
//            buffer[bytes_received] = '\0';
//            std::cout << "Received: " << buffer << std::endl;
//
//            // Reverse the string
//            std::string response(buffer);
//            std::reverse(response.begin(), response.end());
//
//            // Send the reversed string back
//            send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
//            std::cout << "Reversed string sent back to client." << std::endl;
//        }
//
//    }
//
//
//    // Step 7: Clean up
//    closesocket(client_socket);
//    closesocket(server_socket);
//    WSACleanup();
//
//    return 0;
//}
//
//
//void client_connection(SOCKET client_socket, int id) {
//    // Step 6: Communicate with the client
//    bool stop = false;
//    while (!stop) {
//        char buffer[1024] = { 0 };
//        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
//        if (bytes_received > 0) {
//            buffer[bytes_received] = '\0';
//            std::cout << "Received(" << id << "): " << buffer << std::endl;
//
//            // Reverse the string
//            std::string response(buffer);
//            if (response == "!bye") stop = true;
//            std::reverse(response.begin(), response.end());
//
//            // Send the reversed string back
//            send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
//            std::cout << "Reversed string sent back to client " << id << std::endl;
//        }
//    }
//    // Step 7: Clean up
//    closesocket(client_socket);
//}
//
//int server_loop_multi() {
//    // Step 1: Initialize WinSock
//    WSADATA wsaData;
//    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
//        return 1;
//    }
//
//    // Step 2: Create a socket
//    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    if (server_socket == INVALID_SOCKET) {
//        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
//        WSACleanup();
//        return 1;
//    }
//
//    // Step 3: Bind the socket
//    sockaddr_in server_address = {};
//    server_address.sin_family = AF_INET;
//    server_address.sin_port = htons(65432);  // Server port
//    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address
//
//    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
//        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
//        closesocket(server_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    // Step 4: Listen for incoming connections
//    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
//        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
//        closesocket(server_socket);
//        WSACleanup();
//        return 1;
//    }
//
//    std::cout << "Server is listening on port 65432..." << std::endl;
//
//    int connections = 0;
//    while (true) {
//        // Step 5: Accept a connection
//        sockaddr_in client_address = {};
//        int client_address_len = sizeof(client_address);
//        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
//        if (client_socket == INVALID_SOCKET) {
//            std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
//            closesocket(server_socket);
//            WSACleanup();
//            return 1;
//        }
//
//        char client_ip[INET_ADDRSTRLEN];
//        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
//        std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;
//
//        // accept connection on separate thread
//        //std::thread* t = new std::thread(client_connection, client_socket, ++connections);
//        std::thread t = std::thread(client_connection, client_socket, ++connections);
//        t.detach();
//    }
//    closesocket(server_socket);
//    WSACleanup();
//
//    return 0;
//}
//
//int main() {
//    // server();
//    server_loop_multi();
//}