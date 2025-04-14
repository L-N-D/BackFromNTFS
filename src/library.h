#pragma once
#include <iostream>
#include <windows.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <winioctl.h>

using namespace std;

class File {
    private:
        string name;
        DWORD size;
        string extend;
        ULONGLONG offset;
        bool residentFlag;
        DWORD dataCluster;
        ULONGLONG clusterLength;
    public:
        File(string name, DWORD size, string extend, ULONGLONG offset, bool isres, DWORD dataCluster, ULONGLONG clusterLength):
        name (name), size(size), extend(extend), offset(offset), residentFlag(isres), dataCluster(dataCluster), clusterLength(clusterLength){};
        string getFileName() {return name;}
        DWORD getFileSize() {return size;}
        string getExtend() {return extend;}
        ULONGLONG getOffset() {return offset;}
        bool isResident() {return residentFlag;}
        DWORD getdataCluster() {return dataCluster;}
        ULONGLONG getClusterLength() {return clusterLength;}
};