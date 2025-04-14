#pragma once
#include "library.h"

class BootSector{
    
    private:
        DWORD jumpBoot[3];
        char ID[8];
        DWORD bytesPerSector;
        DWORD sectorsPerCluster;
        DWORD typeMedia;
        DWORD numHead;
        DWORD hiddenSectors;
        DWORDLONG totalSectors;
        DWORDLONG clusterMFT;
        DWORDLONG clusterMFT_Mirror;
        DWORD sizeRecord;
        DWORD sizeIndexBuffer;
        DWORD volumSerial;
        char excutableCode[426];
        DWORD signature;
    public:
        // Constructor
        BootSector() {
            memset(jumpBoot, 0, sizeof(jumpBoot));
            memset(ID, 0, sizeof(ID));
            bytesPerSector = 0;
            sectorsPerCluster = 0;
            typeMedia = 0;
            numHead = 0;
            hiddenSectors = 0;
            totalSectors = 0;
            clusterMFT = 0;
            clusterMFT_Mirror = 0;
            sizeRecord = 0;
            sizeIndexBuffer = 0;
            volumSerial = 0;
            memset(excutableCode, 0, sizeof(excutableCode));
            signature = 0;
        }

        BootSector(
            const DWORD jb[3],
            const char* id,
            DWORD bps,
            DWORD spc,
            DWORD media,
            DWORD heads,
            DWORD hidden,
            DWORDLONG total,
            DWORDLONG mft,
            DWORDLONG mftMirror,
            DWORD recordSize,
            DWORD indexSize,
            DWORD volSerial,
            const char* code,
            DWORD sig
        ) {
            memcpy(jumpBoot, jb, sizeof(jumpBoot));
            strncpy(ID, id, 8);
            bytesPerSector = bps;
            sectorsPerCluster = spc;
            typeMedia = media;
            numHead = heads;
            hiddenSectors = hidden;
            totalSectors = total;
            clusterMFT = mft;
            clusterMFT_Mirror = mftMirror;
            sizeRecord = recordSize;
            sizeIndexBuffer = indexSize;
            volumSerial = volSerial;
            memcpy(excutableCode, code, sizeof(excutableCode));
            signature = sig;
        }

        void printInfo() const {
            cout << "OEM ID: " << string(ID, 8) << "\n";
            cout << "Bytes per Sector: " << bytesPerSector << "\n";
            cout << "Sectors per Cluster: " << sectorsPerCluster << "\n";
            cout << "Media Type: " << typeMedia << "\n";
            cout << "Number of Heads: " << numHead << "\n";
            cout << "Hidden Sectors: " << hiddenSectors << "\n";
            cout << "Total Sectors: " << totalSectors << "\n";
            cout << "MFT Cluster: " << clusterMFT << "\n";
            cout << "MFT Mirror Cluster: " << clusterMFT_Mirror << "\n";
            cout << "Record Size: " << sizeRecord << "\n";
            cout << "Index Buffer Size: " << sizeIndexBuffer << "\n";
            cout << "Volume Serial: " << volumSerial << "\n";
            cout << "Signature: 0x" << hex << signature << dec << "\n";
        }

        // Getter
        const DWORD* getJumpBoot() const { return jumpBoot; }
        const char* getID() const { return ID; }
        DWORD getBytesPerSector() const { return bytesPerSector; }
        DWORD getSectorsPerCluster() const { return sectorsPerCluster; }
        DWORD getTypeMedia() const { return typeMedia; }
        DWORD getNumHead() const { return numHead; }
        DWORD getHiddenSectors() const { return hiddenSectors; }
        DWORDLONG getTotalSectors() const { return totalSectors; }
        DWORDLONG getClusterMFT() const { return clusterMFT; }
        DWORDLONG getClusterMFT_Mirror() const { return clusterMFT_Mirror; }
        DWORD getSizeRecord() const { 
            
            if ((char)sizeRecord < 0) {
                return 1 << abs((char)sizeRecord);
            }
            
            return sizeRecord; 
            
        }
        DWORD getSizeIndexBuffer() const { return sizeIndexBuffer; }
        DWORD getVolumeSerial() const { return volumSerial; }
        const char* getExecutableCode() const { return excutableCode; }
        DWORD getSignature() const { return signature; }

        void readBootSector(string driveName){

            HANDLE drive = CreateFileA(

                driveName.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr

            );

            if (drive == INVALID_HANDLE_VALUE){
                DWORD errCode = GetLastError();
                cout << "Cannot open drive: " << driveName << endl;
                cout << "Error code: " << errCode << endl;
                return;
            }

            BYTE buffer[512];  // boot sector thường 512 byte
            DWORD bytesRead;

            BOOL success = ReadFile(drive, buffer, sizeof(buffer), &bytesRead, nullptr);
            CloseHandle(drive);

            if (!success || bytesRead != 512) {
                cout << bytesRead << endl;
                cout << "Failed to read boot sector data." << endl;
                return;
            }

            memcpy(jumpBoot, buffer + 0x00, 3);
            memcpy(ID, buffer + 0x03, 8);
            bytesPerSector = *reinterpret_cast<WORD*>(buffer + 0x0B);
            sectorsPerCluster = *(buffer + 0x0D);
            typeMedia = *(buffer + 0x15);
            numHead = *reinterpret_cast<WORD*>(buffer + 0x1A);
            hiddenSectors = *reinterpret_cast<DWORD*>(buffer + 0x1C);
            totalSectors = *reinterpret_cast<DWORDLONG*>(buffer + 0x28);
            clusterMFT = *reinterpret_cast<DWORDLONG*>(buffer + 0x30);
            clusterMFT_Mirror = *reinterpret_cast<DWORDLONG*>(buffer + 0x38);

            int8_t szFR = *(int8_t*)(buffer + 0x40);
            sizeRecord = szFR < 0 ? (1 << -szFR) : szFR * sectorsPerCluster * bytesPerSector;

            int8_t szIndex = *(int8_t*)(buffer + 0x44);
            sizeIndexBuffer = szIndex < 0 ? (1 << -szIndex) : szIndex * sectorsPerCluster * bytesPerSector;

            memcpy(&volumSerial, buffer + 0x48, sizeof(volumSerial));
            memcpy(excutableCode, buffer + 0x54, 426);
            signature = *reinterpret_cast<WORD*>(buffer + 0x1FE);

        }

};
