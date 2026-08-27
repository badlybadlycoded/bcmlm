#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include "llman.h"
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

using namespace std;

void cleanup() {
    ifstream pidfile("/var/bcmlm/pidlist.txt");
    std::string pline; 
    while(std::getline(pidfile,pline)) {
        pid_t pid = stoi(pline);
        kill(pid,SIGTERM);
    }
    pidfile.close();
    ofstream pf("/bar/bcmlm/pidlist.txt");
    pf.close();
}

void monitor_process() {
    while(true) {
        sleep(1);
        if (access("/var/bcmlm/listlist.notify",F_OK) == 0) {
            unlink("/var/bcmlm/listlist.notify");
            exit(-1);
        }
    }
}

int main(int argc, char **argv) {
    listListMgr llman;
    cleanup();
    pid_t monitor_pid = -1;
    llman.read_config();
    llman.start_all();
    while (true) {
        if (monitor_pid < 0) {
            monitor_pid = fork();
            if (monitor_pid == 0) {
                monitor_process();
                // should never get here...
                exit(-1);
            }
        }
        auto spid = wait(NULL);
        if (spid == monitor_pid) {
            monitor_pid = -1;
            llman.read_config();
            llman.cleanup();
        } else {
            llman.restart_list(spid);
        }
    }
    return 0;
}
