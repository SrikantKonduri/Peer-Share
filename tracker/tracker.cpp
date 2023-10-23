#include <iostream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <string>

#define LISTEN_PORT 8000
#define CHUNK_SIZE 524288

using namespace std;

class User{
public:
    string username,pwd,ip,port;
    bool is_alive;
    User(){}
    User(string _username,string _pwd){
        username = _username;
        pwd = _pwd;
        ip = "";
        port = "";
        is_alive = false;
    }
};

class Group{
public:
    string group_name,group_owner;
    set<string> group_members,files_list,pending_requests;

    Group(){}
    Group(string _group_name,string _group_owner){
        group_name = _group_name;
        group_owner = _group_owner;
    }

    void addMember(string username){
        group_members.insert(username);
    }

    bool hasMember(string username){
        return group_members.find(username) != group_members.end();
    }

    void addToPendingList(string username){
        pending_requests.insert(username);
    }

    string getGroupOwner(){
        return group_owner;
    }

    set<string> getPendingList(){
        return pending_requests; 
    }

    set<string>::iterator findUserInPendingList(string username){
        return pending_requests.find(username);
    }
};

class FileMeta{
public:
    string filename;
    int chunkcount;
    long long int filesize;
    vector<string> hashes_list,seeders_list;

    FileMeta(){}
    FileMeta(string _filename,int _chunkcount,long long int _filesize,string _username){
        filename = _filename;
        chunkcount = _chunkcount;
        filesize = _filesize;
        seeders_list.push_back(_username);
    }

    void addPeer(string username){
        seeders_list.push_back(username);
    }

    void addHash(string hash){
        hashes_list.push_back(hash);
    }
};

int server_fd;
struct sockaddr_in address;
int add_len = sizeof(address);
string TRACKER_IP = "127.0.0.1";
map<string,User> user_map;
map<string,Group> group_map;
map<string,FileMeta> filemeta_map;
// filemeta_map has elements in the format --> {<groupname$filename>,<FileMeta>}

void hardCodeInit(){
    string username = "srikant";
    User new_user1("srikant","sk123");
    user_map.insert({"srikant",new_user1});
    username = "chandu";
    User new_user2("chandu","ch123");
    user_map.insert({"chandu",new_user2});
    User new_user3("abhi","abhi123");
    user_map.insert({"abhi",new_user3});
    User new_user4("pv","pv123");
    user_map.insert({"pv",new_user4});
    Group g("movies","abhi");
    g.addMember("chandu");
    g.addMember("srikant");
    g.addMember("pv");
    group_map.insert({"movies",g});
    // FileMeta fm("large.mp4",845,442802325,"abhi");
    // FileMeta fm("kartikeya.mp4",920,481919421,"abhi");
    // fm.addPeer("srikant");
    // FileMeta fm1("image4k.jpeg",1,235139,"abhi");
    // fm1.addPeer("srikant");
    // FileMeta fm2("filexplorer.cpp",1,25843,"abhi");
    // fm2.addPeer("srikant");
    // FileMeta fm3("mydoc.pdf",1,138325,"abhi");
    // fm3.addPeer("srikant");
    // cout << "sdfkjjsbdhjfhjdbjfjjhfbjhdgx" << fm.seeders_list.size() << endl;
    // for(auto user : fm.seeders_list){
    //     cout << "seeder: " << user << endl;
    // }
    // filemeta_map.insert({"movies$large.mp4",fm});
    // filemeta_map.insert({"movies$kartikeya.mp4",fm});
    // filemeta_map.insert({"movies$image4k.jpeg",fm1});
    // filemeta_map.insert({"movies$filexplorer.cpp",fm2});
    // filemeta_map.insert({"movies$mydoc.pdf",fm3});
}

