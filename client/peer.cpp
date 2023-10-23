#include <iostream>
#include <thread>
#include <pthread.h>
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <fstream>
#include <fcntl.h>

#define CHUNK_SIZE 524288
#define BLOCK_SIZE 65536

using namespace std;



int TRACKER_PORT,CLIENT_PORT;
string TRACKER_IP,CLIENT_IP;
int tracker_sock,tracker_fd;
string current_user = "";

// following map is used by peer client
// Values format: <grpname$filename>,<list of ip$port strings>
map<string,vector<string>> file_sockaddr_map;

map<string,string> filebitstring_map_client;

// following map is used by peer server
// Values format: {<grpname$filename>,<bitstring>}
map<string,string> filebitstring_map;



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

string getStringHash(string segmentString){
    unsigned char md[20];
    string hash = "";
    if(!SHA1(reinterpret_cast<const unsigned char *>(&segmentString[0]), segmentString.length(), md)){
        printf("Error in hashing\n");
    }
    else{
        for(int i=0; i<20; i++){
            char buf[3];
            sprintf(buf, "%02x", md[i]&0xff);
            hash += string(buf);
        }
    }
    return hash;
}


vector<string> getSocketSplits(string socket_address){
    vector<string> result = split(socket_address,':');
    return result;
}

string getAbsolutePath(string relative_path){
    string result = get_current_dir_name();
    if(relative_path[0] == '~'){
        result = "/home/";
        result.append(getlogin());
        relative_path = relative_path.substr(1,relative_path.length()); 
        result.append(relative_path);
    }
    else if(relative_path[0] == '/'){
        result = relative_path;
    }
    else{
        string local_pwd = "";
        for(int i=0;i<relative_path.length();i++){
            char ch = relative_path[i];
            if(ch != '/'){
                local_pwd.append(1,ch);
            }
            else{
                if(local_pwd == ".."){
                    int pos = result.find_last_of('/');
                    if(pos != 0){
                        result = result.substr(0,pos);
                    }
                }
                else if(local_pwd != "." && local_pwd != ""){
                    result.append("/");
                    result.append(local_pwd);
                }
                local_pwd = "";
            }
        }
        if(local_pwd != ""){
            if(local_pwd == ".."){
                int pos = result.find_last_of('/');
                if(pos != 0){
                    result = result.substr(0,pos);
                }
            }
            else if(local_pwd != "."){
                result.append("/");
                result.append(local_pwd);
            }
        }
    }
    return result;
}


long long int getFileSize(string file_abs_url){
    struct stat file_meta;
    stat(file_abs_url.c_str(),&file_meta);
    return file_meta.st_size;
}

void handleGetBitString(string filename){

}

void servePiece(int socket,vector<string> peer_command_splits){
    string filename = peer_command_splits[2];
    int pieceno = stoi(peer_command_splits[1]);
    cout << "Serving piece: " << pieceno << endl;
    long long int offset = pieceno * CHUNK_SIZE;
    char buffer[BLOCK_SIZE] = { 0 };
    long long int maxoffset = (pieceno+1) * CHUNK_SIZE;
    string file_url = "./files/" + filename;
    int source = open(file_url.c_str(),O_RDONLY,0);
    int read_count = 0,readtillnow = 0;
    while((offset < maxoffset )&& (read_count = pread(source,buffer,BLOCK_SIZE,offset)) > 0){
        cout << "In loop : " << pieceno << endl;
        if((send(socket,buffer,read_count,0)) == -1){
            perror("[-] Error in sending file.");
            exit(1);
        }
        usleep(10000);
        readtillnow += read_count;
        offset = offset + read_count;
        memset(buffer,0,BLOCK_SIZE);
        // if(readtillnow >= CHUNK_SIZE){
        //     break;
        // }
        // cout << "[+] Reading next in peer server." << endl;
    }
    // cout << "Serving piece: readcount: " << pieceno << " " << read_count << endl;
    cout << "[+] Piece sent successfully. " << pieceno << endl;
}

void serveBitString(int socket,vector<string> peer_command_splits){
    string grp_filename = peer_command_splits[1];
    string response = filebitstring_map[grp_filename];
    cout << "Sending bitstring: " << response << endl;
    send(socket,response.c_str(),response.length(),0);
}

