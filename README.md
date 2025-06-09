# 简单的多线程聊天室(服务器端)
基于C++与winsock构建的多线程TCP聊天服务器,用于学习基本的网络通信, 客户端管理与消息通信等知识

## 功能
- **多线程客户端处理** ：每个客户端在单独的线程中进行管理，以便同时进行通信。
- **广播消息**：支持向所有连接的客户端发送公共消息。
- **通知系统**: 用户加入或者离开客户端会收到消息通知音
- **用户列表同步**: 服务器随时记录并更新当前用户

## 消息协议
- BROADCAST|<message> — 向所有用户发送公开消息。
服务器发送:
- PRIVATE|<fromUser>|<message>
- USERLIST|<user1>,<user2>,...
- USERJOIN|<username>
- USERLEFT|<username>

## 技术栈
- C++14
- C++多线程
- Winsock

# English
# Simple Multithreaded Chatroom (Server Side)

A multithreaded TCP chat server built with C++ and Winsock, designed for learning the basics of network communication, client management, and message exchange.

## Features
- **Multithreaded Client Handling**: Each client is managed in a separate thread to enable simultaneous communication
- **Broadcast Messaging**: Supports sending public messages to all connected clients
- **Notification System**: Clients receive notification sounds when a user joins or leaves
- **User List Synchronization**: The server continuously tracks and updates the current list of connected users

## Message Protocol
- `BROADCAST|<message>` — Sends a public message to all users  
Server sends:
- `PRIVATE|<fromUser>|<message>`
- `USERLIST|<user1>,<user2>,...`
- `USERJOIN|<username>`
- `USERLEFT|<username>`

## Tech Stack
- C++14  
- C++ Multithreading  
- Winsock
