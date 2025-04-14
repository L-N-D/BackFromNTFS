#include <iostream>
#include "BootSector.h"
#include "Scan.h"
#include "Restore_Engine.h"

void printBootSectorInfo(BootSector bootsector){

    cout << "==================================" << endl;
    cout << "|         BOOT SECTOR INFO       |" << endl;
    cout << "==================================" << endl;
    bootsector.printInfo();
    cout << "==================================" << endl;

}

void printDeleted(vector<File> deletedFiles){

    if (deletedFiles.empty()) {
        cout << "No deleted files found." << endl;
        return;
    }

    cout << "\nFound " << deletedFiles.size() << " deleted file(s).\n" << endl;

    for (size_t i = 0; i < deletedFiles.size(); ++i) {
        cout << "[" << i + 1 << "]. " << deletedFiles[i].getFileName() << " (" << deletedFiles[i].getExtend() << ") " << deletedFiles[i].getOffset() << " | " << deletedFiles[i].getClusterLength() << endl;
    }

}

int main() {

    string disk;
    while (true){
        cout << "Input disk (C, D, E, ...): ";
        
        getline(cin, disk);
        if (disk.size() > 1){
            cout << "Invalid disk!" << endl;
        }else{

            if (!disk.empty()) {
                disk[0] = toupper(disk[0]);
                break;
            }

        }

    }

    string driveName = "\\\\.\\" + disk +":";
    BootSector bootsector;
    bootsector.readBootSector(driveName);
    // display bootsector info
    printBootSectorInfo(bootsector);

    // get deleted files list
    vector<File> deletedFiles = ScanDeleted(driveName, bootsector);

    vector<string> options = {"scan", "printbootsector", "help", "printdeleted", "changedisk", "@@exit"};

    bool running = true;
    while (running){

        string command;

        cout << "=================================================" << endl;
        cout << "Input command [input help to display all commands]: ";
        getline(cin, command);

        for (int i = 0; i < options.size(); i++){
            if (options[i] == command){

                switch (i) {

                    case 0:{
                        deletedFiles = ScanDeleted(driveName, bootsector);
                        break;
                    }
                    case 1:{
                        printBootSectorInfo(bootsector);
                        break;
                    }
                    case 2:{
                        cout << "Command list:" << endl;
                        for (int j = 0; j < options.size(); j++){
                            cout << options[i] << endl;
                        }
                        cout << endl;
                        break;
                    }
                    case 3:{
                        printDeleted(deletedFiles);
                        cout << "Do you want to restore any file in this lisk? (y/n): ";
                        char choice;
                        cin >> choice;
                        if (choice == 'y' | choice == 'Y'){

                            while (true){

                                string input;
                                cout << "Input file index [Input @@exit to exit]: ";
                                getline(cin, input);
                                
                                if (input == "@@exit"){
                                    break;
                                }

                                int index = stoi(input);
                                if (index >= 1 && index <= (int)deletedFiles.size()) {
                                    if (restoreClone(driveName, deletedFiles[index - 1], bootsector)){
                                        cout << " [+] Restore file successfully" << endl;
                                    }else{
                                        cout << "[-] Fail to restore file" << endl;
                                    }
                                } else {
                                    cout << "Invalid index." << endl;
                                }
                                
                            }
                            
                        }
                        break;
                    }
                    case 4:{
                        string oldDisk = driveName;
                        cout << "Input new disk: ";
                        getline(cin, disk);
                        driveName = "\\\\.\\" + disk +":";
                        bootsector.readBootSector(driveName);
                        deletedFiles = ScanDeleted(driveName, bootsector);
                        cout << "Complete change from " << oldDisk << "to " << driveName << endl;
                        break;
                    }
                    case 5:{
                        cout << "Exiting ..." << endl;
                        running = false;
                        break;
                    }
                    default:{
                        cout << "Invalid command" << endl;
                    }

                }

            }
        }

    }

    system("pause");
}