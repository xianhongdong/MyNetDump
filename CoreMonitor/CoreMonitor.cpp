// CoreMonitor.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "CoreMonitor.h"
#include <stdio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <Windows.h>
#include <string.h>

#pragma comment(lib,"Ws2_32.lib")

#define SOURCE_PORT 7234
#define MAX_RECEIVEBYTE 255
#define MAX_ADDR_LEN 32
#define SIO_RCVALL  (IOC_IN|IOC_VENDOR|1)//定义网卡为混杂模式

typedef struct ip_hdr//定义IP首部
{
	unsigned char h_verlen;//4位首部长度，4位IP版本号
	unsigned char tos;//8位服务类型TOS
	unsigned short tatal_len;//16位总长度
	unsigned short ident;//16位标示
	unsigned short frag_and_flags;//偏移量和3位标志位
	unsigned char ttl;//8位生存时间TTL
	unsigned char proto;//8位协议（TCP,UDP或其他）
	unsigned short checksum;//16位IP首部检验和
	unsigned int sourceIP;//32位源IP地址
	unsigned int destIP;//32位目的IP地址
}IPHEADER;

typedef struct tsd_hdr//定义TCP伪首部
{
	unsigned long saddr;//源地址
	unsigned long daddr;//目的地址
	char mbz;
	char ptcl;//协议类型
	unsigned short tcpl;//TCP长度
}PSDHEADER;

typedef struct tcp_hdr//定义TCP首部
{
	unsigned short sport;//16位源端口
	unsigned short dport;//16位目的端口
	unsigned int seq;//32位序列号
	unsigned int ack;//32位确认号
	unsigned char lenres;//4位首部长度/6位保留字
	unsigned char flag;//6位标志位
	unsigned short win;//16位窗口大小
	unsigned short sum;//16位检验和
	unsigned short urp;//16位紧急数据偏移量
}TCPHEADER;

typedef struct udp_hdr//定义UDP首部
{
	unsigned short sport;//16位源端口
	unsigned short dport;//16位目的端口
	unsigned short len;//UDP 长度
	unsigned short cksum;//检查和
}UDPHEADER;

typedef struct icmp_hdr//定义ICMP首部
{
	unsigned short sport;
	unsigned short dport;
	unsigned char type;
	unsigned char code;
	unsigned short cksum;
	unsigned short id;
	unsigned short seq;
	unsigned long timestamp;
}ICMPHEADER;

CoreMonitor::CoreMonitor():m_Handler(NULL),m_HasStop(true)
{
}
CoreMonitor & CoreMonitor::Instance()
{
	static CoreMonitor __instance;
	return __instance;
}

extern "C" static void* ThreadHandler(void* arg)
{
	CoreMonitor::Instance().ListenHandler();
	return NULL;
}

bool CoreMonitor::Start()
{
	if (!m_HasStop)
		return false;
	DWORD tid;
	m_ThreadId =  CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ThreadHandler, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, &tid);
	if (m_ThreadId == 0)
		return false;
	m_HasStop = false;
	return true;
}

bool CoreMonitor::Stop()
{
	if (m_HasStop)
		return true;
	m_HasStop = true;
	return true;
}

bool CoreMonitor::RegisteNotifyHandler(NotifyHandler handler)
{
	m_Handler = handler;
	return true;
}

