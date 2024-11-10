#include <iostream>
#include <Windows.h>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include "Common.h"

#define FILEN "log.txt"

using namespace std;

std::mutex logMutex;

static void WriteLogEntry(HANDLE hFile, const Message& msg) {
    lock_guard<mutex> lock(logMutex);

    ostringstream oss;
    oss << "Client ID: " << msg.clientId
        << ", Time: " << msg.timestamp.wHour << ":"
        << msg.timestamp.wMinute << ":"
        << msg.timestamp.wSecond
        << ", Message: " << msg.text << "\n";

    string logEntry = oss.str();
    DWORD bytesWritten;

    BOOL success = WriteFile(
        hFile,
        logEntry.c_str(),
        static_cast<DWORD>(logEntry.size()),
        &bytesWritten,
        NULL
    );

    if (!success) {
        cerr << "Error writing to file: " << GetLastError() << endl;
    }

    cout << logEntry.c_str() << endl;
}

void static HandleClient(HANDLE hPipe, HANDLE hFile) {
    DWORD dwRead;
    Message msg{};

    while (ReadFile(hPipe, &msg, sizeof(msg), &dwRead, NULL)) {
        if (string(msg.text) == "exit") {
            cout << "Client " << msg.clientId << " disconnected." << std::endl;
            break;
        }

        WriteLogEntry(hFile, msg);
    }

    CloseHandle(hPipe);
}

int main()
{
    vector<thread> clientThreads;
    BOOL fConnected = FALSE;
    DWORD dwRead = 0;

    HANDLE hFile = CreateFileA(FILEN,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Log file cannot be opened. Error: " << GetLastError() << endl;
        return 1;
    }

    cout << "Server is listening..." << endl;
    while (true)
    {
        // Create a new named pipe instance for each client
        HANDLE hPipe = CreateNamedPipeA(
            PIPENAME,
            PIPE_ACCESS_INBOUND,  // read only
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            0,
            sizeof(Message),
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to create named pipe. Error: " << GetLastError() << std::endl;
            continue;
        }

        BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (fConnected) {
            std::cout << "Client connected." << std::endl;

            clientThreads.emplace_back(HandleClient, hPipe, hFile);
        }
        else {
            CloseHandle(hPipe);
        }
    }

    for (auto& th : clientThreads) {
        if (th.joinable()) {
            th.join();
        }
    }

    CloseHandle(hFile);
    return 0;
}