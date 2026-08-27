#include <set>
#include <string>
#include <sys/types.h>
#include <ctime>

class mbproc {
    std::string mlist;
    std::set<std::string> subs;
    std::set<std::string> senders;
    std::set<std::string> admins;
    std::string trailer;
    public:
    typedef struct moveInfo_s {
        char oldEmail[220]; // Max username = 32, max domain name = 252
        char newEmail[220];
        char code[24]; // add a word for the NUL byte 
        struct moveInfo_s *next;
        struct moveInfo_s *prev;
        std::time_t validTime;
        std::time_t startTime;
    } moveInfo; 
    private:
    moveInfo *pendingMoves;
    bool open;
    int rateLimit;

    public:
        mbproc(std::string&);
        void start_proc();

    private:
        void main_loop();
        void process_mailbox(std::string&);
        void handle_email(std::string&);
        void handle_send(std::string&, std::string&);
        void handle_subscribe(std::string&);
        void handle_unsubscribe(std::string&);
        void handle_move_start(std::string&, std::string&);
        void read_config(std::string&);
        void write_config(std::string&);
        void log_message(std::string&, std::string&, std::string&);
};