void handlePeerServerRequests(int socket){
    cout << "Request recieved from another peer" << endl;
    while(true){
        char buffer[CHUNK_SIZE] = {0};
        memset(buffer,0,CHUNK_SIZE);
        int read_count;
        if((read_count = read(socket,buffer,CHUNK_SIZE)) <= 0){
            // Handle this
            close(socket);
            break;
        }
        string peer_command(buffer);
        string response;
        cout << "User command recieved by peer server: " << buffer << endl;
        vector<string> peer_command_splits = split(peer_command,' ');
        string command_type = peer_command_splits[0];
        if(command_type == "getbitstring"){
            serveBitString(socket,peer_command_splits);
        }
        else if(command_type == "download_piece"){
            int pieceno = stoi(peer_command_splits[1]);
            thread th1(servePiece,socket,peer_command_splits);
            th1.join();
        }
    }
}

int getPeerSocket(string ip,int port){
    int fd,valread,peer_fd,peer_socket;
    struct sockaddr_in peer_addr;
    // string hello = "Hello from client1";
    char buffer[1024] = { 0 };

    if((peer_socket = socket(AF_INET,SOCK_STREAM,0)) == 0){
        perror("[-] Error in creating tracker socket.");
        exit(1);
    }
    // cout << "[+] Socket created successful for connecting to peer." << endl;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &peer_addr.sin_addr) <= 0) {
        perror("[-] Invalid address.");
        exit(1);
    }

    if ((peer_fd = connect(peer_socket, (struct sockaddr*)&peer_addr,sizeof(peer_addr))) < 0) {
        perror("[-] Connection failed.");
        cout << "Errno: " << errno << endl;
        exit(1);
    }
    return peer_socket;
}


void startClientServer(){
    int server_fd,new_socket,valread;
    char buffer[1024] = { 0 };
    int opt = 1;
    struct sockaddr_in address;
    vector<thread> peer_server_threads;
    int add_len = sizeof(address);
    string message = "Hello from server!";
    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) == 0){
        perror("[-] Error in creating socket.");
        exit(1);
    }
    cout << "[+] Socket created!" << endl;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CLIENT_PORT);
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    int status = bind(server_fd,(struct sockaddr *)&address,sizeof(address));
    if(status < 0){
        perror("[-] Error in binding ");
        exit(1);
    }
    cout << "[+] Binding successful!" << endl;

    if(listen(server_fd,INT_MAX) == -1){
        perror("[-] Error in listening ");
        exit(1);
    }
    cout << "[+] Listening.." << endl;
    while(true){
        if((new_socket = accept(server_fd, (struct sockaddr*)&address,(socklen_t*)&add_len))>= 0){
            // handlePeerServerRequests(peer_server_threads);
            peer_server_threads.push_back(thread(handlePeerServerRequests,new_socket));
        }
    }
    for(auto it=peer_server_threads.begin();it!=peer_server_threads.end();it++){
        if(it->joinable())it->join();
    }
}

void setTrackerDetails(string file_url){
    fstream file;
    file.open(file_url.c_str(),ios::in);
    if (file.is_open()){
        string text;
        // int readcount = getline(file,text);
        getline(file,text);
        // if(readcount == -1){
        //     perror("[-] Error in reading from tracker file.");
        //     exit(1);
        // }
        vector<string> text_splits = split(text,' ');
        TRACKER_IP = text_splits[0];
        TRACKER_PORT = stoi(text_splits[1]);
        file.close();
    }
    else{
        perror("[-] Error in reading tracker info file.");
        exit(1);
    }
}

void setTrackerSock(){
    int fd,valread;
    struct sockaddr_in tracker_addr;
    // string hello = "Hello from client1";
    char buffer[1024] = { 0 };

    if((tracker_sock = socket(AF_INET,SOCK_STREAM,0)) == 0){
        perror("[-] Error in creating tracker socket.");
        exit(1);
    }
    cout << "[+] Socket created successful for connecting to tracker." << endl;
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_port = htons(8000);
    if (inet_pton(AF_INET, "127.0.0.1", &tracker_addr.sin_addr) <= 0) {
        perror("[-] Invalid address.");
        exit(1);
    }

    if ((tracker_fd = connect(tracker_sock, (struct sockaddr*)&tracker_addr,sizeof(tracker_addr))) < 0) {
        perror("[-] Connection failed.");
        cout << "Errno: " << errno << endl;
        exit(1);
    }
}

void createUserEvent(string user_command){
    cout << "[+] Sending request to tracker for creating user" << endl;
    if(send(tracker_sock,user_command.c_str(),user_command.length(),0) == -1){
        perror("[-] Error in sending command to tracker.");
        exit(1);
    }
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    cout << "[+] response from tracker: " << buffer << endl;
}

