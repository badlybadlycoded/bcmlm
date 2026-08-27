#include "mlview.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>

using namespace std;

const std::string confdir = "/var/bcmlm/";

void mlItem::getAdmins(std::set<std::string>& s) {
    for (auto e : admins) {
        s.insert(e);
    }
}

std::string mlItem::get(std::string& aname) {
    return attrs[aname];
}

bool operator<(const mlItem& e1, const mlItem& e2) {
    return e1.listname < e2.listname;
}

void mlItem::readFile() {
    std::string confile = confdir + listname + ".yaml";
    ifstream cfile(confile);
    std::string fline;
    std::string section = "admin";
    std::string trailer;
    attrs["senders"] = "";
    attrs["subs"] = "";
    attrs["open"] = "false";
    attrs["ratelimit"] = "1";

    while(std::getline(cfile,fline)) {
        if (fline.starts_with("ratelimit:")) attrs["ratelimit"] = fline.substr(10);
        if (fline.starts_with("open:")) attrs["open"] = fline.substr(5);
        if (fline.starts_with("trailer:")) trailer.append(fline.substr(9) + "\n");
        if (fline.starts_with("admins:")) section = "admin";
        if (fline.starts_with("senders:")) section = "senders";
        if (fline.starts_with("subs:")) section = "subs";
        if (fline.starts_with("  - ")) {
            if (section == "admin") {
                string addr = fline.substr(4,fline.find("#")-4);
                admins.insert(addr);
            } else {
                attrs[section].append(fline + "\n");
            }
        }
    }
    attrs["trailer"] = trailer;
}

void mlView::readFromFile() {
    std::string listfilePath = confdir + "listlist.yaml";
    ifstream listfile(listfilePath);
    std::string fline;
    sLists.clear();
    bool listpart = false;
    while(std::getline(listfile,fline)) {
        if (fline.starts_with("domain:")) domain = fline.substr(8);
        if (fline.starts_with("lists:")) listpart = true;
        if (fline.starts_with("  - ") && listpart) {
            auto listname = fline.substr(4,fline.find("#")-4);
            auto mli = mlItem(listname);
            mli.readFile();
            sLists.insert(mli);
        }
    }
}

void writeListList(std::string& domain, std::set<mlItem>& lists) {
    std::string listfilePath = confdir + "listlist.yaml";
    std::string notifyPath = confdir + "listlist.notify";
    ofstream listfile(listfilePath);
    time_t timestamp;
    time(&timestamp);

    listfile << "domain: " << domain << endl;
    listfile << "date: " << ctime(&timestamp) << endl;
    listfile << "lists:" << endl;
    for (auto mli: lists) {
        listfile << "  - " << mli.listname << endl;
    }
    listfile.close();
    ofstream nf(notifyPath);
    nf << "";
    nf.close();
}

void mlView::addList(std::string& listname) {
    auto mli = mlItem(listname);
    string yamlPath = confdir + listname + ".yaml";
    ofstream yamlFile(yamlPath);
    yamlFile << "listname: " << listname << endl;
    yamlFile.close();
    string mbox = "/var/mail/" + listname;
    stringstream cmd;
    cmd << "useradd -m -G bcmlm,mail " << listname << " ; touch " << mbox 
        << "; chown " + listname + ":bcmlm " << mbox;   
    system(cmd.str().c_str());
    stringstream cmd2;
    cmd2 << "chown " + listname + ":bcmlm " << yamlPath;
    system(cmd2.str().c_str()); 
    chmod(mbox.c_str(),0660);
    chmod(yamlPath.c_str(), 0660);
    mli.readFile();
    sLists.insert(mli);
    writeListList(domain,sLists);
    string logPath = "/var/log/bcmlm/" + listname + ".log";
    ofstream lf(logPath);
    lf << "START OF LOG" << endl;
    lf.close();
    chmod(logPath.c_str(),0666);
}

void mlView::removeList(std::string& listname) {
    for (auto mli : sLists) {
        if (mli.listname == listname) {
            sLists.erase(mli);
            break;
        }
    }
    writeListList(domain,sLists);
}

bool mlView::exists(std::string& listname) {
    mlItem mli(listname);
    return (sLists.find(mli) != sLists.end());
}

