/////////////////////////////////////////////////////////////////
//! This is a independent testing environment of chatroom server.
/////////////////////////////////////////////////////////////////
//#include"Source.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <conio.h>
#include<vector>
#include<mutex>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFFER_SIZE 1024
struct message {
    std::string content;
    std::string client_id;
};

std::atomic<bool> close = false;
std::vector<message> chatHistory; // saves message
std::mutex client_lock;

void Controlclients(SOCKET  client_socket, const char* client_ip) {
    // multithread version of receive
    int connection = 0; // number of connected devices
    char buffer[DEFAULT_BUFFER_SIZE];
    while (true) {
    int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        // identify sender
        std::cout << "Received from client" << buffer << std::endl; // display texture
        message mess;
        mess.client_id = client_ip;
        mess.content = buffer;
        {
            std::lock_guard<std::mutex> lock(client_lock);
            chatHistory.push_back(mess);
        }
        
        // ! reply client for testing!
        std::string response = "Server received: " + std::string(buffer);
        send(client_socket, response.c_str(), response.size(), 0);
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

void Display() {
    // save and display every message in IMGUI

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
    // add while and multithread to enable build more connections.
    while (true) {
        sockaddr_in client_address;
        int client_address_len = sizeof(client_address);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error " << WSAGetLastError() << std::endl;
            continue;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "New connection from " << client_ip << std::endl;

        std::thread(Controlclients, client_socket,client_ip).detach();
    }


    // Step 7: Clean up
  //  closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;

}

int main() {
    server();
}