void loginUserEvent(string user_command,vector<string> command_splits){
    cout << "[+] Sending request to tracker for creating user" << endl;
    user_command = user_command + " " + CLIENT_IP + " " + to_string(CLIENT_PORT);
    if(send(tracker_sock,user_command.c_str(),user_command.length(),0) == -1){
        perror("[-] Error in sending command to tracker.");
        exit(1);
    }
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    cout << "[+] response from tracker: " << buffer << endl;
    string response(buffer);
    if(response == "Login successful"){
        current_user = command_splits[1];
    }
    cout << "[*] Current user: " << current_user << endl;
}

void createGroupEvent(string user_command){
    user_command = user_command + " " + current_user;
    cout << "[+] Sending request to tracker for creating user: " << user_command << endl;
    if(send(tracker_sock,user_command.c_str(),user_command.length(),0) == -1){
        perror("[-] Error in sending command to tracker.");
        exit(1);
    }
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    cout << "[+] response from tracker: " << buffer << endl;   
}

void joinGroupEvent(string user_command){
    user_command = user_command + " " + current_user;
    cout << "[+] Sending request to tracker for creating user: " << user_command << endl;
    if(send(tracker_sock,user_command.c_str(),user_command.length(),0) == -1){
        perror("[-] Error in sending command to tracker.");
        exit(1);
    }
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    cout << "[+] response from tracker: " << buffer << endl;
}

void listGroupsEvent(string user_command){
    user_command = user_command + " " + current_user;
    cout << "[+] Sending request to tracker for creating user: " << user_command << endl;
    if(send(tracker_sock,user_command.c_str(),user_command.length(),0) == -1){
        perror("[-] Error in sending command to tracker.");
        exit(1);
    }
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    string buffer_parsed(buffer);
    vector<string> group_list = split(buffer_parsed,' ');
    cout << "[+] response from tracker: " << buffer << endl;
    cout << "************** List of Groups **************" << endl;;
    for(auto grp : group_list){
        cout << "> " << grp << endl;
    }    
}

void listRequestsEvent(string user_command,vector<string> command_splits){
    user_command = user_command + " " + current_user;
    cout << "[+] Sending request to tracker for pending requests: " << user_command << endl;
    if(send(tracker_sock,user_command.c_str(),user_command.length(),0) == -1){
        perror("[-] Error in sending command to tracker.");
        exit(1);
    }
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    string buffer_parsed(buffer);
    if(buffer_parsed == "No such group exists." || buffer_parsed == "No pending requests."){
        cout << buffer_parsed << endl;
    }
    else{
        cout << "*************** List of pending requests for " << command_splits[1] << " *************" << endl;
        vector<string> pending_list = split(buffer_parsed,' ');
        for(auto user : pending_list){
            cout << "> " << user << endl;
        }
    }
}

void acceptRequestEvent(string user_command,vector<string> command_splits){
    user_command = user_command + " " + current_user;
    cout << "[+] Sending request to tracker for accepting requests: " << user_command << endl;
    if(send(tracker_sock,user_command.c_str(),user_command.length(),0) == -1){
        perror("[-] Error in sending command to tracker.");
        exit(1);
    }
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    string buffer_parsed(buffer);
    cout << "Tracker response: " << buffer_parsed << endl;

}

string getFileName(string absolute_fileurl){
    int idx = absolute_fileurl.find_last_of('/');
    return absolute_fileurl.substr(idx+1,absolute_fileurl.length()-1);
}



void uploadFileEvent(string user_command,vector<string> command_splits){
    string file_abs_url = getAbsolutePath(command_splits[1]);
    string filename = getFileName(file_abs_url);
    string group_name = command_splits[2];

    long long int filesize = getFileSize(file_abs_url);
    string to_send_temp = command_splits[0] + " " + filename + " " + 
        to_string(filesize) + " " + current_user + " " + group_name;
    cout << "Absolute path: " << filename << endl;

    int fd = open(file_abs_url.c_str(),O_RDONLY,0);
    int read_count = 0,chunk_no = 0;
    char buffer[CHUNK_SIZE] = {0};
    read(fd,buffer,CHUNK_SIZE);
    string read_str(buffer);
    string hash = getStringHash(read_str);
    string to_send = to_send_temp + " " + to_string(chunk_no) + " " + hash;
    send(tracker_sock,to_send.c_str(),to_send.length(),0);
    memset(buffer, 0, sizeof(buffer));
    while((read_count = read(fd,buffer,CHUNK_SIZE)) > 0){
        string temp_str(buffer);
        string hash = getStringHash(temp_str);
        chunk_no++;
        string to_send = to_send_temp + " " + to_string(chunk_no) + " " + hash;
        cout << "HASH: " << to_send << endl;
        send(tracker_sock,hash.c_str(),hash.length(),0);
        memset(buffer, 0, sizeof(buffer));
    }
    close(fd);
    string filebitstring_map_key = group_name + "$" + filename;
    float temp_count = (float)(filesize) / CHUNK_SIZE;
    int piece_count = (int)ceil(temp_count);
    string temp(piece_count,'1');
    filebitstring_map.insert({filebitstring_map_key,temp});
    // Above line sets the bit string to filebitstring_map for corresponding grp$filename
    string final_msg = "Done";
    send(tracker_sock,final_msg.c_str(),final_msg.length(),0);
    if(read_count == -1){
        perror("[-] Cannnot read file");
        exit(1);
    }
    memset(buffer, 0, sizeof(buffer));
    recv(tracker_sock,buffer,sizeof(buffer),0);
    string response_from_peerserver(buffer);
    cout << "[+] Reponse: " << response_from_peerserver << endl;
}

