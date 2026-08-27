#include <iostream>
#include <pwd.h>
#include <sys/types.h>
#include <cerrno>
#include <string>
#include "mbproc.h"
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstring>
#include <vector>
#include <fcntl.h> 
#include <optional>
#include <cstdint>
#include <filesystem>
#include <cstdio>

const std::string confdir = "/var/bcmlm/";
const std::string mboxdir = "/var/mail/";
const std::string logdir = "/var/log/bcmlm/";
const time_t move_offset = 86400;
using namespace std;

void change_user(std::string& mlist) {
    struct passwd* pwd = getpwnam(mlist.c_str());
    setresuid(pwd->pw_uid,pwd->pw_uid,pwd->pw_uid);
    setenv("HOME","/tmp",1);
}

mbproc::mbproc(std::string &mlist) : mlist(mlist) { 
    pendingMoves = nullptr;
}

void mbproc::read_config(std::string& cfg_filename) {
    std::ifstream mbf(cfg_filename);
    std::string line;
    subs.clear();
    senders.clear();
    admins.clear();
    std::set<std::string> *adding = &admins; 

    while(std::getline(mbf,line)) {
        if (line.starts_with("open:")) {
            this->open = line.find("true") != std::string::npos;
        }
        if (line.starts_with("admins:")) adding = &admins;
        if (line.starts_with("subs:")) adding = &subs;
        if (line.starts_with("senders:")) adding = &senders;
        if (line.starts_with("  - ")) {
            string addr = line.substr(4,line.find("#")-4);
            adding->insert(addr);
        }
        if (line.starts_with("ratelimit: ")) this->rateLimit = std::stoi(line.substr(10));
        if (line.starts_with("trailer: ")) 
            this->trailer.append(line.substr(9,std::string::npos) + "\n");
    }
    mbf.close();
}

void mbproc::start_proc() {
    change_user(this->mlist);
    std::string cfg_filename = confdir + mlist + ".yaml";
    read_config(cfg_filename);
    main_loop();
}

void mbproc::main_loop() {
    std::string mbpath = mboxdir + mlist;
    std::string notify_path = confdir + mlist + ".notify";
    std::string cfg_filename = confdir + mlist + ".yaml";
    struct stat mbstat;
    std::time_t last_mod = 0;

    while(true) {
        stat(mbpath.c_str(),&mbstat);
        if (mbstat.st_mtime > last_mod && mbstat.st_size > 0) {
            cerr << "reading inbox" << endl;
            last_mod = mbstat.st_mtime;
            // copy mailbox to temporary file and truncate
            std::string mbname = std::tmpnam(nullptr);
            filesystem::copy(mbpath,mbname);
            filesystem::resize_file(mbpath,0);
            // process temp file
            process_mailbox(mbname);
            filesystem::remove(mbname);
            // write out any changes made to the config by email processing
            write_config(cfg_filename);
        } 
        if (access(notify_path.c_str(),F_OK) == 0) {
            read_config(cfg_filename);
            unlink(notify_path.c_str());
            std::string nosender("(null)");
            std::string noID("(null)");
            std::string rld_msg("reloaded config file.");
            log_message(nosender,noID,rld_msg);
        }
        sleep(1);
    }
}

void mbproc::process_mailbox(std::string& mbpath) {
    std::ifstream mbp(mbpath);
    std::string line;
    std::vector<std::string> emails; 
    char emfile[32];
    int mfd = -1;

    // Write each email in the mail spool to a separate file for handling
    while(std::getline(mbp,line)) {
        if (line.starts_with("From ")) {
            if (mfd > 0) close(mfd);
            strcpy(emfile, "/tmp/emXXXXXX");
            mfd = mkstemp(emfile);
            emails.push_back(emfile);
        }
        write(mfd,line.c_str(),line.length());
        write(mfd,"\n",1);
    }
    close(mfd);

    for(auto emf : emails) {
        handle_email(emf);
        unlink(emf.c_str());
    }
}

