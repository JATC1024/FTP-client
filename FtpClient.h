#pragma once

#include "resource.h"
#ifndef _MAX_REPLY_LENGTH
#define _MAX_REPLY_LENGTH 100
#endif
#ifndef _NOT_CONNECTED
#define _NOT_CONNECTED -1
#endif
#ifndef _INVALID_IP_ADDRESS
#define _INVALID_IP_ADDRESS -2
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
	string dir;
private:
	// Voi moi lenh can thiet de gui den server, viet mot ham voi cu phap bool send<lenh>(const string &) const
	// Trong do:
	// lenh la lenh can gui toi server.
	// Tham so la tham so cua lenh do.
	// Gia tri tra ve: true neu gui den server thanh cong, nguoc lai false.
	// Vi du: bool sendUser(const string &) const dung de gui username den server.
	// Reply tu server duoc xu ly ngay sau khi goi ham send<lenh>.	

	bool sendUser(const string &) const;
	bool sendPass(const string &) const;


	// Nhan reply tu server, xuat reply ra man hinh. Tra ve reply.
	int recvReply() const;	

	// Khoi tao winsock. Tra ve true neu thanh cong, nguoc lai false.
	bool initialize() const; 

	// Ket noi den server. Tra ve true neu thanh cong, nguoc lai false.
	bool connectToServer();	

	// Dang nhap den server, username va password do nguoi dung nhap. Tra ve true neu thanh cong, nguoc lai false.
	bool loginToServer() const;

	// Khoi tao sockData theo che do active.
	void activeMode();

	// Khoi tao sockData theo che do passvice.
	void passiveMode();

	// Liet ke cac thu muc, tap tin trong thu muc hien hanh.
	void list();

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
