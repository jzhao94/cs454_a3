#include "rpc.h"
#include <iostream>	
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>
#include <string>
#include <time.h>
#include <vector>
#include <cstdint>

using namespace std;

#define ARG_CHAR 1
#define ARG_SHORT 2
#define ARG_INT 3
#define ARG_LONG 4
#define ARG_DOUBLE 5
#define ARG_FLOAT 6
#define BACKLOG 10
#define ARG_INPUT 31
#define ARG_OUTPUT 30

//fetch binder address and port from somewhere
//get it to work for 1 server, then expand to more
//want to assume server can somehow remember who they are when they call rpc Register
//but prob not gonna happen

class rpcServer{
public:
	std::string addressName;
	int toClientPort;
	int toClientSocket;
	int toBinderSocket;
	bool terminate;
	int regCount;
	std::vector<skeleton>functions;
	std::vector<string>functionName;
	std::vector<std::vector<std::vector<unsigned int>>>functionArgTypes;
private:
};

static char* binder_addr;
static char* binder_port;

static rpcServer server;

static bool exitThread = false;
static fd_set allCon; //list of all current socket(file descriptor) connections
static fd_set curCon; //temp for select()
static int numOfCons;
static int bufferSize = 32768;//need to increase this to 32768
static char* out = new char[bufferSize];
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

void flush(char * buf){
	for(int i = 0; i < 32767; i++){
		buf[i]='\0';
	}
}

int clientSocketSetup(const char* addr, const char* port){
	int retVal;
	int client;
	struct addrinfo addrHolder1, *serverInfo1, *ports1;
	//open connection to binder
	memset(&addrHolder1, 0, sizeof(addrHolder1));
	addrHolder1.ai_family = AF_INET;
	addrHolder1.ai_socktype = SOCK_STREAM;
	if ((retVal = getaddrinfo(addr, port, &addrHolder1, &serverInfo1)) != 0) {
		return -1;
	}
	for(ports1 = serverInfo1; ports1 != NULL; ports1 = ports1->ai_next) {
		if ( (client = socket(ports1->ai_family, ports1->ai_socktype, ports1->ai_protocol)) == -1) {
			continue;
		}
		if (connect(client, ports1->ai_addr, ports1->ai_addrlen) == -1) {
			close(client);
			continue;
		}
		break;
	}
	if( ports1 == NULL ){//fail to find free socket to connect
		return -2;
	}
	//done with presetting connection to binder
	freeaddrinfo(serverInfo1);
	
	return client;
}

unsigned int getBits(unsigned int a, unsigned int b){
	unsigned int r = 0;
	for(int j = a; j <= b; j++){
		r |= 1 << j;
	}
	return r;
}

void processArgTypes(int* argTypes, std::vector<std::vector<unsigned int>> & processed, std::string & msg){
	int i = 0;

	while(argTypes[i] != 0){
		//int leng = argTypes[i] & 0xffff;
		//cout << "length: " << leng << " type " << ty << endl;
		//int out = argTypes[i]>>30 &0x 
		unsigned int input = ((1<<31) & argTypes[i])==0? 0:1;
		unsigned int output = ((1<<30) & argTypes[i])==0? 0:1;
		//type
		unsigned int type = getBits(16, 23) & argTypes[i];
		unsigned int ty = argTypes[i] >> 16 & 0xff;
		//length
		unsigned int length = getBits(0, 15) & argTypes[i];
		
		vector<unsigned int> argument;
		argument.push_back(input);
		argument.push_back(output);
		argument.push_back(ty);
		argument.push_back(length);

		//cout << input << " " << output << " " << ty << " " << length << endl;
		
		processed.push_back(argument);
		msg += " " + std::to_string(input) + " " + std::to_string(output) + " " + std::to_string(ty) + " " + std::to_string(length);
		i++;
	}
	return;
}

