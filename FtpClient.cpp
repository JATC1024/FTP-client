// FtpClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "FtpClient.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
// The one and only application object

CWinApp theApp;
using namespace std;

int main()
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			wprintf(L"Fatal Error: MFC initialization failed\n");
			nRetCode = 1;
		}
		else
		{
			// TODO: code your application's behavior here.
			try
			{
				FtpClient client("192.168.24.2");
				client.startClient();
				_getch();
			}			
			catch (int e)
			{
				if (e == INADDR_NONE)
				{
					cout << "Ftp server IP address wrong format\n";
					return 0;
				}
				else if (e == CONNECTION_CLOSED)
				{
					cout << "Connection closed\n";
					return 0;
				}
				else if (e == UNABLE_TO_CREATE_SOCKET)
				{
					cout << "Could not create socket, please try again\n";
					return 0;
				}
			}
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		wprintf(L"Fatal Error: GetModuleHandle failed\n");
		nRetCode = 1;
	}

	return nRetCode;
}

/// <summary>
/// Khoi tao dia chi ip cua server cho client
/// Neu dia chi ip khong hop le thi throw.
/// </summary>
FtpClient::FtpClient(const string & serverIP)
{
	svAddr.sin_family = AF_INET;
	svAddr.sin_port = htons(21); // Port luu theo kieu big edian.
	uint32_t ip;
	ip = inet_addr(serverIP.c_str());
	if (ip == INADDR_NONE)
		throw INADDR_NONE;
	svAddr.sin_addr.S_un.S_addr = ip;
	isPassive = false;
	localDir = "C:/FtpClient";
}

/// <summary>
/// Ham khoi tao winsock.
/// </summary>
/// <returns> True neu khoi tao thanh cong, nguoc lai false </returns>
void FtpClient::initialize() const
{
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2); // Version 2.2 https://msdn.microsoft.com/en-us/library/windows/desktop/ms742213(v=vs.85).aspx.
	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err)
		throw err;	
}

/// <summary>
/// Ham ket noi den ftp server.
/// </summary>
/// <param name = "serverIP"> Dia chi IP cua ftp server </param>
/// <returns> True neu ket noi thanh cong, false nguoc lai </returns>
bool FtpClient::connectToServer()
{
	//throw "Chua xu ly 120, FtpClient::connectToServer()";
	sockCmd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Tao socket de ket noi toi ftp server.
	if (sockCmd < 0) // Ko tao duoc socket.
		throw INVALID_SOCKET;
	if (connect(sockCmd, (sockaddr*)&svAddr, sizeof(sockaddr)) == SOCKET_ERROR) // Ket noi den ftp server va kiem tra xem ket noi thanh cong hay that bai.			
		throw SOCKET_ERROR;
	// Nhan reply tu ftp server.
	int reply = recvReply();	
	if (reply == 220) // 220 Service ready for new user.
		return true;
	return false;
}

/// <summary>
/// Ham dang nhap vao ftp server.
/// </summary>
/// <returns> True neu dang nhap thanh cong, nguoc lai false </returns>
bool FtpClient::loginToServer() const
{
	string userName, passWord;
	cout << "Enter username: ";
	cin >> userName;
	sendCmd("USER " + userName);
	int reply = recvReply();	
	cout << "Enter password: ";
	cin >> passWord;	
	sendCmd("PASS " + passWord); // Gui password.
	reply = recvReply();
	return reply == 230 || reply == 202;
}

