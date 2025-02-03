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
struct Client {
    SOCKET socket;
    std::string id; // the username
};

// Global container and mutex for thread safety.
std::vector<Client> currentClients;
std::mutex client_lock;

// Utility: Trim whitespace from both ends of a string.
std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Add a new client (thread-safe).
void AddClient(SOCKET s, const std::string& id) {
    std::lock_guard<std::mutex> lock(client_lock);
    currentClients.push_back({ s, id });
}

// Look up a client by username (returns pointer or nullptr).
Client* FindClientById(const std::string& id) {
    for (auto& client : currentClients) {
        if (client.id == id)
            return &client;
    }
    return nullptr;
}

// Send a private message using the protocol "PRIVATEMSG|fromUser|message\n"
bool SendPrivateMessage(const std::string& fromUser, const std::string& toUser, const std::string& messageContent) {
    std::lock_guard<std::mutex> lock(client_lock);
    Client* targetClient = FindClientById(toUser);
    if (targetClient) {
        std::string formattedMsg = "PRIVATE|" + fromUser + "|" + messageContent + "\n";
        int result = send(targetClient->socket, formattedMsg.c_str(), formattedMsg.size(), 0);
        if (result == SOCKET_ERROR) {
            std::cerr << "Failed to send private message to " << toUser << std::endl;
            return false;
        }
        std::cout << "Sent private message from " << fromUser << " to " << toUser << ": " << messageContent << std::endl;
        return true;
    }
    else {
        std::cerr << "User " << toUser << " not found." << std::endl;
        return false;
    }
}

//  Broadcast a message to all clients.
void Broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(client_lock);
    for (const Client& client : currentClients) {
        // Debug output (can be removed later)
        std::cout << "Sending: " << message;
        send(client.socket, message.c_str(), message.size(), 0);
    }
}

// Broadcast the current user list in the format "USERLIST|user1,user2,...\n"
void BroadcastUserList() {
    std::lock_guard<std::mutex> lock(client_lock);
    std::string userList = "USERLIST|";
    bool first = true;
    for (const Client& client : currentClients) {
        if (!first) {
            userList += ",";
        }
        userList += client.id;
        first = false;
    }
    userList += "\n";
    for (const Client& client : currentClients) {
        send(client.socket, userList.c_str(), userList.size(), 0);
    }
}

// Broadcast a user join event.
void BroadcastUserJoined(const std::string& username) {
    std::string msg = "USERJOIN|" + username + "\n";
    Broadcast(msg);
}

// Broadcast a user left event.
void BroadcastUserLeft(const std::string& username) {
    std::string msg = "USERLEFT|" + username + "\n";
    Broadcast(msg);
}

// The client thread function.
void Controlclients(SOCKET client_socket, const char* client_ip) {
    char buffer[DEFAULT_BUFFER_SIZE];
    // Phase 1: Receive the username from the client.
    int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';
    std::string username = Trim(buffer);
    // Clean up any protocol contamination ¨C take only the first line and remove any '|' characters.
    username = username.substr(0, username.find('\n'));
    username = username.substr(0, username.find('|'));
    username = Trim(username);

    // Add the client to the list.
    AddClient(client_socket, username);
    // Broadcast the new user joining.
    BroadcastUserJoined(username);
    // Send the updated user list to everyone.
    BroadcastUserList();

    // Phase 2: Enter the message handling loop.
    while (true) {
        bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string rawMessage = Trim(buffer);

            // Handle potential multiple messages in one read (packet fragmentation or concatenation)
            std::istringstream stream(rawMessage);
            std::string singleMessage;
            while (std::getline(stream, singleMessage, '\n')) {
                singleMessage = Trim(singleMessage);
                if (singleMessage.empty()) continue;

                // If the message is a private message, it should be in the format:
                // "PRIVATE|recipient|messageContent"
                if (singleMessage.find("PRIVATE|") == 0) {
                    size_t firstSep = singleMessage.find('|', 11); // after "PRIVATE|"
                    if (firstSep != std::string::npos) {
                        std::string toUser = singleMessage.substr(11, firstSep - 11);
                        std::string messageContent = singleMessage.substr(firstSep + 1);
                        // Process private message only.
                        SendPrivateMessage(username, toUser, messageContent);
                        continue;
                    }
                }
                // If the message already has the "BROADCAST|" header, forward it directly.
                else if (singleMessage.find("BROADCAST|") == 0) {
                    // Ensure a newline at the end and forward.
                    Broadcast(singleMessage + "\n");
                }

                // early version
                // Otherwise, treat as a public message and broadcast it.
                //std::string fullMsg = "BROADCAST|" + username + "|" + singleMessage + "\n";
                //Broadcast(fullMsg);
            }
        }
        else {  // Client disconnected or error occurred.
            // Remove the client from the global list.
            {
                std::lock_guard<std::mutex> lock(client_lock);
                auto it = std::remove_if(currentClients.begin(), currentClients.end(),
                    [client_socket](const Client& c) { return c.socket == client_socket; });
                if (it != currentClients.end()) {
                    std::string leavingUser = it->id;
                    currentClients.erase(it, currentClients.end());
                    BroadcastUserLeft(leavingUser);
                    BroadcastUserList();
                    std::cout << "Client " << leavingUser << " disconnected." << std::endl;
                }
            }
            break;
        }
    }
    closesocket(client_socket);
    std::cout << "Connection with " << client_ip << " closed." << std::endl;
}

// The main server loop.
int server() {
    // Step 1: Initialize WinSock.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Step 2: Create a socket.
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Step 3: Bind the socket.
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(65432);  // Server port.
    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address.
    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Step 4: Listen for incoming connections.
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 65432..." << std::endl;

    // Main loop: accept new connections and handle them on separate threads.
    while (true) {
        sockaddr_in client_address = {};
        int client_address_len = sizeof(client_address);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error " << WSAGetLastError() << std::endl;
            continue;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "New connection from " << client_ip << std::endl;

        std::thread(Controlclients, client_socket, client_ip).detach();
    }

    // Cleanup (won't really be reached in an infinite loop).
    closesocket(server_socket);
    WSACleanup();
    return 0;
}

int main() {
    return server();
}