/*
#define ARG_CHAR 1
#define ARG_SHORT 2
#define ARG_INT 3
#define ARG_LONG 4
#define ARG_DOUBLE 5
#define ARG_FLOAT 6
*/
//changed, added output variable handling, 3 param to 4 param
void processArgs(void** args, vector<vector<unsigned int>> & processed, string & msg, bool care){

	for(int i = 0; i < processed.size(); i++){
		int in = processed[i][0];
		int out = processed[i][1];
		int type = processed[i][2];
		unsigned int length = processed[i][3] == 0 ? processed[i][3]:processed[i][3]-1;
		cout<<in<<", "<<out<<", "<< type<<", "<<length<<endl;;
		for(int j = 0; j <= length; j++){
			switch(type){
				case 1:
				{
					//cout<<args<<endl;
					//cout<<args[i]<<endl;
					//cout<<args[i][j]<<endl;
					char c  =  ((char*)args[i])[j];
					if(in == 0 && out == 1 && care){
						msg += " 0";
					}
					else{
						msg += " ";
						msg += c;
					}
					cout<< c;
					break;
				}
				case 2:
				{
					short s = ((short*)args[i])[j];
					if(in == 0 && out == 1 && care){
						msg += " 0";
					}
					else{
						msg += " " + std::to_string(s);
					}
					cout<< s;
					break;
				}
				case 3:
				{
					//cout<<args<<endl;
					//cout<<*(int *)args[i]<<endl;
					int in = ((int*)args[i])[j];
					if(in == 0 && out == 1 && care){
						msg += " 0";
						cerr<<"\nint\n"<<endl;
					}
					else{
						msg += " " + std::to_string(in);
					}
					cout<< in;
					break;
				}
				case 4:
				{
					long l = ((long*)args[i])[j];
					if(in == 0 && out == 1 && care){
						msg += " 0";
						cerr<<"\nlong\n"<<endl;
					}
					else{
						msg += " " + std::to_string(l);
					}
					cout<< l;
					break;
				}
				case 5:
				{
					double d = ((double*)args[i])[j];
					if(in == 0 && out == 1 && care){
						msg += " 0";
					}
					else{
						msg += " " + std::to_string(d);
					}
					cout<< d;
					break;
				}
				case 6:
				{
					float f = ((float*)args[i])[j];
					if(in == 0 && out == 1 && care){
						msg += " 0";
					}
					else{
						msg += " " + std::to_string(f);
					}
					cout<< f;
					break;
				}
			}
		}
		cout<<endl;
	}
}
//----------------------------------------------------------------------------------
/*
The server first calls rpcInit, which does two things. 
First, it creates a connection socket to be used for accepting connections from clients. 
Secondly, it opens a connection to the binder, 
	this connection is also used by the server for sending register requests to the binder
	left open as long as the server is up so that the binder knows the server is available
	

init gets lock, hold until rpcExecute is called
when blocked all rpc calls blocked, wait in waiting channel

*/
int rpcInit(){
	//0 - good execution, !0 - something somewhere went wrong
	//call rpcThatTakesStuff <- aka new function that does what rpcInit does but returns port number\
	//which is open a server client port and server BINDER port
	
	//server setup
	server.terminate = false;
	server.regCount = 0;
	
	int listener;//listening socket
	
	int yes = 1;//setsockopt()
	int i, retVal;
	
	struct addrinfo addrHolder, *serverInfo, *ports;
	
	/*
	
	IM A MOTHER FUCKING SERVER	
	
	*/
	//get and set socket
	memset(&addrHolder, 0, sizeof(addrHolder));
	addrHolder.ai_family = AF_INET;
	addrHolder.ai_socktype = SOCK_STREAM;
	addrHolder.ai_flags = AI_PASSIVE;

	retVal = getaddrinfo(INADDR_ANY, "0", &addrHolder, &serverInfo);
	if ( retVal != 0) {
		return -1;
	}

	for(ports = serverInfo; ports != NULL; ports = ports->ai_next) {
		listener = socket(ports->ai_family, ports->ai_socktype, ports->ai_protocol);
		if (listener < 0) {
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
		return -2;
	}
	//done with presetting
	freeaddrinfo(serverInfo);
	

	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	strcat(hostname, ".student.cs.uwaterloo.ca");
	//cout<<"SERVER_ADDRESS "<<hostname<<endl;
	//setenv("SERVER_ADDRESS", hostname, 0);
	server.addressName = hostname;

	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(listener, (struct sockaddr *)&sin, &len);
	int portnum = ntohs(sin.sin_port);
	//cout<<"SERVER_PORT "<<portnum<<endl;
	//setenv("SERVER_PORT", to_string(portnum).c_str(), 0);
	server.toClientSocket = listener;
	server.toClientPort = portnum;
	
	//activate port to listen
	if ( listen(server.toClientSocket, BACKLOG) == -1 ){
		return -3;
	}
	
	/*
	
	IM ALSO A MOTHER FUCKING CLIENT TOO
	
	*/
	
	binder_addr = getenv("BINDER_ADDRESS");
	binder_port = getenv("BINDER_PORT");
	server.toBinderSocket = clientSocketSetup(binder_addr, binder_port);
	
	return 0;
}

//----------------------------------------------------------------------------------
int rpcCall(char* name, int* argTypes, void** args){	
	//connection to binder, binder does shit to server, server responds to client or here????
	binder_addr = getenv("BINDER_ADDRESS");
	binder_port = getenv("BINDER_PORT");
	int client = clientSocketSetup(binder_addr, binder_port);
	
	//process argtypes and set up send msg
	string msgTemp = "";
	vector<vector<unsigned int>> processed;
	
	processArgTypes(argTypes, processed, msgTemp);
	string msg = "call ";
	msg += std::string(name);
	msg += " " + std::to_string(processed.size());
	msg += msgTemp;
	cout<<"call msg sending: ";
	cout<<msg<<endl;
	cout << "msg c_str: " << msg.c_str() << endl;
	int rety = send(client, msg.c_str(), msg.size(), 0);
	cout<<"call msg sent "<<rety<<endl;
	
	//now process msg received from server
	cout<<"call waiting for msg "<<endl;
	
	recv(client, out, bufferSize, 0);
	string check = std::string(out);
	cout<<"call receive msg "<<check<<endl;
	close(client);
	flush(out);
	string address = "";
	string portnum = "";
	int found = check.find(",");
	//cout<<check.substr(0, found)<<endl;
	//assuming msg received of the format "call_"statues", server information(address, port)"
	if(check.substr(0, found).compare("call_success") == 0){
		cout<<"CALL_SUCCESS"<<endl;
			//server exist, talk with dat server
		string serverInfo = check.substr(found+2, check.length());
		found = serverInfo.find(",");
		address = serverInfo.substr(0, found);
		cout<<"ADDRESS: " <<address<<endl;
		portnum = serverInfo.substr(found+2, serverInfo.length());
		cout<<"PORTNUM: " <<portnum<<endl;
		int newfd = clientSocketSetup(address.c_str(), portnum.c_str());

			//send stuff to the server
		string executeMsg = "";
		executeMsg += std::string(name);
		executeMsg += " " + std::to_string(processed.size());
		executeMsg += msgTemp;
		cout<<executeMsg<<", "<<executeMsg.size()<<endl;

		string msgArgs = "";
		processArgs(args, processed, msgArgs, true);
		executeMsg += msgArgs;
		cout<<executeMsg<<endl;

			string msgSize = "execute " + executeMsg + " @";//std::to_string(executeMsg.length());
			cout<<msgSize<<endl;
			
			send(newfd, msgSize.c_str(), msgSize.size(), 0);
			//int repeat = (executeMsg.length()/32768)+1;
			//cout<<"repeat "<< repeat<<endl;
			/*recv(client, out, bufferSize, 0);
			cout<<"go msg "<<out<<endl;
			for(int rep = 0; rep<repeat; rep++){
				send(client, executeMsg.c_str(), bufferSize, 0);
				cout<<rep<<endl;
				cout<<executeMsg;
				if(executeMsg.length() > bufferSize)
					executeMsg = executeMsg.substr(bufferSize, executeMsg.length());
				else
					executeMsg = "";
			}
			cout<<endl;
			*/
			//changed added marshelling
			cout<<"waiting for recv"<<endl;
			recv(newfd, out, bufferSize, 0);
			int retVal = 0;
			string executeCheck = std::string(out);
			cout<<executeCheck<<endl;
			flush(out);
			stringstream ss(executeCheck);
			string var;
			ss >> var;
			//found = executeCheck.find(",");
			if(var.compare("execute_success") == 0){
				//value stored in variable locations
				//name
				ss >> var;
				int numOfArgs;
				ss >> numOfArgs;
				vector<vector<int>> argProperties;

				int* argProp = new int[numOfArgs + 1];
				for (int i = 0; i < numOfArgs; i++){
					vector<int> *arguments = new vector<int>();
					for (int j = 0; j < 4; j++){
						int temp;
						ss >> temp;
						arguments->push_back(temp);
					}
					cout << arguments->at(0) << " " << arguments->at(1) << " " << arguments->at(2) << " " << arguments->at(3) << endl;
					argProp[i] = (arguments->at(0) << 31) | (arguments->at(1) << 30) | (arguments->at(2) << 16) | (arguments->at(3));
					argProperties.push_back(*(arguments));
				}

				argProp[numOfArgs] = 0;

				void** argValues = new void*[argProperties.size()];
				cout << "numOfArgs: " << numOfArgs << endl;
				for(int i = 0; i < numOfArgs; i++){
					void *values; 
					char *cArray;
					short *sArray;
					int *iArray;
					long *lArray;
					double *dArray;
					float *fArray;
					int type = argProperties[i][2];
					cout <<"in: "<<argProperties[i][0]<< " out: " << argProperties[i][1] << " type: " << type <<" length: "<< argProperties[i][3] << endl;
					int leng = argProperties[i][3] == 0? argProperties[i][3]:argProperties[i][3]-1;

			//change if varible is output, add address instead of whatever its pointing at
			//if (argProperties[i][0] == 1){
					switch (type){
						case 1:
						{
							cArray = new char[leng + 1];
							for (int j = 0; j <= leng; j++)
							{
								ss >> var;
								cout << "var: " << var << endl;
							char f1 = (var.c_str())[0];// = std::stoi(data.substr(0, found));
							cArray[j] = f1;
							cout<<"cArray at "<<j<<" " << cArray[j]<<endl;
							//data = data.substr(found+1, data.length());
						}
						values = (void*)cArray;
						break;
					}
					case 2:
					{
						sArray = new short[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							short f1;// = (short)std::stoi(data.substr(0, found));
							ss >> var;
							cout << "var: " << var << endl;
							f1 = (short)std::stoi(var);
							sArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)sArray;
						break;
					}
					case 3:
					{
						iArray = new int[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							int f1;// = std::stoi(data.substr(0, found));
							ss >> var;
							f1 = std::stoi(var);
							iArray[j] = f1;
							cout << "var: " << var << " " << iArray[j] << endl;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)iArray;
						//cout<<*(int*)values<<endl;
						break;
					}
					case 4:
					{
						lArray = new long[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							long f1;// = (long)std::stoi(data.substr(0, found));
							ss >> var;
							cout << " var: " << var << endl;
							f1 = (long)std::stoi(var);
							lArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)lArray;
						break;
					}
					case 5:
					{
						dArray = new double[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							double f1;// = (double)std::stoi(data.substr(0, found));
							ss >> var;
							cout << "var: " << var << endl;
							f1 = (double)std::stoi(var);
							dArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)dArray;
						break;
					}
					case 6:
					{
						fArray = new float[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							float f1;// = (float)std::stoi(data.substr(0, found));
							ss >> var;
							cout << "var: " << var << endl;
							f1 = (float)std::stoi(var);
							fArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)fArray;
					//float f1 = std::stoi(data.substr(0, found));
					//values[j] = (void)f1;
						break;
					}
				}
				argValues[i] = values;
			//}
			}

			*args = argValues[0];
			retVal = 0;
		}
		else{
				//execute failed for whatever reason, should never happen
			ss>>retVal;
			//retVal = std::stoi(executeCheck.substr(found+2, executeCheck.length()));
		}
		close(newfd);
		return retVal;
	}
	else if(check.substr(0, found).compare("call_warning") == 0){
			//warnings!, do something
		return 1;
	}
	//else no server, call failed, we fucked so -1
	return -1;
}

//----------------------------------------------------------------------------------
int rpcRegister(char* name, int* argTypes, skeleton f){
	cerr << "rpc: register"<<endl;
	//f is address to the skeleton of name
	//send stuff to the server through the binder;
	int i = 0;

	//ArgType processing
	string msgTemp = "";
	vector<vector<unsigned int>> processed;
	processArgTypes(argTypes, processed, msgTemp);
	//cout << "processed size " << processed.size() << endl;

	//msg to send
	string msg = "register ";
	//add funtion name
	msg += " " + std::string(name);
	//server name
	msg += " " + server.addressName;
	//server port
	msg += " " + std::to_string(server.toClientPort);
	//add args
	msg += " " + std::to_string(processed.size());
	msg += msgTemp;

		//cout<<"toclientSocket: "<<server.toClientSocket<<endl;
		//cout<<"toclientPort: "<<server.toClientPort<<endl;
	cout << "rpc reg: "<<msg << endl;

	//send binder fucntion name and args
	int count = send(server.toBinderSocket, msg.c_str(), msg.length(), 0);
		//cout<<count<<endl;
	server.regCount++;
	//response from binder

	pthread_mutex_lock( &mutex1 );

	//this here is getting skipped
	recv(server.toBinderSocket, out, bufferSize, 0);
	cout<<out<<endl;
	server.regCount--;
	//check the msg
	//expect msg to be of the format: register #
	string check = std::string(out);
	flush(out);
	if(check.compare("register_success") == 0){	
			//cerr<<" rpcRegister check good "<<check<<endl;
		//store in server
		server.functions.push_back(f);
		server.functionName.push_back(std::string(name));
		server.functionArgTypes.push_back(processed);
		//nothing went wrong we good;
		pthread_mutex_unlock( &mutex1 );
		return 0;
	}
	//something went wrong, most likely a copy of the function already exist
	pthread_mutex_unlock( &mutex1 );
	return 1;
}

//----------------------------------------------------------------------------------
void *s2b(void* ptr){
	cout << "rpcCall execute terminate" << endl;
	recv(server.toBinderSocket, out, bufferSize, 0);
	cout << "execute terminate recieved "<< out << endl;
	string check = std::string(out);
	flush(out);
	if(check.compare("terminate")==0){
		cout<<"terminated !!!!"<<endl;
		//send terminated msg
		string msg = "terminated";
		send(server.toBinderSocket, msg.c_str(), msg.length(), 0);
		//close all ports, assumed that when calling terminate, all rpcCalls have been resolved
		close(server.toClientSocket);
		close(server.toBinderSocket);
		exitThread = true;
		pthread_exit(NULL);
	}
	else{
		//binder only communicate with server when it wants to exit during execute
		//thus all other messages are ignored
	}
}
void *c2s (void* ptr){
	cout << "rpcCall execute call" << endl;
	int newfd = (intptr_t)ptr;

	flush(out);
	string msg = "";
	int numbytes = 1;
	while((numbytes=recv(newfd, out, bufferSize, 0))>0){
		out[numbytes] = '\0';

		cout<<"received: "<<out<<endl;
		string temp = std::string(out);

		msg += std::string(out);
		if (msg.back() == '@'){
			//msg = msg.substr(0, msg.length() - 1);
			break;
		}
		flush(out);
	}
	//cout<<out<<endl;
	//process the data
	cout << "rpcCall execute call check: "<<msg << endl;
	flush(out);
	int found = msg.find(" ");
	string data = "";
	if(msg.compare(0, found, "execute") == 0){
		cout<<"execute skeles"<<endl;
		stringstream ss(msg);
		string var;
		ss >> var;
		string funcName;
		ss >> funcName;
		int numOfArgs;
		ss >> numOfArgs;
		vector<vector<int>> argProperties;

		int* argProp = new int[numOfArgs + 1];
		for (int i = 0; i < numOfArgs; i++){
			vector<int> *arguments = new vector<int>();
			for (int j = 0; j < 4; j++){
				int temp;
				ss >> temp;
				arguments->push_back(temp);
			}
			cout << arguments->at(0) << " " << arguments->at(1) << " " << arguments->at(2) << " " << arguments->at(3) << endl;
			argProp[i] = (arguments->at(0) << 31) | (arguments->at(1) << 30) | (arguments->at(2) << 16) | (arguments->at(3));
			argProperties.push_back(*(arguments));
		}

		argProp[numOfArgs] = 0;
		cout<<"arg properties: "<<endl;

		for (int i = 0; i < argProperties.size(); i++){
			for (int j = 0; j < 4; j++){
				cout << argProperties[i][j];
			}
			cout<< endl;
			int t = argProp[i];
				//cout<<t<<endl;
		}
		/*int rep = (std::stoi(msg.substr(found+1, msg.length()))/32768)+1;
		string goMsg = "go";
		//send(server.toClientSocket, goMsg.c_str(), goMsg.length(), 0);
		for(int i = 0; i< rep; i++){
			recv(server.toClientSocket, out, bufferSize, 0);
			data += std::string(out);
		}*/
		/*vector<vector<int>> argProperties;
		found = msg.find(" ");
		//data: name numofArgs ...........
		string exe = msg.substr(0, found);
		cout << exe << ", left: ";

		data = msg.substr(found + 1, msg.length());
		cout << data << endl;
		found = msg.find(" ");
		string funcName = data.substr(0, found);
		cout << funcName << ", left ";		
		data = data.substr(found + 1, data.length());
		cout << data << endl;

		found = msg.find(" ");
		//data: numOfArgs arg1_properties .......
		cout << "#ofArgs " << data.substr(0, found) << endl;
		int numOfArgs = std::stoi(data.substr(0, found));
			
		data = data.substr(found+1, data.length());
		//data: arg1_properties arg2_properties .......
		for(int i = 0; i < numOfArgs; i++){
			vector<int> arguments;
			for(int j = 0; j < 4; j++){
				found = msg.find(" ");
				arguments.push_back(std::stoi(data.substr(0,found)));
				if (found + 1 <= data.length())
					data = data.substr(found+1, data.length());
			}
			cout << arguments.at(0) << " " << arguments.at(1) << " " << arguments.at(2) << " " << arguments.at(3) << endl;
			argProp[i] = (arguments.at(0)<<31)|(arguments.at(1)<<30)|(arguments.at(2)<<16)|(arguments.at(3));
			argProperties.push_back(arguments);
		}
		argProp[numOfArgs] = 0;*/
		
		//data: arg1_values arg2_values ...
		//change (void*)&arrays to (void*)arrays
		void** argValues = new void*[argProperties.size()];
		cout << "numOfArgs: " << numOfArgs << endl;
		for(int i = 0; i < numOfArgs; i++){
			void *values; 
			char *cArray;
			short *sArray;
			int *iArray;
			long *lArray;
			double *dArray;
			float *fArray;
			int type = argProperties[i][2];
			cout <<"in: "<<argProperties[i][0]<< " out: " << argProperties[i][1] << " type: " << type <<" length: "<< argProperties[i][3] << endl;
			int leng = argProperties[i][3] == 0? argProperties[i][3]:argProperties[i][3]-1;

			//change if varible is output, add address instead of whatever its pointing at
			//if (argProperties[i][0] == 1){
			switch (type){
				case 1:
				{
					cArray = new char[leng + 1];
					for (int j = 0; j <= leng; j++)
					{
						ss >> var;
						cout << "var: " << var << endl;
							char f1 = (var.c_str())[0];// = std::stoi(data.substr(0, found));
							cArray[j] = f1;
							cout<<"cArray at "<<j<<" " << cArray[j]<<endl;
							//data = data.substr(found+1, data.length());
						}
						values = (void*)cArray;
						break;
					}
					case 2:
					{
						sArray = new short[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							short f1;// = (short)std::stoi(data.substr(0, found));
							ss >> var;
							cout << "var: " << var << endl;
							f1 = (short)std::stoi(var);
							sArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)sArray;
						break;
					}
					case 3:
					{
						iArray = new int[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							int f1;// = std::stoi(data.substr(0, found));
							ss >> var;
							f1 = std::stoi(var);
							iArray[j] = f1;
							cout << "var: " << var << " " << iArray[j] << endl;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)iArray;
						//cout<<*(int*)values<<endl;
						break;
					}
					case 4:
					{
						lArray = new long[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							long f1;// = (long)std::stoi(data.substr(0, found));
							ss >> var;
							cout << " var: " << var << endl;
							f1 = (long)std::stoi(var);
							lArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)lArray;
						break;
					}
					case 5:
					{
						dArray = new double[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							double f1;// = (double)std::stoi(data.substr(0, found));
							ss >> var;
							cout << "var: " << var << endl;
							f1 = (double)std::stoi(var);
							dArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)dArray;
						break;
					}
					case 6:
					{
						fArray = new float[leng + 1];
						for (int j = 0; j <= leng; j++)
						{
							float f1;// = (float)std::stoi(data.substr(0, found));
							ss >> var;
							cout << "var: " << var << endl;
							f1 = (float)std::stoi(var);
							fArray[j] = f1;
						//data = data.substr(found+1, data.length());
						}
						values = (void*)fArray;
					//float f1 = std::stoi(data.substr(0, found));
					//values[j] = (void)f1;
						break;
					}
				}
				argValues[i] = values;
			//}
			}
			
			int result;
		//find function from server.functions
			for(int i = 0; i < server.functions.size(); i++){
				if(server.functionName[i].compare(funcName) == 0){
					cout<<"skeleton found! "<<server.functionName[i]<<endl;
					result = (server.functions[i])(argProp, argValues);
					cout<<" execution result: "<<result<<endl;
					//for(int j = 0; j<argValues.size(); j++)
					//	cout<<argValues[i][j]<<endl;
					break;
				}
			}
			if(result != 0){
				string str = "execute_fail " + std::to_string(result);
				send(newfd, str.c_str(), str.length(), 0);
			}
			else{
				//change, marshal response
				string str = "";
				if (argProperties[0][1] == 1){
				//of format: EXECUTE_SUCCESS name argTypes args
					str = "execute_success " + funcName + " " + std::to_string(argProperties.size());
					string temp;
					vector< vector<unsigned int> > processed;

					processArgTypes(argProp, processed, temp);
					str+=temp;

					temp = "";
					processArgs(argValues, processed, temp, false);
					str+=temp;
					cout<<str<<endl;

				}
				else{
					str = "execute_fail -1";
				}
				cout<<"execute replay msg: "<<str<<endl;
				send(newfd, str.c_str(), str.length(), 0);
				//cout<<"elseend"<<endl;
			}
		}
		close(newfd);
	}

	int rpcExecute(){
		cerr << "execute" << endl;
	//both ports are active
		pthread_t stob, ctos;
		int  iret1, iret2;
		void* ignored;

	//clear fd sets
		FD_ZERO(&allCon);
		FD_ZERO(&curCon);

	int listener = server.toClientSocket;//listening socket
	int listener1 = server.toBinderSocket;
	//set fd trackers
	FD_SET(listener, &allCon);
	FD_SET(listener1, &allCon);
	numOfCons = listener + listener1;
	int acceptedSocket;//socket used for accept()
	
	struct sockaddr_storage clientAddr; //client address
	socklen_t addrLen;
	
	//wait for all reg to finish
	while(server.regCount != 0){}

	//connection handling
	//change from while(true) to while(!exitThread)
	while(!exitThread){ // infinite loop?
		curCon = allCon; // copy all connections
		select(numOfCons+1, &curCon, NULL, NULL, NULL );
		
		//process data in existing connections 
		for(int i = 0; i <= numOfCons; i++){
			if(FD_ISSET(i, &curCon)) {	//connection! hope we have data
				//new connection?
				//cout << "connection" << endl;
				if( i == listener ){
					cout << "connection!" << endl;
					addrLen = sizeof(clientAddr);
					acceptedSocket = accept(listener,
						(struct sockaddr*)&clientAddr,
						&addrLen);
					FD_SET(acceptedSocket, &allCon); //put into allCon
					// keep track of how many connections we're got
					if(acceptedSocket > numOfCons) 
						numOfCons = acceptedSocket;
				}
				//else client data handling
				else{
					cerr<<"execute data handling"<<endl;
					//thread2, client to server
					iret2 = pthread_create( &ctos, NULL, c2s, (void*) acceptedSocket );
					if(iret2){
						return EXIT_FAILURE;
					}
					pthread_join( ctos, NULL );
					//thread1, server to binder
					iret1 = pthread_create( &stob, NULL, s2b, (void*) ignored );
					if(iret1){
						return EXIT_FAILURE;
					}
					//pthread_join( stob, NULL);

					if(exitThread){
						pthread_cancel(ctos);
						pthread_cancel(stob);
						//break;
					}
				}
			}
		}
	}
	
	return 0;
}

//----------------------------------------------------------------------------------
int rpcTerminate(){
	
	binder_addr = getenv("BINDER_ADDRESS");
	binder_port = getenv("BINDER_PORT");
	int client = clientSocketSetup(binder_addr, binder_port);
	
	string msg = "terminate";
	send(client, msg.c_str(), msg.length(), 0);
	//close client
	close(client);
	
	return 0;
}

//----------------------------------------------------------------------------------
//bounse
int rpcCacheCall(char* name, int* argTypes, void** args){
	return 0;
}

//int main(){
//	return 0;
//}