string getBitStringFromPeer(vector<string> socket_splits,string grp_file){
    string ip = socket_splits[0];
    int port = stoi(socket_splits[1]);
    int peer_socket = getPeerSocket(ip,port);
    string command = "getbitstring " + grp_file;
    send(peer_socket,command.c_str(),command.length(),0);
    char buffer[CHUNK_SIZE] = {0};
    cout << "[+] Data sent to tracker." << endl;
    recv(peer_socket,buffer,CHUNK_SIZE,0);
    string bitstring(buffer);
    return bitstring;
}

int selectRandomPeer(int n){
    srand(time(0));
    return rand() % n;
}

void printBitString(string bit_str){
    int n = bit_str.length();
    for(int i=0;i<n;i++){
        cout << bit_str[i];
    }
    cout << endl << "*******************" << endl;
}

void downloadPiece(int fd,string ip,int port,string command,int pieceno,long long int filesize,string grp_filename){
    // cout << "In download piece " << endl;
    // cout << "Thread created for piece " << pieceno << endl;
    int peer_server_socket = getPeerSocket(ip,port);
    send(peer_server_socket,command.c_str(),command.length(),0);
    char buffer[BLOCK_SIZE] = { 0 };
    long long int offset = pieceno * CHUNK_SIZE;
    long long int maxoffset = (pieceno + 1) * CHUNK_SIZE;
    int read_count = 0;
    while(offset < maxoffset){
        read_count = read(peer_server_socket, buffer, BLOCK_SIZE);
        cout << "Receiving pieceno: " << pieceno << " " << offset << " " << read_count << endl;
        if((read_count < 0) || ((offset + read_count) == filesize)){
            break;
        }   
        pwrite(fd,buffer,read_count,offset);
        offset = offset + read_count;
        memset(buffer,0,BLOCK_SIZE);
    }
    // cout << "OM" << endl;
    close(peer_server_socket);
    filebitstring_map[grp_filename].replace(pieceno,1,"1");
    printBitString(filebitstring_map[grp_filename]);
    // while((read_count = read(peer_server_socket, buffer, BLOCK_SIZE)) > 0){
    //     pwrite(fd,buffer,read_count,offset);
    //     offset = offset + read_count;

    //     memset(buffer,0,BLOCK_SIZE);
    // }

}

void downloadPBP(vector<string> grpfilename_splits,int chunkcount,map<int,vector<string>> whohaschunk_map,
    long long int filesize,string grp_filename,string download_path){
    
    vector<thread> chunk_threads;
    download_path = download_path + "/" + grpfilename_splits[0] + "$" + grpfilename_splits[1];
    // string new_filename = "./downloads/" + grpfilename_splits[0] + "$" + grpfilename_splits[1];
    string new_filename = download_path;
    int fd = open(new_filename.c_str(), O_WRONLY | O_CREAT, 0644);
    for(int i=0;i<chunkcount;i++){
        // cout << "In download PBP" << endl;
        string command = "download_piece " + to_string(i) + " " + grpfilename_splits[1];
        int peer_no = selectRandomPeer(whohaschunk_map[i].size());
        string socket_address = whohaschunk_map[i][peer_no];
        vector<string> splits = split(socket_address,'$');
        string ip = splits[0];
        string port = splits[1];
        usleep(10000);
        chunk_threads.push_back(thread(downloadPiece,fd,ip,stoi(port),command,i,filesize,grp_filename));
    }
    for(auto it=chunk_threads.begin();it!=chunk_threads.end();it++){
        if(it->joinable()){
            it->join();
        }
    }
}

