#ifndef AUTH_H
#define AUTH_H
#include <string>
#include <map>

class AuthCode {
    private:
    std::map<std::string,std::string> tokens;
    std::string domain;
    char state[32];
    int j;
    
    public:
    AuthCode(std::string &domain);
    bool checkToken(const std::string &email, const std::string& token);
    void createToken(std::string &email);
    void createExtraToken(std::string& email);
    void sendToken(std::string &email);
    void sendExtraToken(std::string& email, std::string& query);
    void expireToken(std::string& email);
};

#endif