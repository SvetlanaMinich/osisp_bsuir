#include <windows.h>
#include <iostream>
#include <string>
#include "Common.h"

using namespace std;


void static SendMessageToServer(DWORD clientId, const std::string& text, HANDLE hPipe)
{
	Message msg{};
	msg.clientId = clientId;
	GetSystemTime(&msg.timestamp);
	strncpy_s(msg.text, text.c_str(), sizeof(msg.text));

	DWORD dwWritten;
	WriteFile(hPipe, &msg, sizeof(msg), &dwWritten, NULL);
	/*CloseHandle(hPipe);*/
}

int main()
{
	DWORD clientId = GetCurrentProcessId();
	std::string text;

	HANDLE hPipe = CreateFileA(
		PIPENAME,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (hPipe == INVALID_HANDLE_VALUE) {
		std::cerr << "Connecting to Server failed. Error: " << GetLastError() << std::endl;
		return 1;
	}

	while (true)
	{
		cout << "Enter message ('exit' to stop): ";
		getline(cin, text);

		if (text == "exit") {
			SendMessageToServer(clientId, text, hPipe);  
			break;
		}

		SendMessageToServer(clientId, text, hPipe);
	}

	CloseHandle(hPipe);
	return 0;
}