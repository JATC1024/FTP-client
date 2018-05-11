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

int main(int argc, char ** argv)
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
				if (argc < 2) throw NOT_ENOUGH_ARG;
				FtpClient client(argv[1]);
				client.startClient();
			}
			catch (int e)
			{
				FtpClient::printError(e);
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
		throw INVALID_ADDRESS;
	svAddr.sin_addr.S_un.S_addr = ip;
	isPassive = false;
	lcd("C:/FtpClient");
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
	sockCmd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Tao socket de ket noi toi ftp server.
	if (sockCmd == INVALID_SOCKET) // Ko tao duoc socket.
		throw INVALID_SOCKET;
	int t;
	if (t = connect(sockCmd, (sockaddr*)&svAddr, sizeof(sockaddr)) == SOCKET_ERROR) // Ket noi den ftp server va kiem tra xem ket noi thanh cong hay that bai.			
		throw SOCKET_ERROR;
	// Nhan reply tu ftp server.
	int reply = recvReply();
	if (reply == 120)
	{
		int wait = (lastReply[21] - 48) * 100 + (lastReply[22] - 48) * 10 + lastReply[23] - 48;
		Sleep(wait * 60 * 1000);
		reply = recvReply();
	}
	if (reply == 220) // 220 Service ready for new user.
		return true;
	return false;
}

/// <summary>
/// Ham dang nhap vao ftp server.
/// </summary>
/// <returns> True neu dang nhap thanh cong, nguoc lai false </returns>
bool FtpClient::loginToServer()
{
	string userName;
	cout << "Enter username: ";
	cin >> userName;
	sendCmd("USER " + userName);
	int reply = recvReply();
	if (reply == 331)
	{
		string passWord;
		cout << "Enter password: ";
		SetStdinEcho(false);
		cin >> passWord;
		SetStdinEcho(true);
		sendCmd("PASS " + passWord); // Gui password.		
		reply = recvReply();
	}
	if (reply == 332)
	{
		string account;
		cout << "Enter account infomation: ";
		cin >> account;
		sendCmd("ACCT " + account);
		reply = recvReply();
	}
	return reply == 230;
}

/// <summary>
/// Ham khoi tao data socket den server theo active mode
/// </summary>
bool FtpClient::activeMode()
{
	int size = sizeof(sockaddr);
	if (getsockname(sockCmd, (sockaddr*)&cmdAddr, &size) == SOCKET_ERROR) // Lay dia chi cua command socket.
		throw SOCKET_ERROR;

	sockaddr_in service; // Dia chi cho data socket.
	service.sin_family = AF_INET;
	service.sin_port = 0; // Tu dong cap free port.
	service.sin_addr = cmdAddr.sin_addr;
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Tao data socket.
	if (listenSocket == INVALID_SOCKET) // Khong tao duoc socket.
		throw INVALID_SOCKET;
	if (bind(listenSocket, (sockaddr*)&service, size) == SOCKET_ERROR)
		throw SOCKET_ERROR;
	if (listen(listenSocket, MAX_PENDING_CONNECTION) == SOCKET_ERROR) // Lang nghe ket noi tu server.
		throw SOCKET_ERROR;
	/// Neu xay ra bat ky loi gi, khong listen nua
	if (getsockname(listenSocket, (sockaddr*)&service, &size) == SOCKET_ERROR) // Lay dia chi cua data socket (lay port).
	{
		closesocket(listenSocket);
		throw SOCKET_ERROR;
	}
	int b1 = service.sin_addr.S_un.S_un_b.s_b1; // 4 byte cua ip address.
	int b2 = service.sin_addr.S_un.S_un_b.s_b2;
	int b3 = service.sin_addr.S_un.S_un_b.s_b3;
	int b4 = service.sin_addr.S_un.S_un_b.s_b4;
	int b5 = service.sin_port & 255; // 2 byte cua port.
	int b6 = service.sin_port >> 8;
	string address = to_string(b1) + "," + to_string(b2) + "," + to_string(b3) + "," + to_string(b4) + "," + to_string(b5) + "," + to_string(b6);
	try
	{
		sendCmd("PORT " + address); // Gui lenh PORT.
		int reply = recvReply();
		if (reply != 200)
		{
			closesocket(listenSocket); // Neu bi loi thi khong listen nua
			return false;
		}
		return true;
	}
	catch (int e)
	{
		closesocket(listenSocket);
		throw e;
	}
}

