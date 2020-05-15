#define UINT16 unsigned short int

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <tchar.h>
#define WINDOWS 1
#endif// _WIN32

#ifdef __unix__
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>

#define WINDOWS 0
#endif // __unix__

#include <thread>
#include <iostream>
#include <queue>
#include <string>

#include <locale>
#include <codecvt>
#include <stdint.h>
std::queue<std::string> OutputLogQueue;

void ManageRequest(int, const char*, UINT16);
void ServerClientTunnel(int, int);
void ClientServerTunnel(int, int);
void StartOutputStream();
int TamperInitialRequest(char*);
std::string ExtractHostFromRequest(std::string);
bool InitializeProxyServer(const char*, UINT16, UINT16, int);
void InitializeServerTunnel(int, int);
void PartialPacketSend(int, char*, int, std::string);
std::string GenerateRequest(std::string, int);

int main(int argc, char** argv)
{
	
    	

	if (argc < 3)	
	{
		std::cout << "Usage: " << argv[0] << " [GateWayIP] [GateWayPort]\n";
		return 1;
	}
	
	while (true)
	{
		
		if (InitializeProxyServer(argv[1], atoi(argv[2]), 8080, 65535) == false)
		{
			std::cout << "Couldn't bind, Waiting for 10sec\n";
			sleep(10);
		}
	}
}


bool InitializeProxyServer(const char* GatewayIP, UINT16 GatewayPort, UINT16 ListenerPort, int BufferSize)
{
	//std::thread(StartOutputStream).detach();
	#ifdef WIN32
        	WSADATA WSAData;
            WSAStartup(MAKEWORD(2, 0), &WSAData);
    #endif // WINDOWS
	
	signal(SIGPIPE, SIG_IGN);
	int ListenerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in ListenerAddress;
	ListenerAddress.sin_family = AF_INET;
	ListenerAddress.sin_addr.s_addr = INADDR_ANY;
	ListenerAddress.sin_port = htons(ListenerPort);
	int ListenerAddressSize = sizeof(ListenerAddress);
	if (0 > bind(ListenerSocket, (sockaddr*)& ListenerAddress, ListenerAddressSize))
	{
		return false;
	}
	std::cout << "Proxy started on 127.0.0.1:8080\n";
		//OutputLogQueue.push("Bound");
	listen(ListenerSocket, 1);
	while (true)
	{



		int IncomingSocket = accept(ListenerSocket, (sockaddr*)&ListenerAddress, (socklen_t*)&ListenerAddressSize);
		//OutputLogQueue.push("Connection Received");
		std::thread(ManageRequest, IncomingSocket, GatewayIP, GatewayPort).detach();
		//ManageRequest(IncomingSocket, GatewayIP, GatewayPort);
	}
	shutdown(ListenerSocket,0);
	return true;
}


void StartOutputStream()
{
	while (true)
	{
		if (!OutputLogQueue.empty())
		{
			std::cout << OutputLogQueue.front() << '\n';
			OutputLogQueue.pop();
		}
	}
}

void ManageRequest(int ClientSocket, const char* GatewayIP, UINT16 GatewayPort)
{
	int ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	std::string gatewayip = GatewayIP;

	sockaddr_in ServerAddress;
	inet_pton(AF_INET, gatewayip.c_str(), &ServerAddress.sin_addr.s_addr); //PROXY SERVER SHIT DONT FORGET IT
	ServerAddress.sin_port = htons(GatewayPort);
	ServerAddress.sin_family = AF_INET;

	if (0 == connect(ServerSocket, (sockaddr*)& ServerAddress, sizeof(ServerAddress)))
	{
		struct timeval nTimeout;
		nTimeout.tv_sec = 20000;
		//setsockopt(ClientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)& nTimeout, sizeof(struct timeval));
		//setsockopt(ServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)& nTimeout, sizeof(struct timeval));


		InitializeServerTunnel(ClientSocket, ServerSocket);

		std::thread(ClientServerTunnel, ClientSocket, ServerSocket).detach();
		std::thread(ServerClientTunnel, ClientSocket, ServerSocket).detach();
	}
}

