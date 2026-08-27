#include "roles.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <filesystem>

const std::string confdir = "/var/bcmlm/";

using namespace std;

void readAdminFile(set<string>& pset) {
    string amFilePath = confdir + "siteAdmins.yaml";
    ifstream amFile(amFilePath);
    string fline;
    bool adminpart;
    while(getline(amFile,fline)) {
        if (fline.find(':') != string::npos) adminpart = false;
        if (fline.starts_with("admins:")) adminpart = true;
        if (fline.starts_with("  - ") and adminpart) {
            string addr = fline.substr(4,fline.find('#')-4);
            pset.insert(addr);
        }
    }
    amFile.close();
} 

RoleView::RoleView(mlView &m) : mlv(m), pAdmins(), lAdmins() {
    readAdminFile(pAdmins);
    for(auto mli : m) {
        std::set<string> mla;
        mli.getAdmins(mla);
        for (auto addr : mla) {
            if (!lAdmins.contains(addr)) {
                set<string> s;
                lAdmins[addr] = s;
            }
            lAdmins[addr].insert(mli.listname);
        }
    }
}

void RoleView::getPlatformAdmins(vector<string>& v) {
    for (auto a : pAdmins) v.push_back(a);
}

bool RoleView::checkPlatformAdmin(string& addr) {
    return pAdmins.find(addr) != pAdmins.end();
}

bool RoleView::checkListAdmin(string& email, string& ls) {
    if (!lAdmins.contains(email)) return false;
    auto s = lAdmins[email];
    return s.find(ls) != s.end();
}

bool RoleView::checkUser(string& email) {
    return (pAdmins.find(email) != pAdmins.end() || lAdmins.contains(email));
}

void RoleView::addPlatformAdmin(string &email) {
    pAdmins.insert(email);
    string amFilePath = confdir + "siteAdmins.yaml";
    ofstream amFile(amFilePath);
    time_t timestamp;
    time(&timestamp);

    amFile << "domain: " << mlv.getDomain() << endl;
    amFile << "date: " << ctime(&timestamp) << endl;
    amFile << "admins:" << endl;
    for (auto e : pAdmins) {
        amFile << "  - " << e << endl;
    }
    amFile.close();
}

void RoleView::remPlatformAdmin(string& email) {
    if (pAdmins.find(email) == pAdmins.end()) return;
    pAdmins.erase(email);
    cerr << "removing admin: " << email << endl;
    string amFilePath = confdir + "siteAdmins.yaml";
    ifstream amFile(amFilePath);
    char tmpAdmCh[32];
    strcpy(tmpAdmCh,"/tmp/saXXXXXX");
    int tfd = mkstemp(tmpAdmCh);
    std::string tmpAdmPath(tmpAdmCh);
    close(tfd);
    ofstream tmpFile(tmpAdmPath);
    string fline;
    while(getline(amFile,fline)) {
        if (fline.find(email) == string::npos) 
            tmpFile << fline << endl;
    }
    amFile.close();
    tmpFile.close();
    filesystem::copy(tmpAdmPath,amFilePath,filesystem::copy_options::overwrite_existing);
    filesystem::remove(tmpAdmPath);
}

void RoleView::rebuild() {
    lAdmins.clear();
    mlv.readFromFile();
    for(auto mli : mlv) {
        std::set<string> mla;
        mli.getAdmins(mla);
        for (auto addr : mla) {
            if (!lAdmins.contains(addr)) {
                set<string> s;
                lAdmins[addr] = s;
            }
            lAdmins[addr].insert(mli.listname);
        }
    }
}