/// <summary>
/// Ham accept connection tu server
/// </summary>
void FtpClient::accept_connection()
{
	sockaddr_in service;
	do
	{
		int size = sizeof(sockaddr);
		sockData = accept(listenSocket, (sockaddr*)&service, &size);
		if (sockData == INVALID_SOCKET)
		{
			closesocket(listenSocket);
			throw INVALID_SOCKET;
		}
	} while (service.sin_port != htons(20)); // Accept den khi gap port 20.
	closesocket(listenSocket); // Khong can listen socket nua.
}

/// <summary>
/// Ham nhan reply tu server
/// Neu duong lenh khong con ket noi thi throw exception.
/// </summary>
/// <returns> Reply cua server </returns>
int FtpClient::recvReply()
{
	char * buffer = new char[MAX_REPLY_LENGTH + 1];
	lastReply = "";
	do
	{
		int length = recv(sockCmd, buffer, MAX_REPLY_LENGTH, 0);
		if (length == 0)
		{
			delete[] buffer;
			throw CONNECTION_CLOSED;
		}
		if (length == SOCKET_ERROR)
		{
			delete[] buffer;
			throw SOCKET_ERROR;
		}
		buffer[length] = '\0';
		cout << buffer;
		lastReply += string(buffer, buffer + length);
	} while (checkReply(lastReply) == false); // DOc den khi nhan du reply.
	delete[] buffer;
	return stoi(string(lastReply.begin(), lastReply.begin() + 3));
}

/// <summary>
/// Ham khoi dong ftp client
/// </summary>
void FtpClient::startClient()
{
	try {
		this->initialize();
		if (this->connectToServer() == false)
		{
			cout << "Ftp client connectToServer failed\n";
			return;
		}
		while (loginToServer() == false)
		{
			cout << "Exit? (Y/N)\n";
			char cmd;
			cin >> cmd;
			if (cmd == 'Y')
				return;
			cout << "Please login to server:\n";
		}
		do
		{
			cout << "FtpClient> ";
			string userInput, cmd, parameter;
			stringstream ss;
			do
			{
				getline(cin, userInput);
			} while (userInput.empty());
			ss << userInput;
			ss >> cmd;
			ss >> parameter;

			if (cmd == "dir")
				this->dir(parameter);
			else if (cmd == "ls")
				this->ls(parameter);
			else if (cmd == "get")
				this->get(parameter);
			else if (cmd == "put")
				this->put(parameter);
			else if (cmd == "passive")
				this->enterPassiveMode();
			else if (cmd == "quit" || cmd == "exit")
				break;
			else if (cmd == "lcd")
				this->lcd(parameter);
			else if (cmd == "delete")
				this->del(parameter);
			else if (cmd == "mkdir")
				this->mkdir(parameter);
			else if (cmd == "cd")
				this->cd(parameter);
			else if (cmd == "pwd")
				this->pwd();
			else if (cmd == "mget")
				this->mget(parameter);
			else if (cmd == "mdel")
				this->mdel(parameter);
			else if (cmd == "rmdir")
				this->rmdir(parameter);
			else if (cmd == "mput")
				this->mput(parameter);
			else
			{
				cout << "Unsupported command\n";
			}

		} while (true);
	}
	catch (int e)
	{
		this->cleanUp();
		throw e;
	}
	this->cleanUp();
}

/// <summary>
/// Ham liet ke cac thu muc, tap tin tren server
/// </summary>
/// <param name = "path"> Duong dan can liet ke cac thu muc </param>
void FtpClient::dir(const string & path)
{
	string res = getDir(path);
	cout << res;
}

/// <summary>
/// Ham gui lenh den server
/// </summary>
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
		{
			delete[] buffer;
			throw SOCKET_ERROR;
		}
		res += string(buffer, buffer + length);
	} while (length > 0);
	delete[] buffer;
	return res;
}