void downloadFileEvent(string user_command,vector<string> command_splits){
    // following map is used by peer client
    // Values format: <int>,<list of ip$port>
    map<int,vector<string>> whohaschunk_map;

    string download_path = command_splits[3];
    user_command = user_command + " " + current_user;
    send(tracker_sock,user_command.c_str(),user_command.length(),0);
    char buffer[CHUNK_SIZE] = {0};
    recv(tracker_sock,buffer,CHUNK_SIZE,0);
    string buffer_parsed(buffer);
    cout << "[*] Download response from tracker: " << endl;
    cout << buffer_parsed << endl;
    vector<string> buffer_parsed_splits = split(buffer_parsed,' ');

    long long int filesize = stoll(buffer_parsed_splits.back());
    // Above buffer_parsed_splits contains list of sockets of form <IP>$<PORT> and filesize at the end
    vector<string> sock_address_list;
    string grp_file = command_splits[1] + "$" + command_splits[2];
    int loop_count = 0;
    buffer_parsed_splits.pop_back();
    int bitstring_len = 0;
    cout << "SOCKS: " << filesize << endl;
    for(auto sock : buffer_parsed_splits){
        cout << sock << endl;
    }
    for(string socket : buffer_parsed_splits){
        // cout << "India" << endl;
        sock_address_list.push_back(socket);
        vector<string> sock_splits = split(socket,'$');
        string bitstring = getBitStringFromPeer(sock_splits,grp_file);
        // string bitstring(920,'1');
        cout << "BITSTRING: " << bitstring.length() << endl;
        bitstring_len = bitstring.length();
        // socksplits has <ip>,<port>
        // string bitstring = getBitStringFromPeer(sock_splits[0],sock_splits[1]);
        // Here assuming hardcoding bitstring later should change.
        
        for(int i=0;i<bitstring.length();i++,loop_count++){
            // cout << loop_count << endl;
            char bit = bitstring[i];
            if(bit == '1'){
                if(whohaschunk_map.find(i) != whohaschunk_map.end()){
                    whohaschunk_map[i].push_back(socket);
                }
                else{
                    vector<string> socket_list;
                    socket_list.push_back(socket);
                    whohaschunk_map.insert({i,socket_list});
                }
            }
        }
    }
    cout << "OUTSIDE " << endl;
    string check_bitstring(bitstring_len,'0');
    filebitstring_map.insert({grp_file,check_bitstring});
    file_sockaddr_map.insert({grp_file,sock_address_list});
    downloadPBP(split(grp_file,'$'),bitstring_len,whohaschunk_map,filesize,grp_file,download_path);
    // thread th1(downloadPBP,split(grp_file,'$'),920,whohaschunk_map);
    // th1.join();
    cout << "[+] File downloaded successfully." << endl;
}

// void pdownload(string user_command, vector<string> command_splits){
//     thread t(downloadFileEvent, ref(user_command), ref(command_splits));
//     t.detach();
// }

int main(int argc, char **argv){
    if(argc != 3){
        perror("[-] Invalid arguments.");
        exit(1);
    }
    // Creating client server and starting client server
    string socket_address = argv[1];
    string trackerinfofile_url = argv[2];
    vector<string> socketsplits = getSocketSplits(socket_address);
    CLIENT_IP = socketsplits[0];
    CLIENT_PORT = stoi(socketsplits[1]);
    thread clientserver_thread(startClientServer);

    // Connecting to tracker
    setTrackerDetails(trackerinfofile_url);
    // cout << "[*] Tracker IP: " << TRACKER_IP << endl;
    // cout << "[*] Tracker Port: " << TRACKER_PORT << endl;
    setTrackerSock();


    while(true){
        cout << "Enter user command!" << endl;
        string user_command;
        getline(cin,user_command);
        cout << "You entered " << user_command << endl;
        vector<string> command_splits = split(user_command,' ');
        string command_type = command_splits[0];
        if(command_type == "create_user"){
            createUserEvent(user_command);
        }
        else if(command_type == "login"){
            loginUserEvent(user_command,command_splits);
        }
        else if(command_type == "create_group"){
            // group name should not contain any spaces.
            createGroupEvent(user_command);
        }
        else if(command_type == "join_group"){
            joinGroupEvent(user_command);
        }
        else if(command_type == "list_groups"){
            listGroupsEvent(user_command);
        }
        else if(command_type == "list_requests"){
            listRequestsEvent(user_command,command_splits);
        }
        else if(command_type == "accept_request"){
            acceptRequestEvent(user_command,command_splits);
        }
        else if(command_type == "upload_file"){
            uploadFileEvent(user_command,command_splits);
        }
        else if(command_type == "download_file"){
            downloadFileEvent(user_command,command_splits);
            // pdownload(user_command,command_splits);
        }

    }
    close(tracker_fd);
    clientserver_thread.join();
}
