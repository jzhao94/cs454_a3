#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <vector>
#include <sstream>

using namespace std;	

/*
1 port for all actions?

*/

//object that stores the nature of a function parameter
class sig{
public:
	int in;
	int out;
	int argtype;
	int arglength;
};

//objects that store the function name and signature of the function
class proc{
public:
	string name;
	string svrname;
	string svrport;
	vector<sig> sigs;
};

//object to store the sockfd and the sockaddr and socklen_t of a server
class svrloc{
public:
	int sockfd;
	struct sockaddr_storage addr;
	socklen_t addrLen;
};

//compares if two sigs are the same
bool equalsig(const sig &a, const sig &b){ 
	if (a.in != b.in){
		cout<<"a.in: "<<a.in<<" b.in: "<<b.in<<endl;
		return false;
	}
	if (a.out != b.out){
		cout<<"a.out: "<<a.out<<" b.out: "<<b.out<<endl;
		return false;
	}
	if (a.argtype != b.argtype){
		cout<<"a.argtype: "<<a.argtype<<" b.argtype: "<<b.argtype<<endl;
		return false;
	}
	if (a.arglength != b.arglength){
		if (a.arglength == 0 ||
			b.arglength == 0){
			cout<<"a.arglength: "<<a.arglength<<" b.arglength: "<<b.arglength<<endl;
			return false;
		}
	}

	return true;
}

//compares if two procs are the exact same
bool issame(const proc &a, const proc &b){
	
	//bool same = true;
	//bool found = false;
	if (a.name.compare(b.name) != 0){
		cout<<"a name: "<<a.name<<" b name: "<<b.name<<endl;
		return false;
	}
	if ((a.sigs).size() != (b.sigs).size()){
		cout<<"a sigs size: "<<(a.sigs).size()<<" b sigs size: "<<(b.sigs).size()<<endl;
		return false;
	}
	for (int i = 0; i < (a.sigs).size(); i++){
		if (!equalsig(a.sigs[i], b.sigs[i])){

			return false;
		}
	}
	return true;
}

//database to store location info of all the servers and their available procedures
map<int, vector<proc> > dbase;

//data struct that keeps a round robin scheduling system for the servers
vector<int> rrobin;

void print_proc_names(){
	cout<<"*****************************************************"<<endl;
	map<int, vector<proc> >::iterator it;

	for ( it = dbase.begin(); it != dbase.end(); it++ )
	{
	    for(int i = 0; i < it->second.size(); i++){
	    	cout<<"Name: "<<(it->second)[i].name<<endl;
	    }
	    
	}
	cout<<"*****************************************************"<<endl;
}

#define BACKLOG 42

//disclaimer, only handling ip4
//source code from Beej's Guide to Network Programming

// get port, IPv4 or IPv6:
in_port_t get_in_port(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return (((struct sockaddr_in*)sa)->sin_port);
	}

	return (((struct sockaddr_in6*)sa)->sin6_port);
}

void flush(char *buf){
	for (int i = 0; i < 32767; i++){
		buf[i] = '\0';
	}
}

