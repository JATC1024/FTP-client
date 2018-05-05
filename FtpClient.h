#pragma once

#include "resource.h"
#ifndef MAX_REPLY_LENGTH
#define MAX_REPLY_LENGTH 100
#endif
#ifndef CONNECTION_CLOSED
#define CONNECTION_CLOSED -1
#endif
#ifndef UNABLE_TO_CREATE_SOCKET
#define UNABLE_TO_CREATE_SOCKET -3
#endif
#ifndef MAX_PENDING_CONNECTION
#define MAX_PENDING_CONNECTION 5
#endif
#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE (1 << 20)
#endif
using namespace std;
class FtpClient
{
private:
	sockaddr_in svAddr; // Dia chi server.
	int sockCmd; // Socket duong lenh.
	sockaddr_in cmdAddr; // Dia chi cua socket duong lenh.
	int sockData; // Socket duong du lieu.		
	bool isPassive;
	string localDir;
private:		
	// VIET MOT HAM sendCmd DUY NHAT CHO TAT CAC CAC LENH
	// Vi du muon gui lenh USER thi sendCmd("USER balbalbal")
	void sendCmd(const string &) const;

	// Nhan reply tu server, xuat reply ra man hinh. Tra ve reply.
	int recvReply() const;		

	// Nhan du lieu tu server.
	string recvData() const;

	// Khoi tao winsock.
	void initialize() const; 

	// Ket noi den server. Tra ve true neu thanh cong, nguoc lai false.
	bool connectToServer();	

	// Dang nhap den server, username va password do nguoi dung nhap. Tra ve true neu thanh cong, nguoc lai false.
	bool loginToServer() const;

	// Khoi tao sockData theo che do active.
	void activeMode();

	// Khoi tao sockData theo che do passvice.
	void passiveMode();

	// Liet ke cac thu muc, tap tin trong thu muc hien hanh.
	void list(const string &);

	// Lenh dir.
	void dir(const string &);

	// Upload file len server. Tham so la duong dan den file.
	void put(const string &);

	// Download mot file tu server. Tham so la duong dan den file.
	void get(const string &);

	// Upload nhieu file len server. Can ban chi tiet hon.
	void mput(const string &);

	// Download nhieu file tu server. Can ban chi tiet hon.
	void mget(const string &);

	// Thay doi duong dan tren server. Tham so la dia chi duong dan moi.
	void cd(const string &);

	// Thay doi duong dan duoi client. THam so la dia chi duong dan moi.
	void lcd(const string &);

	// Xoa mot file tren server. Tham so la duong dan file.
	void del(const string &);

	// Xoa nhieu file tren server. Can ban chi tien hon
	void mdel(const string &);

	// Tao thu muc tren server. Tham so la duong dan cua thu muc sau khi duoc tao.
	void mkdir(const string &);

	// Xoa thu muc rong. Can ban chi tiet hon.
	void rmdir(const string &);

	// Chuyen sang trang thai passive.
	void enterPassiveMode();

	// Thoat khoi server
	void quit();
public:
	// Tao mot ftpclient moi. Tham so la dia chi IP cua ftp server.
	FtpClient(const string&);
	
	// Chay client.
	void startClient();
};
