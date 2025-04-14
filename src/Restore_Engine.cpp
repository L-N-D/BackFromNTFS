#include "Restore_Engine.h"

vector<vector<BYTE>> getData(BYTE* buffer, DWORD recordSize) {
    vector<vector<BYTE>> fileData;

    // Get start offset of list attributes
    DWORD attrOffset = *reinterpret_cast<DWORD*>(buffer + 0x14);

    // check attribute list
    while (attrOffset < recordSize) {
        DWORD attrType = *reinterpret_cast<DWORD*>(buffer + attrOffset);
        // end of attribute list
        if (attrType == 0xFFFFFFFF) {
            break;
        }

        // Check for DATA attribute (0x80)
        if (attrType == 0x80) {
            // Check if attribute is resident or non-resident
            BYTE flags = *(buffer + attrOffset + 0x08);
            bool isResident = !(flags & 0x01);  // Bit 0 clear means resident

            if (isResident) {
                // Handle resident attribute
                DWORD contentSize = *reinterpret_cast<DWORD*>(buffer + attrOffset + 0x10);
                WORD contentOffset = *reinterpret_cast<WORD*>(buffer + attrOffset + 0x14);

                // Point to data of file
                BYTE* contentPointer = buffer + attrOffset + contentOffset;

                // Create a vector to store this chunk of data
                vector<BYTE> cache(contentPointer, contentPointer + contentSize);
                fileData.push_back(cache);
            }
            // Non-resident attributes are handled in restoreClone
        }

        // continue to next attribute
        DWORD attrLength = *reinterpret_cast<DWORD*>(buffer + attrOffset + 4);
        if (attrLength == 0) {
            // cout << "Warning: Found attribute with zero length at offset " << attrOffset << endl;
            break;
        }
        attrOffset += attrLength;
    }

    return fileData;
}

bool restoreClone(string driveName, File file, BootSector bootsector) {
    // Open disk
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
        cout << "Unable to open drive " << driveName << endl;
        cout << "Error code: " << GetLastError() << endl;
        return false;
    }

    // Initial base info of volume
    DWORD recordSize = bootsector.getSizeRecord();
    DWORD bytesPerCluster = bootsector.getBytesPerSector() * bootsector.getSectorsPerCluster();

    // Initial buffer for handling data
    BYTE* buffer = new BYTE[recordSize];
    DWORD bytesRead;
    LARGE_INTEGER offset;
    offset.QuadPart = file.getOffset();

    // ========================================= Start restore process ===============================================================
    
    // Read MFT record to get information or data
    SetFilePointerEx(drive, offset, nullptr, FILE_BEGIN);
    if (!ReadFile(drive, buffer, recordSize, &bytesRead, nullptr)) {
        cout << "Unable to read MFT record" << endl;
        cout << "Error: " << GetLastError() << endl;
        delete[] buffer;
        CloseHandle(drive);
        return false;
    }

    // create restore file
    string recoName = "RESTORED_" + file.getFileName();
    ofstream out(recoName, ios::binary);
    if (!out.is_open()) {
        cout << "Unable to create restore file " << recoName << endl;
        delete[] buffer;
        CloseHandle(drive);
        return false;
    }

    bool success = false;

    // handle both resident file and non-resident file
    if (file.isResident()) {
        cout << "Processing resident file: " << file.getFileName() << endl;
        
        // get data of file
        vector<vector<BYTE>> fileData = getData(buffer, recordSize);
        
        if (fileData.empty()) {
            cout << "Warning: No data found for resident file!" << endl;
        } else {
            // Write all data chunks to the output file
            size_t totalBytes = 0;
            for (const auto& chunk : fileData) {
                out.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
                totalBytes += chunk.size();
            }
            success = true;
        }
    } else {
        cout << "Processing non-resident file: " << file.getFileName() << endl;
        
        // Initial some information about data of non-resident file
        DWORD dataCluster = file.getdataCluster();  // the position of start cluster of file
        ULONGLONG length = file.getClusterLength(); // the quantity of clusters which contain data of file

        cout << "Starting cluster: " << dataCluster << ", Cluster length: " << length << endl;

        if (length <= 0) {
            cout << "Warning: File has zero or negative length!" << endl;
        } else {
            // Initial a buffer for data
            BYTE* dataBuffer = new BYTE[bytesPerCluster];

            // write data
            for (ULONGLONG i = 0; i < length; i++) {
                LARGE_INTEGER pos;
                pos.QuadPart = (dataCluster + i) * bytesPerCluster;
                SetFilePointerEx(drive, pos, nullptr, FILE_BEGIN);

                if (!ReadFile(drive, dataBuffer, bytesPerCluster, &bytesRead, nullptr)) {
                    cout << "Restore FAIL at cluster " << dataCluster + i << endl;
                    continue;
                }

                out.write(reinterpret_cast<char*>(dataBuffer), bytesPerCluster);
            }

            delete[] dataBuffer;
            success = true;
        }
    }

    // Clean up
    delete[] buffer;
    out.close();
    CloseHandle(drive);

    if (success) {
        cout << "[+] Restore Completed: " << recoName << endl;
        return true;
    } else {
        cout << "[-] Restore Failed: " << recoName << endl;
        return false;
    }
}