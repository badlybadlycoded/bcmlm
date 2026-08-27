#ifndef MLVIEW_H
#define MLVIEW_H
#include <string>
#include <set>
#include <map>

// mailing list
class mlItem {
    std::set<std::string> admins;
    std::map<std::string,std::string> attrs;

    public:
    std::string listname;
    mlItem(std::string& lname) : listname(lname), admins(), attrs() { }
    void getAdmins(std::set<std::string>& s);
    std::string get(std::string& attr);
    void readFile();
};

bool operator<(const mlItem&, const mlItem&);

class mlView {
    std::set<mlItem> sLists;
    std::string domain;

    public:
    mlView() : sLists(), domain() { }
    void readFromFile();
    void addList(std::string& listname);
    void removeList(std::string& listname);
    auto begin() { return sLists.begin(); }
    auto end() { return sLists.end(); }
    bool exists(std::string& ls);
    std::string getDomain() { return domain; }
};
#endif