/// <summary>
/// Ham khoi tao ket noi den server theo che do passive
/// </summary>
bool FtpClient::passiveMode()
{
	sendCmd("PASV");
	int reply = recvReply();
	if (reply == 227) // 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
	{
		sockData = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sockData == INVALID_SOCKET)
			throw INVALID_SOCKET;
		// Socket da duoc tao, neu co loi gi thi close truoc khi thoat
		try {
			sockaddr_in svDataAddr = getAddrFromPasvReply(); // Dia chi cong data cua server.
			if (connect(sockData, (sockaddr*)&svDataAddr, sizeof(sockaddr)) == SOCKET_ERROR) // Ket noi den cong data.
				throw SOCKET_ERROR;
			return true;
		}
		catch (int e)
		{
			closeDataChannel();
			throw e;
		}
	}
	return false;
}

/// <summary>
/// Ham ngat ket noi duong truyen du lieu
/// </summary>
void FtpClient::closeDataChannel() const
{
	if (shutdown(sockData, SD_BOTH) == SOCKET_ERROR)
		throw SOCKET_ERROR;
	if (closesocket(sockData) == SOCKET_ERROR)
		throw SOCKET_ERROR;
}

/// <summary>
/// Ham lay dia chi tu phan hoi 227 cua lenh PASV
/// </summary>
sockaddr_in FtpClient::getAddrFromPasvReply() const
{
	if ((int)lastReply.size() < 3 || string(lastReply.begin(), lastReply.begin() + 3) != "227")
		throw PASSIVE_REPLY_NOT_FOUND;
	string tmp;
	stringstream ss;
	ss << lastReply;
	// 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
	for (int i = 0; i < 5; ++i)
		ss >> tmp;
	// tmp = (h1,h2,h3,h4,p1,p2)
	uint8_t bytes[6] = {};
	int n = 0, sum = 0;
	for (int i = 0; i < (int)tmp.size(); ++i) // Tach cac byte cua dia chi ra luu vao mang.
		if (tmp[i] >= '0' && tmp[i] <= '9') // Chu so.
			sum = sum * 10 + tmp[i] - '0';
		else if (tmp[i] == ',' || tmp[i] == ')')
		{
			bytes[n++] = sum;
			sum = 0;
		}
	sockaddr_in res;
	res.sin_family = AF_INET;
	uint32_t ip = 0;
	for (int i = 3; i >= 0; --i)
		ip = (ip << 8) + bytes[i];
	res.sin_addr.S_un.S_addr = ip;
	res.sin_port = (uint16_t(bytes[5]) << 8) + bytes[4]; // Port luu theo kieu big edian.
	return res;
}

/// <summary>
/// Khoi dong che do passive
/// </summary>
void FtpClient::enterPassiveMode()
{
	isPassive = true;
	cout << "Client changes to passive mode\n";
}

/// <summary>
/// Ham download file tu server
/// </summary>
/// <param name = "path"> Duong dan den file can download </param>
void FtpClient::get(const string & path)
{
	if (!openDataChannel()) return;
	int reply;
	try
	{
		sendCmd("RETR " + path);
		reply = recvReply();
	}
	catch (int e)
	{
		if (isPassive) closeDataChannel();
		throw e;
	}
	if (!isPassive)
		accept_connection();
	if (reply == 125 || reply == 150)  
	{
		ofstream os((localDir + '/' + getFileNameFromPath(path)).c_str(), ofstream::binary | ofstream::out);
		if (os.is_open() == false)
		{
			cout << "Local folder not found\n";
			closeDataChannel();
			return;
		}
		try {
			recvData(os);
			os.close();
			reply = recvReply();
		}
		catch (int e)
		{
			closeDataChannel();
			throw e;
		}
		closeDataChannel();
	}
}

/// <summary>
/// Ham mo duong truyen du lieu
/// </summary>
bool FtpClient::openDataChannel()
{
	if (isPassive)
		return passiveMode();
	else
		return activeMode();
}

