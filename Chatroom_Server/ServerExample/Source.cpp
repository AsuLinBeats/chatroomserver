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
#include<sstream>
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFFER_SIZE 1024
struct message {
    std::string content;
    std::string client_id;
};
struct Client {
    SOCKET socket;
    std::string id;
};

std::vector<Client> currentClients;
std::atomic<bool> close = false;
std::vector<message> chatHistory; // saves message
std::mutex client_lock;
//
// public chat server
void AddClient(SOCKET s, const std::string& id) {
    std::lock_guard<std::mutex> lock(client_lock);
    currentClients.push_back({ s, id });
}

//
//void Broadcast(const std::string& msg, SOCKET exclude_socket = INVALID_SOCKET) {
//    std::vector<Client> clients_copy;
//    {
//        std::lock_guard<std::mutex> lock(client_lock);
//        clients_copy = currentClients;
//    }
//
//    // send messages to all clients except sender
//    for (const auto& client : clients_copy) {
//        if (client.socket != exclude_socket) {
//            send(client.socket, msg.c_str(), msg.size(), 0);
//        }
//    }
//}





//void Broadcast(const std::string& msg, SOCKET exclude_socket = INVALID_SOCKET) {
//    std::vector<Client> clients_copy;
//    {
//        std::lock_guard<std::mutex> lock(client_lock);
//        clients_copy = currentClients;
//    }
//    for (const auto& client : clients_copy) {
//        if (client.socket != exclude_socket) {
//            send(client.socket, msg.c_str(), msg.size(), 0);
//        }
//    }
//}

void Broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(client_lock);
    for (const Client& client : currentClients) {
        std::cout << "message sent" << message << "\n";
        send(client.socket, message.c_str(), message.size(), 0);
    }
}



std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

//
// Modified Controlclients function
//
void Controlclients(SOCKET client_socket, const char* client_ip) {
    // Add the client (this should already be thread-safe)
    // AddClient(client_socket, client_ip);
    char buffer[DEFAULT_BUFFER_SIZE];

    // Receive the username as the first message
    int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';
    std::string username = Trim(buffer);

    // Optionally remove unwanted characters (for example, if the username has a trailing colon, remove it)
    if (!username.empty() && username.front() == '|')
        username.erase(0, 1);
    if (!username.empty() && username.back() == ':')
        username.pop_back();
    username = Trim(username);

    // Add client with the username
    {
        std::lock_guard<std::mutex> lock(client_lock);
        currentClients.push_back({ client_socket, username });
    }

    std::cout << "Client " << username << " (" << client_ip << ") connected.\n";

    // Now process messages in a loop
    while (true) {
        bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string messageContent = Trim(buffer);

            std::cout << "Received from " << username << ": " << messageContent << std::endl;

            // Check if the message contains a pipe delimiter.
            // Our protocol here is that if the message contains a pipe, the actual content is after it.
            size_t delimPos = messageContent.find('|');
            if (delimPos != std::string::npos) {
                // Use only the part after the pipe as the content.
                messageContent = Trim(messageContent.substr(delimPos + 1));
            }
            // Construct the message in the format "name: content"
            std::string fullMsg = username + "|" + messageContent + "\n";

            // Broadcast the message to all clients (including the sender)
            Broadcast(fullMsg);
        }
        else if (bytes_received <= 0) {
            std::cout << "Client " << client_ip << " disconnected\n";
            {
                std::lock_guard<std::mutex> lock(client_lock);
                currentClients.erase(
                    std::remove_if(currentClients.begin(), currentClients.end(),
                        [client_socket](const Client& c) {
                            return c.socket == client_socket;
                        }),
                    currentClients.end()
                );
            }
            closesocket(client_socket);
            break;
        }
        else if (close) {
            std::cout << "Terminating connection\n";
        }
        else {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
        }
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