void mbproc::handle_email(std::string &empath) {
    // get the sender, subject, and command line (if any)
    std::ifstream mf(empath);
    std::string line;
    std::string sender;
    std::string subject;
    std::string msgID;
    std::optional<std::string> cmdline;
    std::string log;
    while(std::getline(mf,line)) {
        if (line.starts_with("Return-path:") || line.starts_with("Return-Path:")) {
            size_t start = line.find('<');
            size_t end = line.rfind('>');
            sender = line.substr(start+1, end-start-1);
        }
        if (line.starts_with("Subject:")) {
            subject = line.substr(9,std::string::npos);
        }
        if (line.starts_with("Message-Id:")) {
            size_t start = line.find('<');
            size_t end = line.rfind('>');
            msgID = line.substr(start+1, end-start-1);
        }
        if (line.starts_with("UNSUBSCRIBE") || 
            line.starts_with("SUBSCRIBE") ||
            line.starts_with("MOVED") ||
            line.starts_with("SENDER")) {
                cmdline.emplace(line);
            }
    }
    mf.close();

    // Is the message sender authorized to send to the list?
    if (senders.find(sender) != senders.end() && !cmdline) {
        handle_send(subject, empath);
        log = "sent-message";
        log_message(sender,msgID,log);
    }
    // Is the message sender a subscriber?  check for unsubscribe
    if (subs.find(sender) != subs.end() && cmdline) {
        if (cmdline.value().starts_with("UNSUBSCRIBE")) {
            handle_unsubscribe(sender);
            log = "unsubscribe";
            log_message(sender,msgID,log);
        }
    }
    // Is the message a MOVED?
    if (cmdline.value_or("").starts_with("MOVED")) {
        auto line = cmdline.value();
        size_t start = line.find('<');
        size_t end = line.rfind('>');
        auto oldaddr = line.substr(start+1, end-start-1);
        handle_move_start(sender,oldaddr);
        log = "move requested: " + oldaddr;
        log_message(sender,msgID,log);
    }
    // Is the message CANCELLING a move?
    if (subs.find(sender) != subs.end() && subject.find("MCANCEL") != std::string::npos) {
        size_t codestart = subject.find("MCANCEL") + 7;
        auto code = subject.substr(codestart, 16);
        auto now = std::time(nullptr);
        for(auto mv = pendingMoves; mv != nullptr; mv = mv->next) {
            if (strcmp(mv->oldEmail,sender.c_str()) == 0 && strcmp(mv->code,code.c_str()) == 0 && mv->validTime > now) {
                    if (mv == pendingMoves) {
                        pendingMoves = mv->next;
                        if (pendingMoves) pendingMoves->prev = pendingMoves;
                    } else {
                        mv->prev->next = mv->next;
                        mv->next->prev = mv->prev;
                    }
                    delete mv;
                    log = "move-cancelled";
                    log_message(sender,msgID,log);
                    break;
            }
        }
    }
    // Handle list admin operations
    if (admins.find(sender) != admins.end() && cmdline) {
        auto cmd = cmdline.value();
        auto a_start = cmd.find('<');
        auto a_end = cmd.rfind('>');
        auto addr = cmd.substr(a_start+1,a_end-a_start-1);
        if (cmd.starts_with("SUBSCRIBE") || subject == "SUBSCRIBE") {
            subs.insert(addr);
            log = "admin-subscribed: " + addr;
            log_message(sender,msgID,log);
        }
        if (cmd.starts_with("UNSUBSCRIBE") || subject == "UNSUBSCRIBE") {
            subs.erase(addr);
            log = "admin-dropped: " + addr;
            log_message(sender,msgID,log);
        }
        if (cmd.starts_with("SENDER") || subject == "SENDER") {
            senders.insert(addr);
            log = "added-sender: " + addr;
            log_message(sender,msgID,log);
        } 
    }
    if (this->open && cmdline.value_or("") == "SUBSCRIBE") {
        subs.insert(sender);
        log = "subscribed";
        log_message(sender,msgID,log);
    }
}

void mbproc::handle_unsubscribe(std::string& addr) {
    subs.erase(addr);
}

void mbproc::handle_send(std::string& subject, std::string& empath) {
    auto now = std::time(nullptr);
    // handle any moves
    auto pm = pendingMoves;
    while (pm != nullptr && pm->validTime > now) {
        pm = pm->next;
    }
    if (pm) { 
        if (pm->prev && pm->prev != pendingMoves) {
            pm->prev->next = nullptr;
            pm->prev = nullptr;
        }
    }
    while (pm) {
        if (pm->prev && pm->prev != pm) delete pm->prev;
        subs.erase(std::string(pm->oldEmail));
        subs.insert(std::string(pm->newEmail));
        if (pm == pendingMoves) {
            pendingMoves = nullptr;
        }
        pm = pm->next;
    }
    // make new file to hold email body, including trailer
    char emfile[32];
    strcpy(emfile, "/tmp/emXXXXXX");
    close(mkstemp(emfile));
    std::ofstream of(emfile);
    std::ifstream ef(empath);
    std::string line;
    bool found_body = false;

    while(std::getline(ef,line)) {
        if (line == "") {
            found_body = true;
            continue;
        }
        if (found_body) of << line << endl;
    }
    
    of << trailer << endl;
    of.close();
    std::string mailpath(emfile);

    // fork and do the send, respecting rate limit (emails/second)
    if (fork() == 0) {
        int sent = 0;
        for(auto dest : subs) {
            auto cmd = "mail --no-config --no-user-config -s \"" \
                + subject + "\" " + dest + " <" + mailpath;
            system(cmd.c_str());
            sent++;
            if (sent == rateLimit) {
                sleep(1);
                sent = 0;
            }
        }
        unlink(mailpath.c_str());
        exit(0);
    }
}