void InitializeServerTunnel(int ClientSocket, int ServerSocket)
{
	std::string DestIP = "";
	unsigned int DestPort;
	unsigned char Buffer[512];
	int DataReceived;
	DataReceived = recv(ClientSocket, (char*)Buffer, 512, 0);

	for (int i = 0; i < 512; i++)
		Buffer[i] = -52;

	Buffer[0] = (char)5;
	Buffer[1] = (char)0;

	send(ClientSocket, (char *)Buffer, 2, 0);
	DataReceived = recv(ClientSocket, (char*)Buffer, 512, 0);

	if (Buffer[3] != 1)
	{
		std::cout << "NOT SUPPORTED REQUEST\n";
		shutdown(ClientSocket, 0);
		shutdown(ServerSocket, 0);
		return;
	}

	
		std::cout << "Connection Receive";

	for (int i = 4; i < DataReceived - 2; i++)
		DestIP += std::to_string((int)Buffer[i]) + ((i != DataReceived - 3)? "." : "");

	DestPort = (int)(Buffer[DataReceived - 2] << 8 | Buffer[DataReceived - 1]);

	std::string Request = GenerateRequest(DestIP, DestPort);
	PartialPacketSend(ServerSocket, (char*)Request.c_str(), Request.size(), DestIP);
	recv(ServerSocket, (char*)Buffer, 512, 0);

	Buffer[0] = 5;
	Buffer[1] = 0;
	Buffer[2] = 0;
	Buffer[3] = 1;
	Buffer[4] = (char)127;
	Buffer[5] = (char)0;
	Buffer[6] = 0;
	Buffer[7] = 1;
	Buffer[8] = 31;
	Buffer[9] = 144;
	send(ClientSocket, (char*)Buffer, 10, 0);
}


std::string GenerateRequest(std::string IP, int port)
{
	return "CONNECT " + IP + ":" + std::to_string(port) + " HTTP/1.1\r\nHost: " + IP + ":" + std::to_string(port) + "\r\n\r\n";
}

void ServerClientTunnel(int ClientSocket, int ServerSocket)
{
	char* Buffer = new char[300000];
	int ServerReceivedCount;
	try
	{


		do {

			ServerReceivedCount = recv(ServerSocket, Buffer, 300000, 0);
			send(ClientSocket, Buffer, ServerReceivedCount, 0);
			//OutputLogQueue.push("Bytes from Server to Client");
			std::cout << ServerReceivedCount << "Bytes from Server to Client\n";
		} while (ServerReceivedCount > 0);
		delete[] Buffer;
		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);

		//	OutputLogQueue.push("Server-Client Tunnel Ended");
	}
	catch (...)
	{
		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);
		delete[] Buffer;
		//OutputLogQueue.push("Server-Client Tunnel Failed");
	}
}


std::vector<int> FindAllSubStrings(char* str, int strsize, const char* substring, int substrsize)
{
	bool found = true;
	std::vector<int> Occurences;
	for (int i = 0; i < strsize - substrsize; i++)
	{
		found = true;
		for (int j = 0; j < substrsize; j++)
		{
			if (str[i + j] != substring[j])
			{
				found = false;
				break;
			}
		}

		if (found == true)
		{
			Occurences.push_back(i);
			found = false;
		}
	}
	Occurences.push_back(strsize - 4);
	return Occurences;
}

void PartialPacketSend(int ServerSocket, char* Buffer, int Length, std::string HostName)
{
	std::vector<int> Hotspots = FindAllSubStrings(Buffer, Length, HostName.c_str(), HostName.size());

	for (int i = 0, hotspot, sent = 0; i < Hotspots.size(); i++)
	{
		hotspot = Hotspots[i];

		send(ServerSocket, Buffer + sent, hotspot - sent + 4, 0);

		/*for (int j = 0; j < hotspot - sent; j++)
		std::cout << (Buffer + sent)[j];*/

		sent = hotspot + 4;

	}
	Hotspots.clear();
}

void ClientServerTunnel(int ClientSocket, int ServerSocket)
{

	std::string Host;
	char* Buffer = new char[300000];
	std::vector<int> Hotspots;
	try
	{

		int ClientReceivedCount;
		do
		{

			ClientReceivedCount = recv(ClientSocket, Buffer, 300000, 0);
			send(ServerSocket, Buffer, ClientReceivedCount, 0);
			//send(ServerSocket, Buffer, ClientReceivedCount, 0);


			//OutputLogQueue.push("Bytes from Client to Server");
		} while (ClientReceivedCount > 0);

		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);
		delete[] Buffer;

		//OutputLogQueue.push("Client-Server Tunnel Ended");
	}
	catch (...)
	{
		shutdown(ServerSocket, 0);
		shutdown(ClientSocket, 0);
		delete[] Buffer;
		//OutputLogQueue.push("Client-Server Tunnel Failed");
	}
}
//
//int TamperInitialRequest(char* Request)
//{
//	std::string RequestString = std::string(Request);
//	if (RequestString.find("CONNECT") > -1)
//	{
//		std::string host = ExtractHostFromRequest(RequestString);
//		while (RequestString.find(Extract))
//	}
//}
//
std::string ExtractHostFromRequest(std::string Request)
{
	if (0 == Request.find("CONNECT"))
	{
		try
		{
			int host_end = std::min(Request.find("\r", 8), Request.find(':', 8));
			std::string host = Request.substr(8, host_end - 8);
			return host;
		}
		catch (...)
		{
			return "";
		}
	}
	return "";
}
