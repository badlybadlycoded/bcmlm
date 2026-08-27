#include "mlview.h"
#include <set>
#include <string>
#include <map>
#include <vector>

class RoleView {
    std::set<std::string> pAdmins;
    std::map<std::string, std::set<std::string>> lAdmins;
    mlView &mlv;

    public:
    RoleView(mlView &);
    bool checkUser(std::string& email);
    bool checkPlatformAdmin(std::string &email);
    bool checkListAdmin(std::string &email, std::string& list);
    void getPlatformAdmins(std::vector<std::string>& v);
    void addPlatformAdmin(std::string &email);
    void remPlatformAdmin(std::string &email);
    void rebuild();
};