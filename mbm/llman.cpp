#include "llman.h"
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <signal.h>
#include "mbproc.h"
#include <fcntl.h>
#include <vector>

listListMgr::listListMgr() : listOf(), pidOf(), lists() { }
using namespace std;

void listListMgr::write_pid_list(const std::string& pf) {
    ofstream pidfile(pf);
    for (auto& [pid, list] : listOf) {
        pidfile << pid << endl;
    }
    pidfile.close();
}


void listListMgr::read_config(const std::string& conf) {
    ifstream cfile(confile);
    std::string fline;
    bool listpart = false;
    lists.clear();

    while(std::getline(cfile,fline)) {
        if (fline.starts_with("domain:")) {
            auto domain = fline.substr(8);
            sethostname(domain.c_str(),domain.size());
            listpart = false;
        }
        if (fline.starts_with("date:")) {
            listpart = false;
        }
        if (fline.starts_with("lists:")) listpart = true;
        if (fline.starts_with("  - ") && listpart) {
            auto listname = fline.substr(4,fline.find("#")-4);
            lists.insert(listname);
        }
    }

}

void listListMgr::cleanup(const std::string& dir) {
    for (auto l : lists) {
        if (pidOf.contains(l)) {
            std::string notify_file = dir + l + ".notify";
	    std::ofstream nf(notify_file);
	    nf << endl;
	    nf.close();
        } else {
            start_list(l);
        }
    }

    std::vector<std::string> del_pids;
    for (auto& e : pidOf) {
        if (lists.find(e.first) == lists.end()) {
            kill(e.second,SIGTERM);
            listOf.erase(e.second);
	    del_pids.push_back(e.first);
	}
    }

    for (auto& e : del_pids) pidOf.erase(e);

    write_pid_list();
}

void listListMgr::start_list(std::string& listname) {
    if (pidOf.contains(listname)) {
        kill(pidOf[listname],SIGTERM);
    }
    auto pid = fork();
    if (pid == 0) {
        auto mbp = mbproc(listname);
        mbp.start_proc();
        // Should never reach here, but...
        exit(0);
    } else {
        pidOf[listname] = pid;
        listOf[pid] = listname;
    }
}
        
void listListMgr::restart_list(pid_t pid) {
    if (listOf.contains(pid)) {
        start_list(listOf[pid]);
        write_pid_list();
    }
}

void listListMgr::start_all() {
    for (auto l : lists) start_list(l);
    write_pid_list();
}
