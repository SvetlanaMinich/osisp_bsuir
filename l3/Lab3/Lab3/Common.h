#pragma once
#include <windows.h>

LPCSTR PIPENAME = "\\\\.\\pipe\\sp1";

struct Message {
    DWORD clientId;
    SYSTEMTIME timestamp;
    char text[256];
};