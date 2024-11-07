#include <iostream>
#include <Windows.h>
#include <future>
#include <chrono>

using namespace std;


struct Record
{
    DWORD id;
    wchar_t text[32];
};

struct FileMapping 
{
    HANDLE hFile;
    HANDLE hMapping;
    size_t fsize;
    unsigned char* dataPtr;
};


void AddRecord(FileMapping* mapping, const Record& record)
{
    size_t offset = mapping->fsize;

    mapping->fsize += sizeof(Record);
    HANDLE hNewMapping = CreateFileMapping(mapping->hFile, NULL,
                                           PAGE_READWRITE,
                                           0, mapping->fsize, NULL);
    if (hNewMapping == NULL)
    {
        cerr << "CreateFileMapping failed" << endl;
        return;
    }

    UnmapViewOfFile(mapping->dataPtr);
    CloseHandle(mapping->hMapping);

    mapping->hMapping = hNewMapping;
    mapping->dataPtr = (unsigned char*)MapViewOfFile(hNewMapping,
                                                     FILE_MAP_ALL_ACCESS,
                                                     0, 0, mapping->fsize);
    if (mapping->dataPtr == NULL) {
        cerr << "MapViewOfFile failed for new mapping" << endl;
        return;
    }

    CopyMemory(mapping->dataPtr + offset, &record, sizeof(Record));
}

void AddRecords(FileMapping* mapping, const Record records[100], size_t recordCount)
{
    for (size_t i = 0; i < recordCount; i++) {
        AddRecord(mapping, records[i]);
    }
}

Record* GetRecord(FileMapping* mapping, DWORD id)
{
    for (size_t i = 0; i < mapping->fsize; i += sizeof(Record))
    {
        Record* record = (Record*)(mapping->dataPtr + i);
        if (record->id == id)
        {
            return record;
        }
    }
    cout << "Record " << id << " not found" << endl;
    return NULL;
}

void RemoveRecord(FileMapping* mapping, DWORD id)
{
    BOOL recordFound = false;
    for (size_t i = 0; i < mapping->fsize; i += sizeof(Record))
    {
        Record* record = (Record*)(mapping->dataPtr + i);
        if (record->id == id)
        {
            recordFound = true;
        }
        if (recordFound && i + sizeof(Record) < mapping->fsize)
        {
            mapping->fsize -= sizeof(Record);
            for (size_t j = i + sizeof(Record); j < mapping->fsize; j += sizeof(Record))
            {
                Record* next = (Record*)(mapping->dataPtr + j);
                *record = *next;
            }
            break;
        }
    }
    if (recordFound)
    {
        UnmapViewOfFile(mapping->dataPtr);
        SetFilePointer(mapping->hFile,
                       mapping->fsize,
                       NULL, FILE_BEGIN);
        SetEndOfFile(mapping->hFile);

        mapping->hMapping = CreateFileMapping(mapping->hFile, NULL,
                                              PAGE_READWRITE,
                                              0, 0, NULL);
        if (mapping->hMapping == NULL) {
            cerr << "CreateFileMapping failed after deletion" << endl;
            CloseHandle(mapping->hFile);
            return;
        }

        mapping->dataPtr = (unsigned char*)MapViewOfFile(mapping->hMapping,
                                                         FILE_MAP_ALL_ACCESS,
                                                         0, 0, mapping->fsize);
        if (mapping->dataPtr == NULL)
        {
            cerr << "MapViewOfFile failed after deletion" << endl;
            CloseHandle(mapping->hMapping);
            CloseHandle(mapping->hFile);
            return;
        }
        cout << "Record " << id << " removed successfully" << endl;
    }
    else
    {
        cout << "Record " << id << " not found" << endl;
    }
    return;
}

void CloseFileMapping(FileMapping* mapping)
{
    UnmapViewOfFile(mapping->dataPtr);
    CloseHandle(mapping->hMapping);
    CloseHandle(mapping->hFile);
    cout << "FileMapping closed" << endl;
}