void CoreMonitor::ListenHandler()
{
	SOCKET sock;
	WSADATA wsd;
	char recvBuf[65535] = { 0 };
	char temp[65535] = { 0 };
	DWORD dwBytesRet;

	int pCount = 0;
	unsigned int optval = 1;
	unsigned char* dataip = nullptr;
	unsigned char* datatcp = nullptr;
	unsigned char* dataudp = nullptr;
	unsigned char* dataicmp = nullptr;

	int lentcp, lenudp, lenicmp, lenip;
	char TcpFlag[6] = { 'F', 'S', 'R', 'A', 'U' };//定义TCP标志位
	WSAStartup(MAKEWORD(2, 1), &wsd);

	if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_IP)) == SOCKET_ERROR)//创建一个原始套接字
	{
		exit(0);
	}

	char FAR name[MAXBYTE];
	gethostname(name, MAXBYTE);
	struct hostent FAR* pHostent;

	pHostent = (struct hostent*)malloc(sizeof(struct hostent));
	pHostent = gethostbyname(name);
	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(1);//原始套接字没有端口的概念，所以这个值随便设置
	memcpy(&sa.sin_addr, pHostent->h_addr_list[0], pHostent->h_length);//设置本机地址

	bind(sock, (SOCKADDR*)&sa, sizeof(sa));//绑定
	if (WSAGetLastError() == 10013)
	{
		exit(0);
	}

	//设置网卡为混杂模式，也叫泛听模式。可以侦听经过的所有的包。
	WSAIoctl(sock, SIO_RCVALL, &optval, sizeof(optval), nullptr, 0, &dwBytesRet, nullptr, nullptr);

	UDPHEADER * pUdpheader;//UDP头结构体指针
	IPHEADER * pIpheader;//IP头结构体指针
	TCPHEADER * pTcpheader;//TCP头结构体指针
	ICMPHEADER * pIcmpheader;//ICMP头结构体指针
	char szSourceIP[MAX_ADDR_LEN] = {}, szDestIP[MAX_ADDR_LEN] = {};//源IP和目的IP
	SOCKADDR_IN saSource, saDest;//源地址结构体，目的地址结构体

								 //设置各种头指针
	pIpheader = (IPHEADER*)recvBuf;
	pTcpheader = (TCPHEADER*)(recvBuf + sizeof(IPHEADER));
	pUdpheader = (UDPHEADER*)(recvBuf + sizeof(IPHEADER));
	pIcmpheader = (ICMPHEADER*)(recvBuf + sizeof(IPHEADER));
	int iIphLen = sizeof(unsigned long)*(pIpheader->h_verlen & 0x0f);
	while (!m_HasStop)
	{

		memset(recvBuf, 0, sizeof(recvBuf));//清空缓冲区
		recv(sock, recvBuf, sizeof(recvBuf), 0);//接收包

		//获得源地址和目的地址
		saSource.sin_addr.s_addr = pIpheader->sourceIP;
		strncpy(szSourceIP, inet_ntoa(saSource.sin_addr), MAX_ADDR_LEN);
		saDest.sin_addr.s_addr = pIpheader->destIP;
		strncpy(szDestIP, inet_ntoa(saDest.sin_addr), MAX_ADDR_LEN);

		//计算各种包的长度（只有判断是否是该包后才有意义，先计算出来）
		lenip = ntohs(pIpheader->tatal_len);
		lentcp = ntohs(pIpheader->tatal_len) - (sizeof(IPHEADER) + sizeof(TCPHEADER));
		lenudp = ntohs(pIpheader->tatal_len) - (sizeof(IPHEADER) + sizeof(UDPHEADER));
		lenicmp = ntohs(pIpheader->tatal_len) - (sizeof(IPHEADER) + sizeof(ICMPHEADER));

		NetPackage package;
		memset(&package, 0, sizeof(NetPackage));
		strncpy(package.srcip, szSourceIP, strlen(szSourceIP));
		strncpy(package.dstip, szDestIP, strlen(szDestIP));
		/*if (strcmp(szSourceIP, "192.168.31.108") != 0
			|| strcmp(szDestIP, "192.168.31.108") != 0)
			continue;*/
		if (m_Handler)
		{
			m_Handler(package);
		}
		//判断是否是TCP包
		if (pIpheader->proto == IPPROTO_TCP&&lentcp != 0)
		{
			pCount++;//计数加一
			dataip = (unsigned char *)recvBuf;
			datatcp = (unsigned char *)recvBuf + sizeof(IPHEADER) + sizeof(TCPHEADER);
			
			printf("\n#################数据包[%i]=%d字节数据#############", pCount,

				lentcp);
			printf("\n**********IP协议头部***********");
			printf("\n标示：%i", ntohs(pIpheader->ident));
			printf("\n总长度：%i", ntohs(pIpheader->tatal_len));
			printf("\n偏移量：%i", ntohs(pIpheader->frag_and_flags));
			printf("\n生存时间：%d", pIpheader->ttl);
			printf("\n服务类型：%d", pIpheader->tos);
			printf("\n协议类型：%d", pIpheader->proto);
			printf("\n检验和：%i", ntohs(pIpheader->checksum));
			printf("\n源IP：%s", szSourceIP);
			printf("\n目的IP：%s", szDestIP);
			printf("\n**********TCP协议头部***********");
			printf("\n源端口：%i", ntohs(pTcpheader->sport));
			printf("\n目的端口：%i", ntohs(pTcpheader->dport));
			printf("\n序列号：%i", ntohs(pTcpheader->seq));
			printf("\n应答号：%i", ntohs(pTcpheader->ack));
			printf("\n检验和：%i", ntohs(pTcpheader->sum));
			printf("\n标志位：");

			unsigned char FlagMask = 1;
			int k;

			//打印标志位
			for (k = 0; k < 6; k++)
			{
				if ((pTcpheader->flag)&FlagMask)
					printf("%c", TcpFlag[k]);
				else
					printf(" ");
				FlagMask = FlagMask << 1;
			}
			//打印出前100个字节的十六进制数据
			printf("\n数据：\n");
			for (int i = 0; i < 100; i++)
			{
				printf("%x", datatcp[i]);
			}
		}
		//+++++++++++++++++++++++++++++
		//在这里可以加入其它封包的判断和处理
		//+++++++++++++++++++++++++++++
	}
}
