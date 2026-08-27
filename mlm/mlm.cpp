#include "crow_all.h"

#include "unsub.h"
#include "mlview.h"
#include "roles.h"
#include "auth.h"
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
using namespace std;

void setup_user_context(RoleView &roles, mlView &lists, string &user, crow::mustache::context &ctx) {
    auto padm = roles.checkPlatformAdmin(user);
    ctx["platform"] = padm;
    ctx["domain"] = lists.getDomain();
    vector<string> authlists;
    for (auto ls : lists) {
        if (padm || roles.checkListAdmin(user,ls.listname)) 
            authlists.push_back(ls.listname);        
    }
    ctx["authlists"] = authlists;
}

void emaildecode(string &email) {
    auto atp = string::npos;
    while ((atp = email.find("%40")) != string::npos)
        email.replace(atp,3,"@",1);
}

int main() {    
    crow::SimpleApp app;
    mlView mlists;
    mlists.readFromFile();
    string domain(mlists.getDomain());
    UnsubHandler us(domain);
    RoleView roles(mlists);
    AuthCode auth(domain);
    crow::mustache::set_global_base("/var/bcmlm/templates/");
    static string certpath("/var/bcmlm/cert.pem");
    static string privkeypath("/var/bcmlm/ec_key.pem");

    CROW_ROUTE(app, "/unsubscribe")(
        [&](crow::request& req, crow::response& res){
            auto &p = req.url_params;
            if (!p.get("list")) {
                res.code = 400; // bad request - link should include a list
            }  else {
                auto page = crow::mustache::load("unsub.html");
                crow::mustache::context ctx;
                string listname(p.get("list"));
                ctx["domain"] = domain;
                ctx["listname"] = listname;
                if (p.get("email") != nullptr) {
                    string email(p.get("email"));
                    ctx["email"] = email;
                    if (!p.get("code")) {
                        us.createToken(email,listname);
                        us.sendToken(email,listname);
                    } else {
                        string token(p.get("code"));
                        if (us.checkToken(email,listname,token)) {
                            us.unsubEmail(email,listname);
                            ctx["unsub"]=true;
                        } else {
                            res.code = 401; // unauthorized
                            res.end();
                            return;
                        }
                    }
                }
                res.write(page.render(ctx).dump());
            }
            res.end();
    });
    CROW_ROUTE(app,"/auth/<string>/<string>/viewlog")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            string email(user);
            emaildecode(email);
            if (!auth.checkToken(email,token)) {
                res.code = 401;
                res.end();
                return;
            }
            auto &p = req.url_params;
            if (!p.get("list")) {
                res.code = 400; 
                res.end(); 
                return;
            }
            string listname(p.get("list"));
            if (!mlists.exists(listname)) {
                res.code = 500;
                res.end();
                return;
            }
            auto page = crow::mustache::load("logview.html");
            crow::mustache::context ctx;
            ctx["listname"] = listname;
            setup_user_context(roles, mlists, email, ctx);
            string logPath = "/var/log/bcmlm/" + listname + ".log"; 
            stringstream logBuf;
            ifstream logFile(logPath);
            logBuf << logFile.rdbuf();
            ctx["logdata"] = logBuf.str();
            res.write(page.render(ctx).dump());
            res.end();
    });

    CROW_ROUTE(app,"/auth/<string>/<string>/main")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            string email(user);
            emaildecode(email);
            if (!auth.checkToken(email,token)) {
                res.code = 401;
                res.end();
                return;
            }
            auto page = crow::mustache::load("main.html");
            crow::mustache::context ctx;
            setup_user_context(roles, mlists, email, ctx);
            res.write(page.render(ctx).dump());
            res.end();
    });


    CROW_ROUTE(app,"/login")(
        [&](const crow::request& req, crow::response& res){
            auto &p = req.url_params;
            crow::mustache::context ctx;
            ctx["domain"] = mlists.getDomain();
            if (p.get("email")!=nullptr) {
                string email(p.get("email"));
                if (!roles.checkUser((email))) {
                    res.code = 404;
                    res.end();
                    return;
                } 
                ctx["email"] = email;
                auth.createToken(email);
                auth.sendToken(email);
            }
            auto page = crow::mustache::load("login.html");
            res.write(page.render(ctx).dump());
            res.end();
    });
    

    CROW_ROUTE(app,"/auth/<string>/<string>/editlist")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            string email(user);
            emaildecode(email);
            if (!auth.checkToken(email,token)) {
                res.code = 401;
                res.end();
                return;
            }
            auto &p = req.url_params;
            if (!p.get("list")) {
                res.code = 400; 
                res.end(); 
                return;
            }
            string listname(p.get("list"));
            if (!mlists.exists(listname)) {
                res.code = 500;
                res.end();
                return;
            }
            auto page = crow::mustache::load("editlist.html");
            crow::mustache::context ctx;
            ctx["listname"] = listname;
            setup_user_context(roles, mlists, email, ctx);
            for (auto m : mlists) {
                if (m.listname == listname) {
                    for (string s : {"subs","senders", "trailer", "ratelimit"})
                        ctx[s] = m.get(s);
                    string openAt = "open";
                    string openLs(m.get(openAt));
                    ctx["open"] = (openLs == "true");
                    set<string> adms;
                    m.getAdmins(adms);
                    stringstream adstr;
                    for (auto a : adms) {
                        adstr << "  - " << a << endl;
                    }
                    ctx["admins"] = adstr.str();
                }
            }
            res.write(page.render(ctx).dump());
            res.end();

    });

    CROW_ROUTE(app, "/auth/<string>/<string>/updatelist").methods(crow::HTTPMethod::POST)(
        [&](const crow::request& req,const std::string& u, const std::string& t){
            string email(u);
            emaildecode(email);
            string token(t);
            if (!auth.checkToken(email,token)) return crow::response(401);
            auto params = req.get_body_params();
            cerr << req.body << endl;
            map<string,string> newlist;
            for (string s : {"listname", "ratelimit", "admins", "senders", "subs", "trailer" }) {
                if (!params.get(s)) return crow::response(400);
                newlist[s] = params.get(s);
                auto rp = string::npos;
                while ((rp = newlist[s].find('\r')) != string::npos) newlist[s].replace(rp,1,"");
            }
            string openattr = "open";
            newlist["open"] = (params.get("open") == nullptr) ? "false" : "true";
            string listname(newlist["listname"]);
            // make sure list exists
            if (!mlists.exists(listname)) return crow::response(500);
            // make sure user can edit
            if (!roles.checkPlatformAdmin(email) && !roles.checkListAdmin(email,listname))
                return crow::response(401);
            string tmpYamlPath = "/tmp/listedit-" + listname + "-" + token + ".yaml";
            string yamlPath = "/var/bcmlm/" + listname + ".yaml";
            string notifyPath = "/var/bcmlm/" + listname + ".notify";
            ofstream tmpyaml(tmpYamlPath);
            tmpyaml << "listname: " << listname << endl;
            tmpyaml << "open: " << newlist["open"] << endl;
            tmpyaml << "ratelimit: " << newlist["ratelimit"] << endl;
            tmpyaml << "trailer: " << newlist["trailer"] << endl;
            tmpyaml << "admins:" << endl << newlist["admins"] << endl;
            tmpyaml << "senders:" << endl << newlist["senders"] << endl;
            tmpyaml << "subs:" << endl << newlist["subs"] << endl;
            tmpyaml.close();
            filesystem::copy(tmpYamlPath,yamlPath,filesystem::copy_options::overwrite_existing);
            filesystem::remove(tmpYamlPath);
            // notify list mailbox process
            ofstream notifyFile(notifyPath);
            notifyFile << "";
            notifyFile.close();
            mlists.readFromFile();
            // return to main page
            auto page = crow::mustache::load("main.html");
            crow::mustache::context ctx;
            setup_user_context(roles, mlists, email, ctx);         
            ctx["message"] = "Update succeeded!";
            return crow::response(page.render(ctx));
    });

    CROW_ROUTE(app,"/auth/<string>/<string>/rmlist")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            string email(user);
            emaildecode(email);
            if (!auth.checkToken(email,token)) {
                res.code = 401;
                res.end();
                return;
            }
            auto &p = req.url_params;
            if (!p.get("list")) {
                res.code = 400; 
                res.end(); 
                return;
            }
            string listname(p.get("list"));
            if (!mlists.exists(listname)) {
                res.code = 500;
                res.end();
                return;
            }
            string message;
            if (!p.get("code2")) {
                string query="rmlist?list=" + listname;
                auth.createExtraToken(email);
                auth.sendExtraToken(email,query);
                message = "You will receive an email with a link to confirm deleting the list " + listname;
            } else {
                string code2(p.get("code2"));
                string semail = email + "+extra";
                if (!auth.checkToken(semail,code2)) { 
                    res.code=401;
                    res.end();
                    return;
                }
                mlists.removeList(listname);
                auth.expireToken(semail);
                message = "Successfully deleted list " + listname;
            }
            auto page = crow::mustache::load("main.html");
            crow::mustache::context ctx;
            ctx["listname"] = listname;
            setup_user_context(roles, mlists, email, ctx);
            ctx["message"] = message;
            res.write(page.render(ctx).dump());
            res.end();
    });

    CROW_ROUTE(app,"/auth/<string>/<string>/addList")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            auto &p = req.url_params;
            string email(user);
            emaildecode(email);
            if (!auth.checkToken(email,token)) {
                res.code = 401;
                res.end();
                return;
            }
            auto page = crow::mustache::load("newlist.html");
            crow::mustache::context ctx;
            setup_user_context(roles,mlists,email,ctx);
            if (p.get("list") != nullptr) {
                string listname(p.get("list"));
                mlists.addList(listname);
                page = crow::mustache::load("editlist.html");
                ctx["listname"] = listname;
            }
            res.write(page.render(ctx).dump());
            res.end();
    });

    CROW_ROUTE(app,"/auth/<string>/<string>/addAdmin")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            auto &p = req.url_params;
            string email(user);
            emaildecode(email);
            if (!auth.checkToken(email,token)) {
                res.code = 401;
                res.end();
                return;
            }
            auto page = crow::mustache::load("newadmin.html");
            crow::mustache::context ctx;
            setup_user_context(roles,mlists,email,ctx);
            if (p.get("admin") != nullptr) {
                page = crow::mustache::load("main.html");
                string admin(p.get("admin"));
                if (!p.get("code2")) {
                    auto ap = string::npos;
                    while ((ap = admin.find('@')) != string::npos) admin.replace(ap,1,"%40");
                    ctx["message"] = "Adding " + admin + " as administrator: you will receive an email with a link to complete the process";  
                    string query="addAdmin?admin=" + admin;
                    auth.createExtraToken(email);
                    auth.sendExtraToken(email,query);                    
                } else {
                    string token2(p.get("code2"));
                    auto semail = email + "+extra";
                    if (!auth.checkToken(semail,token2)) { 
                        res.code=401;
                        res.end();
                        return;
                    }
                    roles.addPlatformAdmin(admin);
                    auth.expireToken(semail);
                    ctx["message"] = "Successfully added new administrator:  " + admin;                   
                }
            }
            res.write(page.render(ctx).dump());
            res.end();
    });

    CROW_ROUTE(app,"/auth/<string>/<string>/rmadmin")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            auto &p = req.url_params;
            string email(user);
            emaildecode(email);
            if (!auth.checkToken(email,token)) {
                res.code = 401;
                res.end();
                return;
            }
            auto page = crow::mustache::load("rmadmin.html");
            crow::mustache::context ctx;
            vector<string> vAdmins;
            roles.getPlatformAdmins(vAdmins);
            ctx["admins"] = vAdmins;
            setup_user_context(roles,mlists,email,ctx);
            if (p.get("admin") != nullptr) {
                page = crow::mustache::load("main.html");
                string admin(p.get("admin"));
                if (!p.get("code2")) {
                    auto ap = string::npos;
                    while ((ap = admin.find('@')) != string::npos) admin.replace(ap,1,"%40");
                    ctx["message"] = "Removing " + admin + " as administrator: you will receive an email with a link to complete the process";  
                    string query="rmadmin?admin=" + admin;
                    auth.createExtraToken(email);
                    auth.sendExtraToken(email,query);                    
                } else {
                    string token2(p.get("code2"));
                    auto semail = email + "+extra";
                    if (!auth.checkToken(semail,token2)) { 
                        res.code=401;
                        res.end();
                        return;
                    }
                    roles.remPlatformAdmin(admin);
                    auth.expireToken(semail);
                    ctx["message"] = "Successfully removed platform administrator:  " + admin;                   
                }
            }
            res.write(page.render(ctx).dump());
            res.end();
    });    

    CROW_ROUTE(app,"/auth/<string>/<string>/logout")(
        [&](const crow::request& req, crow::response& res, const string& user, const string& token){
            string email(user);
            emaildecode(email);
            auth.expireToken(email);
            string loginpath("/login");
            res.redirect(loginpath);
            res.end();
    });
    app.ssl_file(certpath, privkeypath).port(443).multithreaded().run();
}
