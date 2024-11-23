#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctime>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>
#include <stdlib.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
std::atomic<bool> isRunning(true); // flag for ending sending

SOCKET ConnectSocket = INVALID_SOCKET;

std::string static GenRandom(const int len) {
    srand(time(0));
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}

void static ReceiveMessages() {
    int iResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    while (isRunning) {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            printf("Server message: %s\n", recvbuf);
        }
        else if (iResult == 0) {
            printf("Connection closed by server.\n");
            break;
        }
        else {
            printf("Receive failed: %d\n", WSAGetLastError());
            break;
        }
    }
}

void static SendMessages() {
    int iResult;
    char sendbuf[DEFAULT_BUFLEN];
    while (true) {
        printf(" -> Enter msg: ");
        fgets(sendbuf, sizeof(sendbuf), stdin);

        size_t len = strlen(sendbuf);
        if (len > 0 && sendbuf[len - 1] == '\n') {
            sendbuf[len - 1] = '\0';
        }

        if (strlen(sendbuf) == 0) {
            sendbuf[0] = '\0';
            iResult = send(ConnectSocket, sendbuf, 1, 0);
            if (iResult == SOCKET_ERROR) {
                printf("Send failed: %d\n", WSAGetLastError());
                isRunning = false;
                break;
            }
            isRunning = false;
            break;
        }
        else {
            iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
            if (iResult == SOCKET_ERROR) {
                printf("Send failed: %d\n", WSAGetLastError());
                break;
            }
        }
    }
}


int main()
{
    WSADATA wsaData;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    // Check for errors to ensure that the socket is a valid socket
    if (ConnectSocket == INVALID_SOCKET) {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Connect to server.
    iResult = connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
    }

    // Free the resources returned by getaddrinfo and print an error message
    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    printf("Connected to server.\n");

    std::string sendbufID = GenRandom(20);

    printf("Generated id: %s\n", sendbufID.c_str());

    iResult = send(ConnectSocket, sendbufID.c_str(), 20, 0);
    if (iResult == SOCKET_ERROR) {
        printf("Send failed: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    printf("Bytes sent: %d\n", iResult);
    printf("INSTRUCTIONS:\nEnter message (50 symbols) and '->name' in the end if you want the specific client to be sent the msg.\n");
    
    std::thread recvThread(ReceiveMessages);
    std::thread sendThread(SendMessages);

    sendThread.join();
    recvThread.join();
    
    //// Communication loop
    //while (true) {
    //    char sendbuf[DEFAULT_BUFLEN];
    //    printf("Enter msg: ");
    //    fgets(sendbuf, sizeof(sendbuf), stdin);

    //    // Remove trailing newline from input
    //    size_t len = strlen(sendbuf);
    //    if (len > 0 && sendbuf[len - 1] == '\n') {
    //        sendbuf[len - 1] = '\0';
    //    }
    //    
    //    // Send message to server
    //    if (strlen(sendbuf) == 0) {
    //        // Send '\0' to inform about empty msg
    //        sendbuf[0] = '\0';
    //        iResult = send(ConnectSocket, sendbuf, 1, 0); 
    //    }
    //    else {
    //        iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
    //    }
    //    if (iResult == SOCKET_ERROR) {
    //        printf("Send failed: %d\n", WSAGetLastError());
    //        closesocket(ConnectSocket);
    //        WSACleanup();
    //        break;
    //    }

    //    printf("Bytes sent: %d\n", iResult);

    //    // Receive response from server
    //    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
    //    if (iResult > 0) {
    //        recvbuf[iResult] = '\0'; // Null-terminate the received data
    //        printf("Server response: %s\n", recvbuf);
    //    }
    //    else if (iResult == 0) {
    //        printf("Connection closed by server.\n");
    //        break;
    //    }
    //    else {
    //        printf("Receive failed: %d\n", WSAGetLastError());
    //        break;
    //    }
    //}

    // Cleanup
    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}
