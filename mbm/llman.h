#include <unistd.h>
#include <map>
#include <string>
#include <set>

const std::string confdir = "/var/bcmlm/";
const std::string mboxdir = "/var/mail/";
const std::string logdir = "/var/log/bcmlm/";
const std::string confile = "/var/bcmlm/listlist.yaml";
const std::string pidlist = "/var/bcmlm/pidlist.txt";

class listListMgr {
    public:
        void read_config(const std::string& = confile);
        void start_list(std::string&);
        void restart_list(pid_t);
        void start_all();
        void cleanup(const std::string& = confdir);

        listListMgr();
    
    private:
        void write_pid_list(const std::string& = pidlist);
        std::map<pid_t,std::string> listOf;
        std::map<std::string,pid_t> pidOf;
        std::set<std::string> lists;
};
