#include <string>
#include <map>

class USPair {
    public:
        std::string email;
        std::string list;
        USPair(std::string &e, std::string& ls) : email(e), list(ls) { }
        USPair(USPair& p) : email(p.email), list(p.list) { }
        USPair() : email(), list() { }
};

class UnsubHandler {
    private:
        std::map<std::string,USPair> tokens;
        unsigned long t_lo, t_hi;
        std::string pendingToken;
        std::string domain;
    
    public:
        UnsubHandler(std::string &domain);
        bool checkToken(std::string &email, std::string& list, std::string& token);
        void createToken(std::string &email, std::string& list);
        void sendToken(std::string &email, std::string& list);
        void unsubEmail(std::string& email, std::string& list);

    private:
        bool getToken(std::string& email, std::string& list, std::string& outTok);
};