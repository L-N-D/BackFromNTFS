#include "Restore_Engine.h"

vector <vector<BYTE>> getData(BYTE* buffer, DWORD recordSize){

    vector<vector<BYTE>> fileData;

    // Get start offset of list attributes
    DWORD attrOffset = *reinterpret_cast<DWORD*>(buffer + 0x14);

    // check attribute list
    while (attrOffset < recordSize){

        DWORD attrType = *reinterpret_cast<DWORD*>(buffer + attrOffset);
        // end of attribute list
        if (attrType == 0xFFFFFFFF) {break;}

        if (attrType == 0x80){

            // get offset of data
            WORD contentOffset = *reinterpret_cast<WORD*>(buffer + attrOffset + 0x14);
            DWORD contentSize = *reinterpret_cast<DWORD*>(buffer + attrOffset + 0x10);

            // Point to data of file
            BYTE* contentPointer = buffer + attrOffset + contentOffset;

            vector<BYTE> cache(contentPointer, contentPointer + contentSize);
            fileData.push_back(cache);

        }

        // continue to next attribute
        DWORD attrLength = *reinterpret_cast<DWORD*>(buffer + attrOffset + 4);
        if (attrLength == 0) break;
        attrOffset += attrLength;

    }

    return fileData;

}

bool restoreClone(string driveName, File file, BootSector bootsector){

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
        cout <<"Error code: " << GetLastError() << endl;
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
    
    // Read cluster to get information or data
    SetFilePointerEx(drive, offset, nullptr, FILE_BEGIN);
    if (!ReadFile(drive, buffer, recordSize, &bytesRead, nullptr)){

        cout << "Unable to read MFT record" << endl;
        cout << "Error: " << GetLastError() << endl;
        delete []buffer;
        CloseHandle(drive);
        return false;

    }

    // create restore file
    string recoName = "RESTORED_" + file.getFileName();
    ofstream out(recoName, ios::binary);
    if (!out.is_open()){

        cout << "Unable to create restore file " << recoName << endl;
        delete []buffer;
        CloseHandle(drive);
        return false;

    }

    // handle both resident file and none-resident file
    if (file.isResident()){ 

        // get data of file
        vector<vector<BYTE>> fileData = getData(buffer, recordSize);

        // write to new file
        for (const auto& chunk : fileData) {
            out.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
        }
        
        out.close();
        return true;

    }else{

        // Initial some infomation about data of none-resident file
        DWORD dataCluster = file.getdataCluster();  //the position of start clsuter of file
        ULONGLONG length = file.getClusterLength(); //the quantity of cluster which contain data of file

        // Initial a buffer for data
        BYTE* dataBuffer = new BYTE[bytesPerCluster];

        // write data
        for (ULONGLONG i = 0; i < length; i++){

            LARGE_INTEGER pos;
            pos.QuadPart = (dataCluster + i) * bytesPerCluster;
            SetFilePointerEx(drive, pos, nullptr, FILE_BEGIN);

            if (!ReadFile(drive, dataBuffer, bytesPerCluster, &bytesRead, nullptr)){
                cout << "Restore FAIL at cluster " << dataCluster + i << endl;
                continue;
            }

            out.write(reinterpret_cast<char*>(dataBuffer), bytesPerCluster);

        }

        delete [] dataBuffer;

    }

    cout << "[+] Restore Completely " << recoName << endl;
    out.close();
    CloseHandle(drive);
    return true;

}