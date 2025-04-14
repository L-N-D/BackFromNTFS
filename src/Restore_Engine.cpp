#include "Restore_Engine.h"

bool restoreClone(string driveName, File file, BootSector bootsector) {

    HANDLE drive = CreateFileA(
        driveName.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (drive == INVALID_HANDLE_VALUE) {
        cout << "Cannot open drive: " << driveName << "\nError code: " << GetLastError() << endl;
        return false;
    }

    WORD recordSize = bootsector.getSizeRecord();
    if ((char)recordSize < 0) {
        recordSize = 1 << abs((char)recordSize);
    }

    DWORD bytesPerCluster = bootsector.getBytesPerSector() * bootsector.getSectorsPerCluster();
    BYTE* buffer = new BYTE[recordSize];
    LARGE_INTEGER offset;
    offset.QuadPart = file.getOffset();

    SetFilePointerEx(drive, offset, nullptr, FILE_BEGIN);
    DWORD bytesRead;
    if (!ReadFile(drive, buffer, recordSize, &bytesRead, nullptr)) {
        cout << "Cannot read MFT record.\nError code: " << GetLastError() << endl;
        delete[] buffer;
        CloseHandle(drive);
        return false;
    }

    // Sửa flag: mark in-use
    buffer[0x16] = 0x01;
    buffer[0x17] = 0x00;

    string recoName = "recovered_" + file.getFileName();
    ofstream out(recoName, ios::binary);
    if (!out) {
        cout << "Cannot create output file: " << recoName << endl;
        delete[] buffer;
        CloseHandle(drive);
        return false;
    }

    if (file.getdataCluster() == 0 && file.getClusterLength() == 0) {
        // === RESIDENT FILE ===
    
        DWORD attrOffset = *reinterpret_cast<WORD*>(buffer + 0x14);
        while (attrOffset < recordSize) {
            DWORD attrType = *reinterpret_cast<DWORD*>(buffer + attrOffset);
            if (attrType == 0xFFFFFFFF) break;
    
            BYTE nonResident = *(buffer + attrOffset + 8);
            if (attrType == 0x80 && nonResident == 0x00) {
                // Tìm nội dung resident
                WORD contentOffset = *reinterpret_cast<WORD*>(buffer + attrOffset + 0x14);
                DWORD contentSize = *reinterpret_cast<DWORD*>(buffer + attrOffset + 0x10);
    
                BYTE* contentPtr = buffer + attrOffset + contentOffset;
                out.write(reinterpret_cast<char*>(contentPtr), contentSize);
                break;
            }
    
            DWORD attrLen = *reinterpret_cast<DWORD*>(buffer + attrOffset + 4);
            if (attrLen == 0) break; // tránh loop vô hạn
            attrOffset += attrLen;
        }
    } else {
        // === NON-RESIDENT FILE ===
        DWORD datacluster = file.getdataCluster();
        ULONGLONG length = file.getClusterLength();
        DWORD bytesPerCluster = bootsector.getBytesPerSector() * bootsector.getSectorsPerCluster();
        BYTE* dataBuffer = new BYTE[bytesPerCluster];
    
        for (ULONGLONG i = 0; i < length; i++) {
            LARGE_INTEGER pos;
            pos.QuadPart = (datacluster + i) * bytesPerCluster;
            SetFilePointerEx(drive, pos, nullptr, FILE_BEGIN);
    
            DWORD bytesRead;
            if (!ReadFile(drive, dataBuffer, bytesPerCluster, &bytesRead, nullptr) || bytesRead != bytesPerCluster) {
                cout << "Failed to read cluster: " << (datacluster + i) << endl;
                continue;
            }
    
            out.write(reinterpret_cast<char*>(dataBuffer), bytesPerCluster);
        }
    
        delete[] dataBuffer;
    }
    
    cout << "[+] File recovered successfully: " << recoName << endl;

    out.close();
    // delete[] dataBuffer;
    delete[] buffer;
    CloseHandle(drive);
    return true;
}