void init(){
    int new_fd,valread;
    int opt = 1;
    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) == 0){
        perror("[-] Error in creating socket.");
        exit(1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(LISTEN_PORT);
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if(bind(server_fd,(struct sockaddr *)&address,sizeof(address)) < 0){
        perror("[-] Error in binding socket.");
        exit(1);
    }
    if(listen(server_fd,INT_MAX) == -1){
        perror("[-] Error in listening.");
        exit(1);
    }
    cout << "[+] Listening..." << endl;
}

vector<string> split(string str,char delimiter){
    int n = str.length();
    vector<string> result;
    string temp = "";
    for(int i=0;i<n;i++){
        char ch = str[i];
        if(ch != delimiter){
            temp.append(1,ch);
        }
        else{
            result.push_back(temp);
            temp = "";
        }
    }
    if(temp != ""){
        result.push_back(temp);
    }
    return result;
}

bool checkGroupExists(string group_name){
    return group_map.find(group_name) != group_map.end();
}

bool checkUserExists(string username){
    return user_map.find(username) != user_map.end();
}

bool checkUserInGroup(string username,string group_name){
    Group grp = group_map[group_name];
    return grp.hasMember(username);
}

bool checkIfUserIsOwner(string username,string group_name){
    Group group_obj = group_map[group_name];
    return group_obj.group_owner == username;
}

int initTorrentFile(string torrent_fileurl,string username,long long int size,int chunkcount){
    int torrentfile_fd = open(torrent_fileurl.c_str(), O_WRONLY | O_CREAT, 0644);
    string temp = username + "\n";
    write(torrentfile_fd,temp.c_str(),temp.length());
    temp = to_string(size) + "\n";
    write(torrentfile_fd,temp.c_str(),temp.length());
    temp = to_string(chunkcount) + "\n";
    write(torrentfile_fd,temp.c_str(),temp.length());

    return torrentfile_fd;
// returns file descriptor of torrent file.
}

int getChunkCount(long long int filesize){
    if(filesize % 524288 != 0){
        return (filesize / 524288) + 1;
    }
    return filesize / 524288;
}

bool checkFileInGroupExists(string check_str){
    return filemeta_map.find(check_str) != filemeta_map.end();
}

string getPeersHavingFile(string group_file_name){
    FileMeta fm = filemeta_map[group_file_name];
    vector<string> seeders_list = fm.seeders_list;
    string ips_str = "";
    for(auto user : fm.seeders_list){
        User user_obj = user_map[user];
        cout << "asdhjbad " << user << " " << user_obj.ip << " " << user_obj.port << endl;
        ips_str = ips_str + user_obj.ip + "$" + user_obj.port + " ";
    }
    cout << ips_str << endl;
    return ips_str.substr(0,ips_str.length()-1);
}

bool handleCreateUser(vector<string> user_command_splits){
    if(user_map.find(user_command_splits[1]) != user_map.end()){
        return false;
    }
    User new_user(user_command_splits[1],user_command_splits[2]);
    user_map.insert({user_command_splits[1],new_user});
    return true;
}

bool handleLogin(vector<string> user_command_splits){
    string username = user_command_splits[1];
    string pwd = user_command_splits[2];
    string ip = user_command_splits[3];
    string port = user_command_splits[4];
    if(user_map.find(username) == user_map.end() || user_map[username].is_alive){
        return false;
    }
    User cur_user = user_map[username];
    if(cur_user.pwd == pwd){
        user_map[username].is_alive = true;
        user_map[username].ip = ip;
        user_map[username].port = port;
    }
    cout << username << ": " << user_map[username].ip << " " << user_map[username].port << endl;
    return cur_user.pwd == pwd;
}

string handleCreateGroup(vector<string> user_command_splits){
    string group_name = user_command_splits[1];
    string username = user_command_splits[2];
    string response = "";
    if(checkGroupExists(group_name)){
        response = "Group already exists.";
    }
    else if(!checkUserExists(username)){
        response = "Invalid user, cannot create group.";
    }
    else{
        Group new_group(group_name,username);
        new_group.addMember(username);
        group_map.insert({group_name,new_group});
        response = "Group created successfully.";
    }
    return response;
}

string handleJoinGroup(vector<string> user_command_splits){
    string group_name = user_command_splits[1];
    string username = user_command_splits[2];
    string response = "";
    if(!checkUserExists(username)){
        response = "User does not exist.";
    }
    else if(!checkGroupExists(group_name)){
        response = "No such group exists.";
    }
    else if(checkUserInGroup(username,group_name)){
        response = "User already added in group.";
    }
    else{
        group_map[group_name].addToPendingList(username);
        response = "Request placed for group owner.";
    }
    return response;
}

string handleListGroups(){
    string answer = "";
    for(auto it : group_map){
        answer = answer + it.first + " "; 
    }
    cout << "Answer: " << answer << answer.length() << endl;
    return answer.substr(0,answer.length()-1);
}

string handleListRequests(vector<string> user_command_splits){
    string group_name = user_command_splits[1];
    string username = user_command_splits[2];
    string response = "";
    if(!checkGroupExists(group_name)){
        response = "No such group exists.";
    }
    else if(!checkIfUserIsOwner(username,group_name)){
        response = "No pending requests.";
    }
    else{
        set<string> pending_list = group_map[group_name].getPendingList();
        if(pending_list.empty()){
           return "No pending requests."; 
        }
        for(auto user : pending_list){
            response = response + user + " ";    
        }
        response = response.substr(0,response.length()-1);
    }
    return response;
}

string handleAcceptRequest(vector<string> user_command_splits){
    string group_name = user_command_splits[1];
    string username = user_command_splits[2];
    string doubt_owner = user_command_splits[3];
    string response = "";
    if(!checkGroupExists(group_name)){
        response = "No such group exists.";
    }
    else if(!checkUserExists(username)){
        response = "No such user exists.";
    }
    else if(!checkIfUserIsOwner(doubt_owner,group_name)){
        response = "User is not owner";
    }
    else{
        Group group_obj = group_map[group_name];
        set<string>::iterator itr = group_obj.findUserInPendingList(username);
        if(itr == group_obj.pending_requests.end()){
            response = "User already joined the group";
        }
        else{
            auto mark = find(group_map[group_name].pending_requests.begin(),group_map[group_name].pending_requests.end(),username);
            group_map[group_name].pending_requests.erase(mark);
            for(auto mem : group_map[group_name].pending_requests){
                cout << "Mem: " << mem << endl;
            }
            group_map[group_name].addMember(username);
            response = "User added to group";
        }
    }
    return response;
}

string handleUploadFile(int client_socket,vector<string> user_command_splits){
    string filename = user_command_splits[1];
    long long int filesize = stoll(user_command_splits[2]);
    string username = user_command_splits[3];
    string group_name = user_command_splits[4];
    int chunkcount = getChunkCount(filesize);
    string hash = user_command_splits[6] + "\n";
    string torrent_fileurl = "uploads/" + group_name + "$" + filename + ".torrrent";
    string grp_filename = group_name + "$" + filename;
    if(checkFileInGroupExists(grp_filename)){
        filemeta_map[grp_filename].addPeer(username);
        return "File in group already exists, but added you to the list";
    }
    int torrentfile_fd = initTorrentFile(torrent_fileurl,username,filesize,chunkcount);
    FileMeta new_file_meta(filename,chunkcount,filesize,username);
    new_file_meta.addHash(user_command_splits[6]);

    write(torrentfile_fd,hash.c_str(),hash.length());
    char buffer[CHUNK_SIZE] = {0};
    while(recv(client_socket,buffer,40,0)){
        string temp(buffer);
        if(temp == "Done"){
            break;
        }
        new_file_meta.addHash(temp);
        temp = temp + "\n";
        write(torrentfile_fd,temp.c_str(),temp.length());
        memset(buffer, 0, sizeof(buffer));
    }
    cout << "OUTSIDE: " << endl;
    close(torrentfile_fd);
    string filemeta_key = group_name + "$" + filename;
    filemeta_map.insert({filemeta_key,new_file_meta});
    cout << "[*] Printing filemeta_map: " << endl;
    for(auto itr : filemeta_map){
        cout << itr.first << endl;
        cout << itr.second.filename << " " << itr.second.filesize << " " << itr.second.chunkcount << endl;
        cout << "Hashes: " << endl;
        for(auto itr1 : itr.second.hashes_list){
            cout << itr1 << endl;
        }
        cout << "Seeders: " << endl;  
        for(auto itr1 : itr.second.seeders_list){
            cout << itr1 << endl;
        }
    }
    return "File uploaded successfully.";
}

void handleDownloadFile(int client_socket,vector<string> user_command_splits){
    string group_name = user_command_splits[1];
    string filename = user_command_splits[2];
    string username = user_command_splits[4];
    string check_str = group_name + "$" + filename;
    string response = "";
    if(!checkFileInGroupExists(check_str)){
        response = "File does'nt exist";
    }
    else{
        response = getPeersHavingFile(check_str) + " " + to_string(filemeta_map[check_str].filesize);
        filemeta_map[check_str].addPeer(username);
    }
    send(client_socket,response.c_str(),response.length(),0);
}

void handle_peer_connection(int client_socket){
    while(true){
        char buffer[CHUNK_SIZE] = {0};
        memset(buffer,0,CHUNK_SIZE);
        int read_count;
        if((read_count = read(client_socket,buffer,CHUNK_SIZE)) <= 0){
            // Handle this
            close(client_socket);
            break;
        }
        string user_command(buffer);
        string response;
        cout << "User command recieved by tracker: " << buffer << endl;
        vector<string> user_command_splits = split(user_command,' ');
        string command_type = user_command_splits[0];
        if(command_type == "create_user"){
            bool status = handleCreateUser(user_command_splits);
            response = "User creation successful.";
            if(!status){
                response = "User already exists.";
            }  
            cout << "[+] response: " << response << endl;
            send(client_socket,response.c_str(),response.length(),0);
        }
        else if(command_type == "login"){
            bool is_loggedin = handleLogin(user_command_splits);
            response = "Login successful";
            if(!is_loggedin){
                response = "Invalid credentials or user already logged in.";
            }
            cout << "[+] response: " << response << " " << is_loggedin << endl;
            send(client_socket,response.c_str(),response.length(),0);
        }
        else if(command_type == "create_group"){
            response = handleCreateGroup(user_command_splits);
            send(client_socket,response.c_str(),response.length(),0);
        }
        else if(command_type == "join_group"){
            response = handleJoinGroup(user_command_splits);
            send(client_socket,response.c_str(),response.length(),0);
        }
        else if(command_type == "list_groups"){
            response = handleListGroups();
            send(client_socket,response.c_str(),response.length(),0);
        }
        else if(command_type == "list_requests"){
            response = handleListRequests(user_command_splits);
            send(client_socket,response.c_str(),response.length(),0);
        }
        else if(command_type == "accept_request"){
            response = handleAcceptRequest(user_command_splits);
            send(client_socket,response.c_str(),response.length(),0);   
        }
        else if(command_type == "upload_file"){
            response = handleUploadFile(client_socket,user_command_splits);
            send(client_socket,response.c_str(),response.length(),0);
        }
        else if(command_type == "download_file"){
            handleDownloadFile(client_socket,user_command_splits);
        }
    }
}

int main(){
    // thread server(init);
    init();
    hardCodeInit();
    int new_fd;
    vector<thread> peer_threads;


    while(true){
        if((new_fd= accept(server_fd, (struct sockaddr*)&address,(socklen_t*)&add_len)) < 0){
            exit(1);
        }
        peer_threads.push_back(thread(handle_peer_connection,new_fd));
    }
    for(auto it=peer_threads.begin();it != peer_threads.end();it++){
        if(it->joinable()){
            it->join();
        }
    }
    if(new_fd < 0){
        perror("[-] Error in recieving request.");
        exit(1);
    }
}
