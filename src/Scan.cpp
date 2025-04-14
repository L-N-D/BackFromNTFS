#include "Scan.h"

bool isDeleted(WORD flag){
    return (flag & 0x01) == 0;  // flag = 0x00 -> deleted | flag = 0x01 -> in use
}

// Function to extract name of file in a record in MFT
string getFileName(BYTE* record, DWORD recordSize){

    DWORD attrOffset = *(WORD*)(record + 0x14);
    while (attrOffset < recordSize) {
        DWORD type = *(DWORD*)(record + attrOffset);
        if (type == 0x30) {
            BYTE nameLen = *(record + attrOffset + 0x58);
            WCHAR* namePtr = (WCHAR*)(record + attrOffset + 0x5A);
            wstring wname(namePtr, nameLen);
            return string(wname.begin(), wname.end());
        }
        if (type == 0xFFFFFFFF) break;
        attrOffset += *(DWORD*)(record + attrOffset + 4);
    }
    return "";

}

string getExtension(const string& name) {
    size_t pos = name.find_last_of('.');
    if (pos != string::npos && pos < name.size() - 1)
        return name.substr(pos);
    return "unknown";
}

DWORD getFileSize(BYTE* record, DWORD recordSize) {

    DWORD attrOffset = *(WORD*)(record + 0x14);
    while (attrOffset < recordSize) {
        DWORD type = *(DWORD*)(record + attrOffset);
        if (type == 0x80) {
            return *(DWORD*)(record + attrOffset + 0x10);
        }
        if (type == 0xFFFFFFFF) break;
        attrOffset += *(DWORD*)(record + attrOffset + 4);
    }
    return 0;

}

DWORD getDataCluster(BYTE* record, DWORD recordSize) {

    DWORD attrOffset = *(WORD*)(record + 0x14);
    while (attrOffset < recordSize) {
        DWORD type = *(DWORD*)(record + attrOffset);
        if (type == 0x80) {
            BYTE flag = *(record + attrOffset + 0x08);
            if (flag == 0x01) {
                WORD dataRunOffset = *(WORD*)(record + attrOffset + 0x20);
                BYTE* dataRun = record + attrOffset + dataRunOffset;
                BYTE header = *dataRun;
                if (header == 0x00) return 0;

                BYTE lenSize = header & 0x0F;
                BYTE offsetSize = header >> 4;

                ULONGLONG clusterStart = 0;
                for (int i = 0; i < offsetSize; i++) {
                    clusterStart |= ((ULONGLONG)*(dataRun + 1 + lenSize + i)) << (i * 8);
                }
                if (offsetSize > 0 && (*(dataRun + 1 + lenSize + offsetSize - 1) & 0x80)) {
                    clusterStart |= ~((1ULL << (offsetSize * 8)) - 1);
                }
                return (DWORD)clusterStart;
            }
        }
        if (type == 0xFFFFFFFF) break;
        attrOffset += *(DWORD*)(record + attrOffset + 4);
    }
    return 0;

}

ULONGLONG getClusterLength(BYTE* record, DWORD recordSize) {

    DWORD attrOffset = *(WORD*)(record + 0x14);
    ULONGLONG total = 0, current = 0;
    while (attrOffset < recordSize) {
        DWORD type = *(DWORD*)(record + attrOffset);
        if (type == 0x80) {
            if (*(BYTE*)(record + attrOffset + 0x08) != 0x01) return 0;
            WORD runOffset = *(WORD*)(record + attrOffset + 0x20);
            BYTE* runlist = record + attrOffset + runOffset;

            int i = 0;
            while (runlist[i] != 0x00) {
                BYTE header = runlist[i++];
                BYTE lenSize = header & 0x0F;
                BYTE offSize = header >> 4;

                ULONGLONG len = 0;
                LONGLONG offset = 0;

                for (int j = 0; j < lenSize; j++) {
                    len |= ((ULONGLONG)runlist[i++]) << (j * 8);
                }
                for (int j = 0; j < offSize; j++) {
                    offset |= ((ULONGLONG)runlist[i]) << (j * 8);
                    i++;
                }

                if (offSize > 0 && (runlist[i - 1] & 0x80)) {
                    offset |= -((LONGLONG)1LL << (offSize * 8));
                }

                current += offset;
                total += len;
            }
            return total;
        }
        if (type == 0xFFFFFFFF) break;
        attrOffset += *(DWORD*)(record + attrOffset + 4);
    }
    return 0;

}

ULONGLONG getParentDirectoryID(BYTE* record, DWORD recordSize) {
    
    DWORD attrOffset = *(WORD*)(record + 0x14);
    while (attrOffset < recordSize) {
        DWORD type = *(DWORD*)(record + attrOffset);
        if (type == 0x30) { 
            return *(ULONGLONG*)(record + attrOffset + 0x00);
        }
        if (type == 0xFFFFFFFF) break;
        attrOffset += *(DWORD*)(record + attrOffset + 4);
    }
    return 0;

}

bool getResident(BYTE* record, DWORD recordSize){

    DWORD attrOffset = *reinterpret_cast<WORD*>(record + 0x14);
    while (attrOffset < recordSize) {
        DWORD attrType = *reinterpret_cast<DWORD*>(record + attrOffset);
        if (attrType == 0xFFFFFFFF) break;

        BYTE nonResident = *(record + attrOffset + 8);

        if (attrType == 0x10 && nonResident == 0x00) {
            return true;
        }

        DWORD attrLen = *reinterpret_cast<DWORD*>(record + attrOffset + 4);
        if (attrLen == 0) break;

        attrOffset += attrLen;
    }
    return false;

}

// Scan all deleted file
vector<File> ScanDeleted(string driveName, BootSector bootsector) {
    vector<File> list;
    HANDLE drive = CreateFileA(driveName.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (drive == INVALID_HANDLE_VALUE) return list;

    DWORD recordSize = bootsector.getSizeRecord();
    if ((char)recordSize < 0) recordSize = 1 << abs((char)recordSize);
    DWORD bytesPerCluster = bootsector.getBytesPerSector() * bootsector.getSectorsPerCluster();
    ULONGLONG offset = bootsector.getClusterMFT() * bytesPerCluster;
    BYTE* buffer = new BYTE[recordSize];

    int stopCnt = 0;
    while (true) {
        LARGE_INTEGER pos; pos.QuadPart = offset;
        SetFilePointerEx(drive, pos, nullptr, FILE_BEGIN);
        DWORD read;
        if (!ReadFile(drive, buffer, recordSize, &read, nullptr) || read != recordSize) break;

        if (memcmp(buffer, "FILE", 4) != 0) {
            if (++stopCnt > 8) break;
            offset += recordSize;
            continue;
        }

        stopCnt = 0;
        ULONGLONG parentID = getParentDirectoryID(buffer, recordSize);
        WORD flag = *(WORD*)(buffer + 0x16);
        if (isDeleted(flag)) {
            string name = getFileName(buffer, recordSize);
            string ext = getExtension(name);
            DWORD size = getFileSize(buffer, recordSize);
            bool isres = getResident(buffer, recordSize);
            DWORD cluster = getDataCluster(buffer, recordSize);
            ULONGLONG count = getClusterLength(buffer, recordSize);
            // cout << offset << endl;
            if (!name.empty()) list.emplace_back(name, size, ext, offset, isres, cluster, count);
        }
        offset += recordSize;
    }

    delete[] buffer;
    CloseHandle(drive);
    return list;
}