/// <summary>
/// Ham nhan du lieu tu server va xuat ra output stream
/// </summary>
/// <param name = "os"> Output stream can xuat </param>
void FtpClient::recvData(ofstream & os) const
{
	char * buffer = new char[MAX_BUFFER_SIZE];
	int length;
	do
	{
		length = recv(sockData, buffer, MAX_BUFFER_SIZE, 0);
		if (length == 0) // Server ngat socket, truyen du lieu xong.
			break;
		if (length == SOCKET_ERROR)
		{
			delete[] buffer;
			os.close();
			throw SOCKET_ERROR;
		}
		os.write(string(buffer, buffer + length).c_str(), length);
	} while (length > 0);
	delete[] buffer;
}

/// <summary>
/// Ham lay ten file tu duong dan 
/// </summary>
/// <param name = "path"> Duong dan </param>
/// <returns> Ten file lay tu duong dan </returns>
string FtpClient::getFileNameFromPath(const string & path)
{
	string res;
	for (int i = (int)path.size() - 1; i >= 0; --i)
	{
		if (path[i] == '/' || path[i] == '\\')
			break;
		res += path[i];
	}
	reverse(res.begin(), res.end());
	return res;
}

/// <summary>
/// Ham gui file den server
/// </summary>
/// <param name = "path"> Duong dan den file can gui </param>
void FtpClient::put(const string & path)
{
	ifstream is(localDir + '/' + path.c_str(), ifstream::binary | ifstream::in);
	if (is.is_open() == false)
	{
		cout << "File does not exist\n";
		return;
	}
	if (!openDataChannel()) return;
	int reply;
	try {
		sendCmd("STOR " + getFileNameFromPath(path));
		reply = recvReply();
	}
	catch (int e)
	{
		if (isPassive) closeDataChannel();
		throw e;
	}
	if (!isPassive)
		accept_connection();
	if (reply == 125 || reply == 150) try
	{
		try { sendData(is); }
		catch (int e)
		{
			closeDataChannel();
			throw e;
		}
		closeDataChannel(); // Dong ket noi de danh dau chuyen xong file.
		reply = recvReply(); // Nhan reply 226.
	}
	catch (int e)
	{
		is.close();
		throw e;
	}
	is.close();
}

/// <summary>
/// Ham gui du lieu tu input stream den server
/// </summmary>
/// <param name = "is"> Input stream </param>
void FtpClient::sendData(ifstream & is) const
{
	char * buffer = new char[MAX_BUFFER_SIZE];
	while (is.eof() == false)
	{
		is.read(buffer, streamsize(MAX_BUFFER_SIZE));
		int bufferSize = int(is.gcount()); // So luong ki tu doc duoc.
		while (bufferSize > 0)
		{
			int length = send(sockData, buffer, bufferSize, 0);
			if (length == SOCKET_ERROR)
			{
				delete[] buffer;
				throw SOCKET_ERROR;
			}
			bufferSize -= length;
		}
	}
	delete[] buffer;
}

/// <summary>
/// Don dep tai nguyen
/// </summary>
void FtpClient::cleanUp()
{
	if (shutdown(sockCmd, SD_BOTH) == SOCKET_ERROR)
		throw SOCKET_ERROR;
	if (closesocket(sockCmd) == SOCKET_ERROR)
		throw SOCKET_ERROR;
	if (WSACleanup() == SOCKET_ERROR)
		throw SOCKET_ERROR;
}

/// <summary>
/// Ham kiem tra mot reply da duoc nhan du chua
/// </summary>
/// <param name = "reply"> Reply can kiem tra </param>
/// <returns> True neu reply da nhan du, nguoc lai false </returns>
bool FtpClient::checkReply(const string & reply)
{
	if (reply[3] != '-') // Reply mot dong.
		return true;
	// Reply nhieu dong.
	string code = string(reply.begin(), reply.begin() + 3) + ' ';
	for (int i = 0; i + 4 < (int)reply.size(); i++)
		if (string(reply.begin() + i, reply.begin() + i + 4) == code)
			return true;
	return false;
}

