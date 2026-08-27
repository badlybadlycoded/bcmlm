#include "unsub.h"
#include "crow_all.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <filesystem>

using namespace std;
const std::string confdir = "/var/bcmlm/";

UnsubHandler::UnsubHandler(std::string& dom) : domain(dom), tokens(), pendingToken("") {
    // initialize token RNG
    t_lo = t_hi = 0;
    int urand = open("/dev/urandom",O_RDONLY);
    // read one long int into both t_lo and t_hi
    read(urand, &t_lo, 1);
    read(urand, &t_hi, 1);
    close(urand);
}

bool UnsubHandler::checkToken(std::string &email, std::string& list, std::string& token) {
    cerr << "checking for email '" << email << "', list '" << list << "' and token " << token << endl;
    if (tokens.contains(token)) {
        cerr << "found token: " << token << endl;
        auto& p = tokens[token];
        return p.email == email && p.list == list;
    } else {
        cerr << "did not find token: " << token << endl;
    }
    return false;
}

void UnsubHandler::createToken(std::string& email, std::string& list) {
    USPair elp(email,list);
    // step RNG
    const unsigned long a_lo = 0xa982dc0f8191b361;
    const unsigned long a_hi = 0x9f9194fe856a7ae3;
    const unsigned long b_lo = 0x9df7ba137be9d751;
    const unsigned long b_hi = 0xb97d747a9f948b03;
    t_lo = a_lo*t_lo + b_lo;
    t_hi = a_hi*t_hi + b_hi;
    // convert to unsafe char string
    char tok[17];
    memcpy(tok,(void *)(&t_lo),8);
    memcpy(tok+8,(void *)(&t_hi),8);
    tok[16] = 0;
    std::string tokStr(tok);
    // base64 encode the token, remove padding
    std::string safeTok(crow::utility::base64encode_urlsafe(tokStr,16));
    safeTok.pop_back();
    safeTok.pop_back();
    tokens[safeTok] = elp;
    pendingToken = safeTok;
}

bool UnsubHandler::getToken(std::string& email, std::string& list, std::string& outTok) {
    if (tokens[pendingToken].email == email && tokens[pendingToken].list == list) {
        outTok = pendingToken;
        return true;
    } else {
        for (auto& [t, p] : tokens) 
            if (p.email == email && p.list == list) {
                outTok = t;
                return true;
            }
    }
    return false;
}

void UnsubHandler::sendToken(std::string& email, std::string& list) {
    std::string tokSend("");
    if (!getToken(email,list,tokSend)) return;
    cerr << "sending token: " << tokSend << endl;
    std::string tmpEmailPath = "/tmp/" + tokSend + ".txt";
    std::ofstream tmpEmail(tmpEmailPath);
    tmpEmail << "We're sorry to see you go!" << endl;
    tmpEmail << "To complete your unsubscription request for " << list << "@" << domain <<
        ", please visit the following link:" << endl;
    tmpEmail << "https://" << domain << "/unsubscribe?list=" << list << "&email=" << email 
        << "&code=" << tokSend << endl;
    tmpEmail.close();
    
    std::stringstream cmd;
    cmd << "mail --no-config --config-verbose -s 'unsubscribe request confirmation' '" << email << "' <" << tmpEmailPath;
    system(cmd.str().c_str());
    unlink(tmpEmailPath.c_str());
}

void UnsubHandler::unsubEmail(std::string& email, std::string& list) {
    std::string token;
    if (!getToken(email,list,token)) return;
    std::string yamlPath = confdir + list + ".yaml";
    std::string tmpYamlPath = "/tmp/" + token + ".yaml";
    std::string notifyPath = confdir + list + ".notify";
    ifstream listyaml(yamlPath);
    ofstream tmpyaml(tmpYamlPath);
    std::string skipline = "  - " + email;
    std::string line;
    while (std::getline(listyaml, line)) {
        if (line != skipline) tmpyaml << line << endl;
    }
    listyaml.close();
    tmpyaml.close();
    filesystem::copy(tmpYamlPath,yamlPath,filesystem::copy_options::overwrite_existing);
    filesystem::remove(tmpYamlPath);
    ofstream nf(notifyPath);
    nf << "";
    nf.close();
}