void mbproc::handle_move_start(std::string& newaddr, std::string& oldaddr) {
    if (subs.find(oldaddr) == subs.end()) return;
    // get expiration time
    time_t validTime = std::time(nullptr) + move_offset;
    // create cancellation code - 10 random bytes -> 16 base32 chars
    char rawcode[10];
    int rfd = 0;
    rfd = ::open("/dev/urandom", O_RDONLY);
    read(rfd,rawcode,10);
    close(rfd);
    string code("");
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    for(int off = 0; off < 10; off+=5) {
        uint64_t codeInt = 0;
        for (int i = 0; i < 5; i++) codeInt = (codeInt << 8) | rawcode[off+i];
        for (int i = 0; i < 8; i++) {
            code.push_back(alphabet[codeInt & 0x1F]);
            codeInt >>= 5;
        }
    }
    moveInfo *mv = new moveInfo;
    mv->prev = mv;
    mv->next = pendingMoves;
    mv->validTime = validTime;
    pendingMoves = mv;
    strcpy(mv->newEmail,newaddr.c_str());
    strcpy(mv->oldEmail,oldaddr.c_str());
    strcpy(mv->code,code.c_str());
    if (mv->next != 0) mv->next->prev = mv;
    // send response to newEmail.  Text needs some help from marketing!
    std::string tmpfilename = "/tmp/mvresp_" + mlist + "_" + code + "_" + newaddr;
    ofstream newBody(tmpfilename);
    newBody << "We have received your request to change your subscription address to the " << mlist << " mailing list." << endl;
    newBody << "If we do not receive a cancellation notice from your previous address (<" << oldaddr << ">) in 24 hours, the subscription will be updated." << endl;
    newBody << endl << "Thank You!" << endl;
    newBody << trailer << endl;
    newBody.close();
    std::string cmd = "mail --config-verbose --no-config -s \"RE: Your subscription update request\" " + newaddr + " <" + tmpfilename;
    system(cmd.c_str());
    unlink(tmpfilename.c_str());

    //send cancellation request to oldEmail.
    tmpfilename = "/tmp/mvreq_" + mlist + "_" + code + "_" + oldaddr;
    ofstream oldBody(tmpfilename);
    oldBody << "We have received your request to update your subscription to the " << mlist << " mailing list." << endl;
    oldBody << "If you do not wish to update your email address, please reply to this email within 24 hours." << endl;
    oldBody << "If you agree to the request, no further action is required." << endl << "Thank You!" << endl;
    oldBody << trailer << endl;
    oldBody.close();

    cmd = "mail --config-verbose --no-config -s \"Subscription Update Requested (MCANCEL" 
        + code + ")\" " + oldaddr + " <" + tmpfilename;
    system(cmd.c_str());
    unlink(tmpfilename.c_str());
}

void mbproc::write_config(std::string& configpath) {
    char tmpconf[32];
    strcpy(tmpconf, "/tmp/confXXXXXX");
    int tfd = mkstemp(tmpconf);
    ofstream cfile(tmpconf);
    cfile << "open: " << (open ? "true" : "false") << endl;
    cfile << "ratelimit: " << rateLimit << endl;
    cfile << "admins:" << endl;
    for (auto e : admins) {
        cfile << "  - " << e << endl;
    }
    cfile << "subs:" << endl;
    for (auto e : subs) {
        cfile << "  - " << e << endl;
    }
    cfile << "senders:" << endl;
    for (auto e : senders) {
        cfile << "  - " << e << endl;
    }
    cfile << "trailer: " << trailer << endl;
    cfile.close();

    // copy new config file over old config file
    filesystem::copy(tmpconf,configpath.c_str(),filesystem::copy_options::overwrite_existing);
    // delete old config file.
    close(tfd);
    unlink(tmpconf);
}

void mbproc::log_message(std::string& sender, std::string& msgID, std::string& action) {
    std::stringstream cmd;
    cmd << "echo `date` '" << sender << "' '" << msgID << ":' " << action;
    cmd << ">>" << logdir << mlist << ".log";
    system(cmd.str().c_str());
}
