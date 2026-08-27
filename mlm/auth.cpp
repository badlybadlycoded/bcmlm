#include "auth.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>

const std::string confdir = "/var/bcmlm/";
static const char sub[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
using namespace std;

void stepRNG(char state[], int &j) {
    for(int i = 0; i < 32; i++) {
        j = (j + state[i]) & 0x1F;
        int s = state[j];
        state[j] = state[i];
        state[i] = s; 
    }
}

AuthCode::AuthCode(std::string& dom) : domain(dom), tokens() {
    // initialize token RNG
    for(int i = 0; i < 32; i++) state[i] = i;
    j = 0;
    int k;
    int urand = open("/dev/urandom",O_RDONLY);
    // read one long int into both t_lo and t_hi
    for(int i = 0; i < 32; i++) {
        read(urand, &k, 1);
        j = (j + state[i] + k) & 0x1F;
        k = state[j];
        state[j] = state[i];
        state[i] = k;
    }
    close(urand);
}

bool AuthCode::checkToken(const std::string &email, const std::string& token) {
    if (tokens.contains(email)) return tokens[email] == token;
    return false;
}

void AuthCode::createToken(std::string& email) {
    stepRNG(state, j);
    std::string tokStr;
    // base32 encode the token
    for(int i = 0; i < 32; i++) tokStr += sub[state[i]];
    tokens[email] = tokStr;
}

void AuthCode::createExtraToken(std::string& email) {
    string emStr = email;
    emStr += "+extra";
    createToken(emStr);
}


void AuthCode::sendToken(std::string& email) {
    if (!tokens.contains(email)) return;
    string tokSend = tokens[email];
    std::string tmpEmailPath = "/tmp/" + tokSend + ".txt";
    std::ofstream tmpEmail(tmpEmailPath);
    std::string safeEmail(email);
    auto pos = string::npos;
    while((pos = safeEmail.find('@'))!=string::npos) {
        safeEmail.replace(pos,1,"%40");
    }
    tmpEmail << "To complete your login for the BCMLM manager for " << domain <<
        ", please visit the following link:" << endl;
    tmpEmail << "https://" << domain << "/auth/" << safeEmail << "/" 
        << tokSend << "/main" << endl;
    tmpEmail.close();
    stringstream cmd;
    cmd << "mail --no-config --config-verbose -s 'Login request' '" << email << "' <" << tmpEmailPath;
    system(cmd.str().c_str());
    unlink(tmpEmailPath.c_str());
}

void AuthCode::sendExtraToken(std::string& em, std::string& qs) {
    string email = em + "+extra";
    if (!tokens.contains(email) || !tokens.contains(em)) return;
    string tokSend = tokens[em];
    string qTok = tokens[email];
    std::string tmpEmailPath = "/tmp/" + qTok + ".txt";
    std::ofstream tmpEmail(tmpEmailPath);
    std::string safeEmail(em);
    auto pos = string::npos;
    while((pos = safeEmail.find('@'))!=string::npos) {
        safeEmail.replace(pos,1,"%40");
    }
    tmpEmail << "To complete the BCMLM manager task you initiated at " << domain << ", please visit the following link:" << endl;
    tmpEmail << "https://" << domain << "/auth/" << safeEmail << "/"  << tokSend << "/" << qs << "&code2=" << qTok << endl;
    tmpEmail.close();
    stringstream cmd;
    cmd << "mail --no-config --config-verbose -s 'Action Authentication request' '" << em << "' <" << tmpEmailPath;
    system(cmd.str().c_str());
    unlink(tmpEmailPath.c_str());
    cerr << qTok << endl;
}

void AuthCode::expireToken(std::string& email) {
    if (tokens.contains(email)) {
        tokens.erase(email);
    }
}