/// <summary>
/// Ham doi duong dan hien hanh tren client.
/// </summmary>
/// <param name = "path"> Duong dan moi sau khi doi </param>
void FtpClient::lcd(const string & path)
{
	localDir = path;
	if (CreateDirectory(localDir.c_str(), NULL) == TRUE)
		cout << "Directory changes successfuly\n";
	else if (GetLastError() == ERROR_ALREADY_EXISTS)
		cout << "Director already exists\n";
	else
		cout << "Can not change directory\n";
}

/// <summary>
/// Ham thay doi duong dan tren server
/// </summary>
/// <param name = "path"> Duong dan moi </param>
void FtpClient::cd(const string &path)
{
	sendCmd("CWD " + path);
	int reply = recvReply();
}

/// <summary>
/// Hien thi duong dan tai server
/// </summary>
/// <param name = "" </param>
void FtpClient::pwd()
{
	sendCmd("PWD");
	int reply = recvReply();
}

/// <summary>
/// Ham xoa mot file tren server
/// </summary>
/// <param name = "path" Duong dan file can xoa </param>
void FtpClient::del(const string &path)
{
	sendCmd("DELE " + path);
	int reply = recvReply();
}

/// <summary>
/// Ham tao mot thu muc tren server
/// </summary>
/// <param name = "path" Duong dan thu muc can tao tren server </param>
void FtpClient::mkdir(const string &path)
{
	string namepath = getFileNameFromPath(path);
	sendCmd("MKD " + namepath);
	int reply = recvReply();
}

/// <summary>
/// Ham liet ke cac thu muc tren server
/// </summary>
/// <param name = "path"> Duong dan can liet ke cac thu muc </param>
void FtpClient::ls(const string &path)
{
	if (!openDataChannel()) return;
	int reply;
	try {
		sendCmd("LIST " + path);
		reply = recvReply();
	}
	catch (int e)
	{
		if (isPassive) closeDataChannel();
		throw e;
	}
	if (!isPassive) accept_connection();
	if (reply == 125 || reply == 150) // Bat dau nhan du lieu.
	{
		try
		{
			string buffer = recvData();
			reply = recvReply();
			if (reply == 226 || reply == 250)
				cout << buffer;
		}
		catch (int e)
		{
			closeDataChannel();
			throw e;
		}
		closeDataChannel();
	}
	
}

/// <summary>
/// Ham tai nhieu file tu server
/// </summary>
/// <param name = "path"> Duong dan den cac thu muc can tai file </param>
void FtpClient::mget(const string & path)
{
	// Tao danh sach file
	vector <string> fnames = createVectorFile(path);

	// Download tung file
	for (int i = 0; i < (int)fnames.size(); i++)
	{
		get(fnames[i]);
	}
}


/// <summary>
/// Ham lay cac tap tin trong thu muc hien hanh.
/// </summary>
/// <param name = "path"> Duong dan den thu muc hien hanh </param>
/// <returns> Danh sach cac thu muc </returns>
string FtpClient::getDir(const string & path)
{
	string res;
	int reply;
	if (!openDataChannel()) return "";
	try {
		sendCmd("NLST " + path);
		reply = recvReply();
	}
	catch (int e)
	{
		if (isPassive) closeDataChannel();
		throw e;
	}
	if (!isPassive)
		accept_connection();
	if (reply == 125 || reply == 150) // Bat dau nhan du lieu.
	{
		try {
			string buffer = recvData();
			reply = recvReply();
			if (reply == 226 || reply == 250)
				res += buffer;
		}
		catch (int e)
		{
			closeDataChannel();
			throw e;
		}
		closeDataChannel();
	}
	return res;
}

/// <summary>
/// Ham xoa nhieu file tu server
/// </summary>
/// <param name = "path"> Duong dan den cac file can xoa </param>
void FtpClient::mdel(const string & path)
{
	// Tao danh sach file
	vector <string> fnames = createVectorFile(path);

	// Xoa tung file
	for (int i = 0; i < (int)fnames.size(); i++)
	{
		del(fnames[i]);
	}
}

