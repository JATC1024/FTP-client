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
int FtpClient::lastErr = 0;

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
				if (e == NOT_ENOUGH_ARG)
					cout << "Error: Not enough arguments\n";
				else
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
	{
		WSACleanup();
		throwException(INVALID_SOCKET);
	}

	try { // Sau khi da tao duoc socket, neu co loi thi close truoc khi thoat
		int t;
		if (t = connect(sockCmd, (sockaddr*)&svAddr, sizeof(sockaddr)) == SOCKET_ERROR) // Ket noi den ftp server va kiem tra xem ket noi thanh cong hay that bai.				
			throwException(SOCKET_ERROR);
		try {	// Sau khi connect, neu gap loi thi shutdown truoc khi thoat
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
		catch (int e)
		{
			shutdown(sockCmd, SD_BOTH);
			throw e;
		}
	}
	catch (int e)
	{
		closesocket(sockCmd);
		WSACleanup();
		throw e;
	}
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
		cout << '\n';
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
		throwException(SOCKET_ERROR);

	sockaddr_in service; // Dia chi cho data socket.
	service.sin_family = AF_INET;
	service.sin_port = 0; // Tu dong cap free port.
	service.sin_addr = cmdAddr.sin_addr;
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Tao data socket.
	if (listenSocket == INVALID_SOCKET) // Khong tao duoc socket.
		throwException(SOCKET_ERROR);
	try {	// Sau khi tao listen socket, neu gap loi thi close sokcet truoc khi thoat
		if (::bind(listenSocket, (sockaddr*)&service, size) == SOCKET_ERROR)
			throwException(SOCKET_ERROR);
		if (listen(listenSocket, MAX_PENDING_CONNECTION) == SOCKET_ERROR) // Lang nghe ket noi tu server.
			throwException(SOCKET_ERROR);
		acceptThread = thread(&FtpClient::accept_connection, this);
		try {	// Sau khi tao thread, neu gap loi thi detach truoc khi thoat
			if (getsockname(listenSocket, (sockaddr*)&service, &size) == SOCKET_ERROR) // Lay dia chi cua data socket (lay port).
				throwException(SOCKET_ERROR);
			int b1 = service.sin_addr.S_un.S_un_b.s_b1; // 4 byte cua ip address.
			int b2 = service.sin_addr.S_un.S_un_b.s_b2;
			int b3 = service.sin_addr.S_un.S_un_b.s_b3;
			int b4 = service.sin_addr.S_un.S_un_b.s_b4;
			int b5 = service.sin_port & 255; // 2 byte cua port.
			int b6 = service.sin_port >> 8;
			string address = to_string(b1) + "," + to_string(b2) + "," + to_string(b3) + "," + to_string(b4) + "," + to_string(b5) + "," + to_string(b6);

			sendCmd("PORT " + address); // Gui lenh PORT.
			int reply = recvReply();
			if (reply != 200)	// Neu server bao loi
			{
				acceptThread.detach();
				closesocket(listenSocket);
				return false;
			}
			return true;
		}
		catch (int e)
		{
			if (accepted)
				closeDataChannel();
			acceptThread.detach();
			throw e;
		}
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
	accepted = false;
	sockaddr_in service;
	while (true)
	{
		int size = sizeof(sockaddr);
		sockData = accept(listenSocket, (sockaddr*)&service, &size);
		if (sockData == INVALID_SOCKET)
			return;
		if (service.sin_port == htons(20)) break;	// Accept den khi gap port 20.
		try {
			closeDataChannel();		// Close data socket truoc khi accept cai khac
		}
		catch (int e) {
			return;
		}
	}
	accepted = true;	
}

/// <summary>
/// Ham nhan reply tu server
/// Neu duong lenh khong con ket noi thi throw exception.
/// </summary>
/// <returns> Reply cua server </returns>
int FtpClient::recvReply()
{
	char buffer;
	lastReply = "";
	do
	{
		int length = recv(sockCmd, &buffer, 1, 0);
		if (length == 0)					
			throw CONNECTION_CLOSED;		
		if (length == SOCKET_ERROR)		
			throwException(SOCKET_ERROR);				
		cout << buffer;
		lastReply += buffer;
	} while (checkReply(lastReply) == false); // DOc den khi nhan du reply.		
	return stoi(string(lastReply.begin(), lastReply.begin() + 3));
}

/// <summary>
/// Ham khoi dong ftp client
/// </summary>
void FtpClient::startClient()
{
	this->initialize();
	if (this->connectToServer() == false)
	{
		cout << "Ftp client connectToServer failed\n";
		return;
	}

	try {	// Sau khi tao command socket, neu co loi thi cleanUp() truoc khi thoat		
		while (loginToServer() == false)
		{
			cout << "Exit? (Y/N)\n";
			char cmd;
			cin >> cmd;
			if (cmd == 'Y')
				return;
			cout << "Please login to server:\n";
		}
	}
	catch (int e)
	{
		this->cleanUp();
		throw e;
	}
	while (true)
	{
		try {
			cout << "FtpClient> ";
			string userInput;
			do
			{
				getline(cin, userInput);
			} while (userInput.empty());
			vector<string> params = getCmdAndParameter(userInput);
			string cmd = params[0];
			string parameter = params.size() >= 2 ? params[1] : "";
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
				this->mget(params);
			else if (cmd == "mdel")
				this->mdel(params);
			else if (cmd == "rmdir")
				this->rmdir(parameter);
			else if (cmd == "mput")
				this->mput(params);
			else
			{
				cout << "Unsupported command\n";
			}
		}
		catch (int e)
		{
			printError(e);
		}
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
		throwException(SOCKET_ERROR);
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
			throwException(SOCKET_ERROR);
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
			throwException(INVALID_SOCKET);
		// Socket da duoc tao, neu co loi gi thi close truoc khi thoat
		try {
			sockaddr_in svDataAddr = getAddrFromPasvReply(); // Dia chi cong data cua server.
			if (connect(sockData, (sockaddr*)&svDataAddr, sizeof(sockaddr)) == SOCKET_ERROR) // Ket noi den cong data.
				throwException(SOCKET_ERROR);
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
		throwException(SOCKET_ERROR);
	if (closesocket(sockData) == SOCKET_ERROR)
		throwException(SOCKET_ERROR);
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
		else {
			if (accepted) 
				closeDataChannel();
			acceptThread.detach();
			closesocket(listenSocket);
		}
		throw e;
	}
	if (reply == 125 || reply == 150)
	{
		ofstream os((localDir + '/' + getFileNameFromPath(path)).c_str(), ofstream::binary | ofstream::out);
		if (os.is_open() == false)
		{
			cout << "Local folder not found\n";
			if (isPassive) closeDataChannel();
			else {
				if (accepted) closeDataChannel();
				else acceptThread.detach();
				closesocket(listenSocket);
			}
			return;
		}
		if (!isPassive)
		{
			cout << "Waiting for server to connect...\n";
			if (acceptDataChannel() == false)
			{
				try { reply = recvReply(); } // 425
				catch (int e) { os.close(); throw e; }
				os.close();
				return;
			}
		}		
		try {
			recvData(os);	// Tu dong close file neu throw
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
	else
	{
		if (isPassive)
			closeDataChannel();
		else
		{
			closesocket(listenSocket);
			acceptThread.detach();
			if (accepted)
				closeDataChannel();
		}		
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
			throwException(SOCKET_ERROR);
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
	try {
		if (!openDataChannel())
		{
			is.close();
			return;
		}
		int reply;
		try {
			sendCmd("STOR " + getFileNameFromPath(path));
			reply = recvReply();
		}
		catch (int e)
		{
			if (isPassive) closeDataChannel();
			else {
				if (accepted)
					closeDataChannel();
				acceptThread.detach();
				closesocket(listenSocket);
			}
			throw e;
		}
		if (reply == 125 || reply == 150)
		{			
			if (!isPassive) // Active mode.
			{
				cout << "Waiting for server to connect...\n";
				if (acceptDataChannel() == false)
				{
					reply = recvReply();
				}
			}
			try { sendData(is); }	// Close data socket truoc khi thoat, neu gap loi
			catch (int e)
			{
				closeDataChannel();
				throw e;
			}
			closeDataChannel(); // Dong ket noi de danh dau chuyen xong file.
			reply = recvReply(); // Nhan reply 226.			
		}
		else
		{
			if (isPassive)
				closeDataChannel();
			else
			{
				closesocket(listenSocket);
				acceptThread.detach();
				if (accepted)
					closeDataChannel();
			}
		}
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
				throwException(SOCKET_ERROR);
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
	shutdown(sockCmd, SD_BOTH);
	closesocket(sockCmd);
	WSACleanup();
}

/// <summary>
/// Ham kiem tra mot reply da duoc nhan du chua
/// </summary>
/// <param name = "reply"> Reply can kiem tra </param>
/// <returns> True neu reply da nhan du, nguoc lai false </returns>
bool FtpClient::checkReply(const string & reply)
{
	if ((int)reply.size() < 2 || reply[(int)reply.size() - 2] != '\r' || reply.back() != '\n')
		return false;
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
	if (CreateDirectory(path.c_str(), NULL) == TRUE)
	{
		cout << "Directory has changed successfuly\n";
		localDir = path;
	}
	else if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		cout << "Director already exists, local directory has changed successfully\n";
		localDir = path;
	}
	else
		cout << "Cannot change directory\n";
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
		else {
			if (accepted) closeDataChannel();
			acceptThread.detach();
			closesocket(listenSocket);
		}
		throw e;
	}

	if (reply == 125 || reply == 150)
	{
		if (!isPassive)
		{
			cout << "Waiting for server to connect...\n";
			if (acceptDataChannel() == false) // Kiem tra accept co bi time out.
			{
				reply = recvReply(); // 425 couldn't open data channel.
				return;
			}
		}		
		try {
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
	else
	{
		if (isPassive)
			closeDataChannel();
		else
		{
			closesocket(listenSocket);
			acceptThread.detach();
			if (accepted)
				closeDataChannel();
		}
	}
}

/// <summary>
/// Ham tai nhieu file tu server
/// </summary>
/// <param name = "fnames"> Duong dan den cac file can tai </param>
void FtpClient::mget(const vector<string> & fnames)
{
	// Download tung file
	for (int i = 1; i < (int)fnames.size(); i++) // Bo qua phan lenh.
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
		else {
			if (accepted) 
				closeDataChannel();
			acceptThread.detach();
			closesocket(listenSocket);
		}
		throw e;
	}
	if (reply == 125 || reply == 150) // Bat dau nhan du lieu.
	{
		if (!isPassive) // Active mode.
		{
			cout << "Waiting for server to connect...\n";
			if (acceptDataChannel() == false) // Kiem tra accept co bi time out hay ko.
			{
				reply = recvReply(); // 425
				return "";
			}
		}		
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
	else
	{
		if (isPassive)
			closeDataChannel();
		else
		{
			closesocket(listenSocket);
			acceptThread.detach();
			if (accepted)
				closeDataChannel();
		}
	}
	return res;
}

/// <summary>
/// Ham xoa nhieu file tu server
/// </summary>
/// <param name = "fnames"> Duong dan den cac file can xoa </param>
void FtpClient::mdel(const vector<string> & fnames)
{
	// Xoa tung file
	for (int i = 1; i < (int)fnames.size(); i++) // Bo qua phan lenh.
	{
		del(fnames[i]);
	}
}

/// <summary>
/// Ham upload nhieu file len server
/// </summary>
/// <param name = "fnames" Duong dan toi cac file can upload </param>
void FtpClient::mput(const vector<string> & fnames)
{
	// Upload tung file
	for (int i = 1; i < (int)fnames.size(); i++) // Bo qua phan lenh.
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
		switch (FtpClient::lastErr)
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
		case WSAEACCES:
			cout << "Permission denied\n";
			break;
		case WSAENOTCONN:
			cout << "Socket is not connected\n";
			break;
		default:
			cout << "Socket error: unknown\n";
			break;
		}
	}
	else if (e == CONNECTION_CLOSED)
		cout << "Error: No connection\n";
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
/*vector<string> FtpClient::createVectorFile(const string & path) const
{
vector<string> fnames;
stringstream ss;
ss << path;
string fileName;
ss >> fileName; // Bo phan lenh di.
while (!ss.eof())
{
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
string tmp = fnames.back();
fnames.pop_back();
fnames.push_back(string(tmp.begin() + 1, tmp.end() - 1));
}
}

return fnames;
}*/

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

/// <summary>
/// Ham lay lenh va cac tham so tu input cua nguoi dung
/// </summary>
/// <param name = "userInput"> Input cua nguoi dung </param>
/// <returns> Mot vector luu lenh va cac tham so </returns>
vector<string> FtpClient::getCmdAndParameter(const string & userInput) const
{
	vector<string> res;
	for (int i = 0; i < (int)userInput.size(); i++)
	{
		if (userInput[i] != ' ' && (i == 0 || userInput[i - 1] == ' ')) // Bat dau mot cum moi.
		{
			if (userInput[i] != '"')
			{
				int j = i;
				while (j < (int)userInput.size() && userInput[j] != ' ') j++; // Tim dau khoang cach gan nhat.				
				res.push_back(string(userInput.begin() + i, userInput.begin() + j)); // Cho vao vector.
				i = j;
			}
			else
			{
				int j = i + 1;
				bool flag = false;
				while (j < (int)userInput.size() && (flag == false || userInput[j] != ' ')) // Chay den khi gap khoang cach va da di qua mot dau '"'.
				{
					if (userInput[j] == '"')
						flag = true;
					j++;
				}
				string tmp;
				for (int k = i; k < j; k++)
					if (userInput[k] != '"')
						tmp += userInput[k];
				res.push_back(tmp); // Cho vao vector.
				i = j;
			}
		}
	}
	return res;
}

/// <summary>
/// Ham kiem tra thread accept co bi time out hay khong
/// </summary>
/// <returns> True neu accept thanh cong </returns>
bool FtpClient::acceptDataChannel()
{
	int beginTime = clock();
	bool timeOut = false;
	while (!accepted) // Thread accept dang chay.
	{
		int time = (clock() - beginTime) / CLOCKS_PER_SEC;
		if (time >= TIME_OUT)
		{
			timeOut = true; // Time out.
			break;
		}
	}
	accepted = false;
	acceptThread.detach();
	closesocket(listenSocket);	// Khong can listen sokcet nua
	return !timeOut;
}

void FtpClient::throwException(int e)
{
	if (e == SOCKET_ERROR || e == INVALID_SOCKET)
		FtpClient::lastErr = WSAGetLastError();
	throw e;
}