int main()
{
    const wchar_t* fileNames[] = {
        L"C:/sem5/osisp/l2/test1.txt",
        L"C:/sem5/osisp/l2/test2.txt",
        L"C:/sem5/osisp/l2/test3.txt"
    };

    HANDLE hFiles[3];
    for (size_t i = 0; i < 3; i++)
    {
        HANDLE hFile = CreateFile(fileNames[i],
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,
                                    NULL,
                                    OPEN_EXISTING, // ERROR_FILE_NOT_FOUND
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            cerr << "CreateFile failed" << endl;
            return 0;
        }
        hFiles[i] = hFile;
    }    
    
    Record firstRecord = Record();
    firstRecord.id = 1;
    wcscpy_s(firstRecord.text, L"Default");
    DWORD bytesWritten;

    for (size_t i = 0; i < 3; i++)
    {
        BOOL writeResult = WriteFile(hFiles[i],
                                    &firstRecord,
                                    sizeof(Record),
                                    &bytesWritten,
                                    NULL);
        if (!writeResult || bytesWritten != sizeof(Record)) {
            cerr << "WriteFile failed" << endl;
            CloseHandle(hFiles[i]);
            return 1;
        }
    }

    DWORD dwFileSizes[3];
    DWORD dwFileSize;
    for (size_t i = 0; i < 3; i++)
    {
        dwFileSize = GetFileSize(hFiles[i], NULL); // if dwFileSize >= 4GB => ERROR
        if (dwFileSize == INVALID_FILE_SIZE) {
            cerr << "GetFileSize failed" << endl;
            CloseHandle(hFiles[i]);
            return 1;
        }
        dwFileSizes[i] = dwFileSize;
    }

    FileMapping* mappings[3];
    for (size_t i = 0; i < 3; i++)
    {
        HANDLE hMapping = CreateFileMapping(hFiles[i], NULL,
                                            PAGE_READWRITE,
                                            0, 0, NULL);
        if (hMapping == NULL) {
            DWORD error = GetLastError();
            cerr << "CreateFileMapping failed with error code: " << error << endl;
            CloseHandle(hFiles[i]);
            return 1;
        }

        unsigned char* dataPtr = (unsigned char*)MapViewOfFile(hMapping,
                                                                FILE_MAP_ALL_ACCESS,
                                                                0, 0, dwFileSize);
        if (dataPtr == NULL)
        {
            cerr << "MapViewOfFile failed" << endl;
            CloseHandle(hMapping);
            CloseHandle(hFiles[i]);
            return 1;
        }

        FileMapping* mapping = (FileMapping*)malloc(sizeof(FileMapping));
        if (mapping == NULL) {
            cerr << "fileMappingCreate - malloc failed" << endl;
            UnmapViewOfFile(dataPtr);
            CloseHandle(hMapping);
            CloseHandle(hFiles[i]);
            return 1;
        }

        mapping->hFile = hFiles[i];
        mapping->hMapping = hMapping;
        mapping->dataPtr = dataPtr;
        mapping->fsize = (size_t)dwFileSize;

        mappings[i] = mapping;
    }

    auto startAsync = chrono::high_resolution_clock::now();
    future<void> adds[3];
    for (size_t i = 0; i < 3; i++)
    {
        Record records[1000];
        for (size_t i = 0; i < 1000; i++)
        {
            records[i] = Record();
            records[i].id = i;
            wcscpy_s(records[i].text, L"Test record");
        }
        future<void> addRecordsFuture = async(launch::async, AddRecords, mappings[i], records, 1000);
        adds[i] = move(addRecordsFuture);
    }
    for (size_t i = 0; i < 3; i++)
    {
        adds[i].wait();
    }
    auto endAsync = chrono::high_resolution_clock::now();
    chrono::duration<double> durationAsync = endAsync - startAsync;
    cout << "Time taken for async writes: " << durationAsync.count() << " seconds" << endl;


    startAsync = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 3; i++)
    {
        Record records[1000];
        for (size_t i = 0; i < 1000; i++)
        {
            records[i] = Record();
            records[i].id = i;
            wcscpy_s(records[i].text, L"Test record");
        }
        AddRecords(mappings[i], records, 1000);
    }
    endAsync = chrono::high_resolution_clock::now();
    durationAsync = endAsync - startAsync;
    cout << "Time taken WITHOUT async writes: " << durationAsync.count() << " seconds" << endl;

    /*future<Record*> getRecordFuture = async(launch::async, GetRecord, mapping, 1);
    Record* readRecord = getRecordFuture.get();

    if (readRecord)
    {
        wcout << "Record read: " << readRecord->text << endl;
    }
    else
    {
        return 1;
    }

    future<void> removeRecordFuture = async(launch::async, RemoveRecord, mapping, 1);
    removeRecordFuture.get();

    future<void> closeFileMappingFuture = async(launch::async, CloseFileMapping, mapping);
    closeFileMappingFuture.wait();*/
    
    return 0;
}