/// <summary>
/// Ham upload nhieu file len server
/// </summary>
/// <param name = "path" Duong dan toi thu muc can upload file </param>
void FtpClient::mput(const string &path)
{
	// Tao danh sach file
	vector <string> fnames = createVectorFile(path);

	// Upload tung file
	for (int i = 0; i < (int)fnames.size(); i++)
	{
		put(fnames[i]);
	}
}

/// <summary>
/// Ham xoa thu muc rong tren server
/// </summary>
/// <param name = "path" Duong dan thu muc can tim xoa </param>
void FtpClient::rmdir(const string &path)
{
	string list = getDir(path);	 // list = "abc.txt\r\nblablabla.txt\r\n"
	string fileName;
	for (int i = 0; i < (int)list.size(); i++)
	{
		if (list[i] == '\n')
		{
			fileName = "";
			continue;
		}
		if (list[i] == '\r')
		{
			string pathFileName = path + fileName;
			WIN32_FIND_DATA fd;
			HANDLE hFind = ::FindFirstFile(pathFileName.c_str(), &fd);
			if (hFind == INVALID_HANDLE_VALUE)
			{
				sendCmd("RMD " + fileName);
				int reply = recvReply();
			}
		}
		else
			fileName += list[i];
	}
}

/// <summary>
/// Ham bao loi socket
/// </summary>
void FtpClient::printError(int e)
{
	if (e == INVALID_ADDRESS)
		cout << "Error: Ftp server IP address wrong format\n";
	else if (e == SOCKET_ERROR || e == INVALID_SOCKET)
	{
		int lastErr = WSAGetLastError();
		switch (lastErr)
		{
		case WSAENETDOWN:
			cout << "The network subsystem has failed\n";
			break;
		case WSAECONNREFUSED:
			cout << "The server refuses to connect\n";
			break;
		case WSAENETUNREACH:
			cout << "Cannot reach the network at this time\n";
			break;
		case WSAETIMEDOUT:
			cout << "Connect timed out\n";
			break;
		case WSAEMFILE:
			cout << "No more socket descriptors are available\n";
			break;
		case WSAECONNABORTED:
			cout << "The virtual circuit was terminated due to a time-out or other failure\n";
			break;
		case WSAEINVALIDPROVIDER:
			cout << "The service provider returned a version other than 2.2\n";
			break;
		case WSAEPROVIDERFAILEDINIT:
			cout << "The service provider failed to initialize\n";
			break;
		}
	}
	else if (e == CONNECTION_CLOSED)
		cout << "Error: No connection\n";
	else if (e == NOT_ENOUGH_ARG)
		cout << "Error: Not enough arguments\n";
	else if (e == PASSIVE_REPLY_NOT_FOUND)
		cout << "Error: Can not receive passive reply\n";
	else
		cout << "Error: Unknown error\n";
}

/// <summary>
/// Ham lay cac tap tin trong duong dan thu muc hien hanh.
/// </summary>
/// <param name = "path"> Duong dan den thu muc hien hanh </param>
/// <returns> Danh sach cac thu muc </returns>
vector<string> FtpClient::createVectorFile(const string & path) const
{
	vector<string> fnames;
	stringstream ss;
	ss << path;
	while (!ss.eof())
	{
		string fileName;
		ss >> fileName;					// Tach ten file
		fnames.push_back(fileName);		// Dua ten file vao danh sach
		if (fileName[0] == '"')			// Ten file co dang dac biet (dau cach, ...)
		{
			while (1)					// Lap den luc tim duoc dau "
			{
				ss >> fileName;
				fnames.back() += fileName;
				if (fileName.back() == '"') break;
			}
			fnames.back().erase(0);
			fnames.back().erase(fnames.back().back());
		}
	}

	return fnames;
}

void SetStdinEcho(bool enable)
{
#ifdef WIN32
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);

	if (!enable)
		mode &= ~ENABLE_ECHO_INPUT;
	else
		mode |= ENABLE_ECHO_INPUT;

	SetConsoleMode(hStdin, mode);

#else
	struct termios tty;
	tcgetattr(STDIN_FILENO, &tty);
	if (!enable)
		tty.c_lflag &= ~ECHO;
	else
		tty.c_lflag |= ECHO;

	(void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}