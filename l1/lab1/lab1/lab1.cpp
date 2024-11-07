//Чтение файла несколькими потоками, «сборка» результата.Количество
//потоков, файл для чтения – выбор пользователем.Количество потоков –
//аналогично предыдущему(в т.ч.единственный поток).
//Оценка времени, зависимость от начальных параметров.

#include <iostream>
#include <Windows.h>
#include <iomanip>
#define THREADS_NUM 2
#define BUFFER_SIZE 500000

#include <mutex>

std::mutex coutMutex;
double totalTime = 0;

#include <commdlg.h> // For OpenFileDialog

std::wstring OpenFileDialog() {
    OPENFILENAME ofn;
    wchar_t fileName[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"Text Files\0*.TXT\0";;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"txt";

    if (GetOpenFileName(&ofn)) {
        return std::wstring(fileName);
    }
    else {
        return L"";  // Return an empty string if the user cancels
    }
}

struct ThreadData {
    HANDLE hFile;
    DWORD offset;
    DWORD bytesToRead;
};

DWORD WINAPI ThreadFunc(LPVOID lpParams) {
    ThreadData* data = (ThreadData*)lpParams;

    char buffer[BUFFER_SIZE] = { 0 };
    DWORD bytesRead;
    LARGE_INTEGER frequency;
    LARGE_INTEGER start, end;

    // Получение частоты таймера
    QueryPerformanceFrequency(&frequency);

    // Старт таймера
    QueryPerformanceCounter(&start);

    /*  [in]                HANDLE hFile,
        [in]                LONG   lDistanceToMove,
        [in, out, optional] PLONG  lpDistanceToMoveHigh,
        [in]                DWORD  dwMoveMethod          */
    SetFilePointer(data->hFile, 
                   data->offset, 
                   NULL, 
                   FILE_BEGIN);

    //BOOL ReadFile(
    //    [in]                HANDLE       hFile,
    //    [out]               LPVOID       lpBuffer,
    //    [in]                DWORD        nNumberOfBytesToRead,
    //    [out, optional]     LPDWORD      lpNumberOfBytesRead,
    //    [in, out, optional] LPOVERLAPPED lpOverlapped
    //);
    BOOL result = ReadFile(data->hFile,
        buffer,
        data->bytesToRead,
        &bytesRead,
        NULL);
    QueryPerformanceCounter(&end);

    // Вычисление времени выполнения в секундах
    double elapsedTime = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    totalTime += elapsedTime;

    if (result) {
        coutMutex.lock();

        std::cout << "Thread " << GetCurrentThreadId() << " read: " << buffer << std::endl;

        coutMutex.unlock();
    }
    else {
        std::cout << GetLastError();
    }
    return 0;
}


int main()
{
    while (true)
    {
        totalTime = 0;
        std::wstring filePath = OpenFileDialog();
        if (filePath.empty()) {
            std::cerr << "\n\nNo file selected." << std::endl;
            return 1;
        }

        DWORD threadsNum = 1;
        std::cout << "\n\nEnter number of threads: ";
        std::cin >> threadsNum;

        if (threadsNum <= 0 || threadsNum > 64)
        {
            std::cout << "Number of threads must be in diapazon [1, 64]" << std::endl;
            return 0;
        }

        // Openning file in read mode:
        HANDLE hFile = CreateFile(
            filePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, //FILE_FLAG_OVERLAPPED for asynchronous IO
            NULL
        );
        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "Error opening file" << std::endl;
            return 1;
        }

        DWORD fileSize = GetFileSize(hFile, NULL);
        if (threadsNum > fileSize)
        {
            std::cout << "Number of threads must be smaller than the file size" << std::endl;
            return 0;
        }
        DWORD bytesPerThread = (fileSize + threadsNum - 1) / threadsNum;

        HANDLE* hThreads = new HANDLE[threadsNum];
        ThreadData* threadData = new ThreadData[threadsNum];


        for (int i = 0; i < threadsNum; i++) {
            threadData[i].hFile = hFile;
            threadData[i].offset = i * bytesPerThread;
            threadData[i].bytesToRead = (fileSize - i * bytesPerThread < bytesPerThread) ? (fileSize - i * bytesPerThread) : bytesPerThread;

            /*HANDLE CreateThread(
                [in, optional]  LPSECURITY_ATTRIBUTES   lpThreadAttributes,
                [in]            SIZE_T                  dwStackSize,
                [in]            LPTHREAD_START_ROUTINE  lpStartAddress,
                [in, optional]  __drv_aliasesMem LPVOID lpParameter,
                [in]            DWORD                   dwCreationFlags,
                [out, optional] LPDWORD                 lpThreadId
            );*/
            hThreads[i] = CreateThread(NULL,
                0,
                ThreadFunc,
                &(threadData[i]),
                0,
                NULL);
        }

        /*DWORD WaitForMultipleObjects(
            [in] DWORD        nCount,
            [in] const HANDLE * lpHandles,
            [in] BOOL         bWaitAll,
            [in] DWORD        dwMilliseconds
        );*/
        WaitForMultipleObjects(threadsNum,
            hThreads,
            TRUE,
            INFINITE);

        /*DWORD endTime = GetTickCount();*/

        for (int i = 0; i < threadsNum; i++) {
            CloseHandle(hThreads[i]);
        }
        CloseHandle(hFile);

        std::cout << "\nSize of file: " << fileSize << std::endl;
        std::cout << "Number of threads: " << threadsNum << std::endl;
        std::cout << "Total time taken: " << std::fixed << std::setprecision(7) << totalTime << " ms" << std::endl;
        delete[] hThreads;
        delete[] threadData;
    }

    return 0;
}