/// <summary>
/// Ham khoi tao data socket den server theo active mode
/// </summary>
void FtpClient::activeMode()
{
	int size = sizeof(sockaddr);
	if (getsockname(sockCmd, (sockaddr*)&cmdAddr, &size) == SOCKET_ERROR) // Lay dia chi cua command socket.
		throw SOCKET_ERROR;


	sockaddr_in service; // Dia chi cho data socket.
	service.sin_family = AF_INET;
	service.sin_port = 0; // Tu dong cap free port.
	service.sin_addr = cmdAddr.sin_addr;
	int listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Tao data socket.
	if (listenSocket == INVALID_SOCKET) // Khong tao duoc socket.
		throw INVALID_SOCKET;
	if (bind(listenSocket, (sockaddr*)&service, sizeof(sockaddr)) == SOCKET_ERROR)
		throw SOCKET_ERROR;
	if (getsockname(listenSocket, (sockaddr*)&service, &size) == SOCKET_ERROR) // Lay dia chi cua data socket (lay port).
		throw SOCKET_ERROR;	


	int b1 = service.sin_addr.S_un.S_un_b.s_b1; // 4 byte cua ip address.
	int b2 = service.sin_addr.S_un.S_un_b.s_b2;
	int b3 = service.sin_addr.S_un.S_un_b.s_b3;
	int b4 = service.sin_addr.S_un.S_un_b.s_b4;
	int b5 = service.sin_port & 255; // 2 byte cua port.
	int b6 = service.sin_port >> 8;
	string address = to_string(b1) + "," + to_string(b2) + "," + to_string(b3) + "," + to_string(b4) + "," + to_string(b5) + "," + to_string(b6);
	sendCmd("PORT " + address); // Gui lenh PORT.
	int reply = recvReply(); // Nhan phan hoi
	if (reply != 200)
		return;

	if (listen(listenSocket, MAX_PENDING_CONNECTION) == SOCKET_ERROR) // Lang nghe ket noi tu server.
		throw SOCKET_ERROR;
	do
	{
		size = sizeof(sockaddr);
		sockData = accept(listenSocket, (sockaddr*)&service, &size);
		if (sockData == INVALID_SOCKET)
			throw INVALID_SOCKET;
	} while (service.sin_port != htons(20)); // Accept den khi gap port 20.
}

/// <summary>
/// Ham nhan reply tu server
/// Neu duong lenh khong con ket noi thi throw exception.
/// </summary>
/// <returns> Reply cua server </returns>
int FtpClient::recvReply() const
{
	char * buffer = new char[MAX_REPLY_LENGTH];
	int length = recv(sockCmd, buffer, MAX_REPLY_LENGTH, 0);
	if (length == 0)
		throw CONNECTION_CLOSED;
	if (length == SOCKET_ERROR)
		throw SOCKET_ERROR;
	buffer[length] = '\0';
	cout << buffer;
	return stoi(string(buffer, buffer + 3));
}

void FtpClient::startClient()
{
	this->initialize();
	if (this->connectToServer() == false)
	{
		cout << "Ftp client connectToServer failed\n";
		return;
	}
	if (this->loginToServer() == false)	
		return;	

	this->dir("Hao");	
}

/// <summary>
/// Ham liet ke cac thu muc, tap tin tren server
/// <summary>
/// <param name = "path"> Duong dan can liet ke cac thu muc </param>
void FtpClient::dir(const string & path)
{
	/*if (isPassive)
		passiveMode();
	else*/
		activeMode();
	sendCmd("NLST " + path);
	int reply = recvReply();
	if (reply == 125 || reply == 150) // Bat dau nhan du lieu.
	{
		string buffer = recvData();
		reply = recvReply();
		if (reply == 226 || reply == 250)
			cout << buffer;
	}
	dataCleanUp();
}

/// <summary>
/// Ham gui lenh den server
/// <summary>
/// <param name = "cmd"> Lenh can gui </param>
void FtpClient::sendCmd(const string & cmd) const
{	
	string ftpCmd = cmd + "\r\n";
	int length = send(sockCmd, ftpCmd.c_str(), ftpCmd.size(), 0);
	if (length == SOCKET_ERROR)
		throw SOCKET_ERROR;
}

/// <summary>
/// Ham nhan du lieu tu server
/// </summary>
/// <returns> Mot string trong do moi ki tu la 1 byte du lieu </returns>
string FtpClient::recvData() const
{
	string res;
	char * buffer = new char[MAX_BUFFER_SIZE];
	int length;
	do
	{
		length = recv(sockData, buffer, MAX_BUFFER_SIZE, 0);
		if (length == 0) // Server ngat socket, truyen du lieu xong.
			break;
		if (length == SOCKET_ERROR)
			throw SOCKET_ERROR;
		res += string(buffer, buffer + length);
	} while (length > 0);
	delete[] buffer;
	return res;
}