int main (){
	fd_set allCon; //list of all current socket(file descriptor) connections
	fd_set curCon; //temp for select()
	int numOfCons;
	
	int listener;//listening socket
	int acceptedSocket;//socket used for accept()
	
	struct sockaddr_storage clientAddr; //client address
	socklen_t addrLen;
	
	int bufferSize = 32768;
	char buffer[bufferSize]; //client data buffer, prob need to be bigger
	int nbytes;
	
	int yes = 1;//setsockopt()
	int i, retVal;
	
	struct addrinfo addrHolder, *serverInfo, *ports;
	
	//clear fd sets
	FD_ZERO(&allCon);
	FD_ZERO(&curCon);
	
	//get and set socket
	memset(&addrHolder, 0, sizeof(addrHolder));
	addrHolder.ai_family = AF_INET;
	addrHolder.ai_socktype = SOCK_STREAM;
	addrHolder.ai_flags = AI_PASSIVE;

	retVal = getaddrinfo(INADDR_ANY, "0", &addrHolder, &serverInfo);
	if ( retVal != 0) {
	//	cerr<<"server: getaddrinfo: "<<gai_strerror(retVal)<<endl;
		return 1;
	}

	for(ports = serverInfo; ports != NULL; ports = ports->ai_next) {
	    listener = socket(ports->ai_family, ports->ai_socktype, ports->ai_protocol);
		if (listener < 0) {
			//cerr<<"server listener socket fail to open"<<endl;
			continue;
		}
		
		// set socket options
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		
		if (bind(listener, ports->ai_addr, ports->ai_addrlen) < 0) {
			close(listener);
			continue;
		}
		
		break;
	}
	
	if( ports == NULL ){
		//fail to find
		return 1;
	}
	//done with presetting
	freeaddrinfo(serverInfo);
	
	//listen
	if ( listen(listener, BACKLOG) == -1 ){
		return 1;
	}

	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	strcat(hostname, ".student.cs.uwaterloo.ca");
	cout<<"BINDER_ADDRESS "<<hostname<<endl;
	setenv("BINDER_ADDRESS", hostname, 0);

	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(listener, (struct sockaddr *)&sin, &len);
	int portnum = ntohs(sin.sin_port);
	
	cout<<"BINDER_PORT "<<portnum<<endl;
	setenv("BINDER_PORT", (std::to_string(portnum)).c_str(), 0);
	
	//TODO - set rpc binder values before connections handling

	//set fd trackers
	FD_SET(listener, &allCon);
	numOfCons = listener;
	
	//connection handling
	while(true){ // infinite loop?
		curCon = allCon; // copy all connections
		select(numOfCons+1, &curCon, NULL, NULL, NULL );
		
		//process data in existing connections 
		for(i = 0; i <= numOfCons; i++){
			if(FD_ISSET(i, &curCon)) {	//connection! hope we have data
				//new connection?
				if( i == listener ){
					addrLen = sizeof(clientAddr);
					acceptedSocket = accept(listener,
									(struct sockaddr*)&clientAddr,
									&addrLen);

					//cout<<acceptedSocket<<
					FD_SET(acceptedSocket, &allCon); //put into allCon
					// keep track of how many connections we're got
					if(acceptedSocket > numOfCons) 
						numOfCons = acceptedSocket;
				}
				//else client data handling
				else{
					//did client close/crash/die/bug out?
					if( (nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0 ){
						flush(buffer);
						close(i);
						FD_CLR(i, &allCon);
					}
					//rpc handeling
					else{						
						// receiving actual data
						buffer[nbytes] = '\0';
						string temprecv(buffer);
						stringstream message;
						string command;
						message.str(temprecv);
						//flush(buffer);

						message >> command;
						if (command == "terminate"){
							cout << "Terminate command received" << endl;
							close(i);
							FD_CLR(i, &allCon);
							for (int j = 0; j <= rrobin.size(); j++){
								cout << "terminating server socket fd: " << rrobin[j] << endl;
								if (true) {
									if (true){
										string term_msg = "terminate";
										
										send(rrobin[j], term_msg.c_str(), term_msg.size(), 0);
										cout << "terminate message sent" << endl;
										flush(buffer);
										if ((nbytes = recv(rrobin[j], buffer, sizeof buffer, 0)) > 0){
											cout << "msg received" << endl;
											buffer[nbytes] = '\0';
											string retermsg(buffer);
											flush(buffer);
											if (retermsg == "terminated"){
												cout << "terminate confirmed" << endl;
												close(rrobin[j]);
											}
										}
										cout << "do we get here?" << endl;
									}
								}
								FD_CLR(rrobin[j], &allCon);
							}
							close(listener);
							FD_CLR(listener, &allCon);
							cout << "binder terminated" << endl;
							return 0;
						}
						else if (command == "register"){
							cout << "register command received" << endl;

							cout << buffer << endl;
							flush(buffer);
							
							bool can_register = true;
							bool has_svr = false;
							int num_params;
							proc *p = new proc();
							string name, svrname, svrport;
							message >> name >> svrname >> svrport;
							cout << "name: " << name << endl;
							cout << "machine name: " << svrname << endl;
							cout << "port: " << svrport << endl;
							p->name = name;
							p->svrname = svrname;
							p->svrport = svrport;
							message >> num_params;
							cout << num_params << " parameters:" << endl;
							for (int j = 0; j < num_params; j++){
								int in, out, argtype, arglength;
								message >> in >> out >> argtype >> arglength;
								cout << "parameter " << j + 1 << ":" << endl;
								cout << "in: " << in << endl;
								cout << "out: " << out << endl;
								cout << "argtype: " << argtype << endl;
								cout << "arglength: " << arglength << endl;
								sig *tempsig = new sig();
								tempsig->in = in;
								tempsig->out = out;
								tempsig->argtype = argtype;
								tempsig->arglength = arglength;
								(p->sigs).push_back(*(tempsig));
							}
							if (rrobin.size() == 0){
								vector<proc> tempproc;
								tempproc.push_back(*(p));
								dbase[i] = tempproc;;
								rrobin.push_back(i);
								string snd = "register_success";
								cout << "first function, " << snd << endl;
								send(i, snd.c_str(), snd.size(), 0);
							}
							else{
								for (int j = 0; j < rrobin.size(); j++){
									if (rrobin[j] == i){
										has_svr = true;
										break;
									}
								}
								if (!has_svr){
									vector<proc> *tempproc;
									tempproc->push_back(*(p));
									dbase[i] = *tempproc;
									rrobin.push_back(i);
									string snd = "register_success";
									cout << snd << endl;
									send(i, snd.c_str(), snd.size(), 0);
								}
								else{
									for (int j = 0; j < dbase[i].size(); j++){
										if (issame(dbase[i][j], *(p))){
											can_register = false;
											break;
										}
									}
									if (can_register){
										dbase[i].push_back(*(p));
										string snd = "register_success";
										cout << snd << endl;
										send(i, snd.c_str(), snd.size(), 0);
									}
									else{
										string snd = "register_fail";
										cout << snd << endl;
										send(i, snd.c_str(), snd.size(), 0);
									}
								}
							}
							cout << "register done" << endl;
						}
						else if (command == "call"){
							print_proc_names();
							flush(buffer);
							cout << "call command received" << endl;
							if (rrobin.size() == 0){
								string snd = "call_fail: no servers registered";
								//cout << snd << ": no servers registered" << endl;
								send(i, snd.c_str(), snd.size(), 0);
								close(i);
								FD_CLR(i, &allCon);
							}
							else{
								bool found = false;
								string name;
								int num_params;
								proc *p = new proc();
								message >> name;
								message >> num_params;
								p->name = name;
								cout << "name: " << name << endl;
								cout << num_params << " parameters" << endl;
								for (int j = 0; j < num_params; j++){
									cout << "parameter " << j + 1 << ":" << endl;
									int in, out, argtype, arglength;
									message >> in >> out >> argtype >> arglength;
									cout << "in: " << in << endl;
									cout << "out: " << out << endl;
									cout << "argtype: " << argtype << endl;
									cout << "arglength: " << arglength << endl;
									sig *tempsig= new sig();
									tempsig->in = in;
									tempsig->out = out;
									tempsig->argtype = argtype;
									tempsig->arglength = arglength;
									(p->sigs).push_back(*(tempsig));
								}
								for (int j = 0; j < rrobin.size(); j++){
									//change j to k
									for (int k = 0; k < dbase[rrobin[j]].size(); k++){
										if (issame(dbase[rrobin[j]][k], *(p))){
											string snd_msg = "call_success, " + dbase[rrobin[j]][k].svrname + ", " + dbase[rrobin[j]][k].svrport;
											cout << snd_msg << endl;
											send(i, snd_msg.c_str(), snd_msg.size(), 0);
											close(i);
											FD_CLR(i, &allCon);
											int temp_fd = rrobin[j];
											rrobin.erase(rrobin.begin() + j);
											rrobin.push_back(temp_fd);
											found = true;
											break;
										}
									}
									if (found) break;
								}
								if (!found){
									string snd = "call_fail: function not found";
									cout << snd << endl;
									send(i, snd.c_str(), snd.size(), 0);
									close(i);
									FD_CLR(i, &allCon);
								}
							}
							cout << "call done" << endl;
						}
						
						//cout << command << endl;

					}
				}
			}
		}
	}
	
	return 0;
}
