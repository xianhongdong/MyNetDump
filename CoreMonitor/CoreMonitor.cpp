// CoreMonitor.cpp : ���� DLL Ӧ�ó���ĵ���������
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
#define SIO_RCVALL  (IOC_IN|IOC_VENDOR|1)//��������Ϊ����ģʽ

typedef struct ip_hdr//����IP�ײ�
{
	unsigned char h_verlen;//4λ�ײ����ȣ�4λIP�汾��
	unsigned char tos;//8λ��������TOS
	unsigned short tatal_len;//16λ�ܳ���
	unsigned short ident;//16λ��ʾ
	unsigned short frag_and_flags;//ƫ������3λ��־λ
	unsigned char ttl;//8λ����ʱ��TTL
	unsigned char proto;//8λЭ�飨TCP,UDP��������
	unsigned short checksum;//16λIP�ײ������
	unsigned int sourceIP;//32λԴIP��ַ
	unsigned int destIP;//32λĿ��IP��ַ
}IPHEADER;

typedef struct tsd_hdr//����TCPα�ײ�
{
	unsigned long saddr;//Դ��ַ
	unsigned long daddr;//Ŀ�ĵ�ַ
	char mbz;
	char ptcl;//Э������
	unsigned short tcpl;//TCP����
}PSDHEADER;

typedef struct tcp_hdr//����TCP�ײ�
{
	unsigned short sport;//16λԴ�˿�
	unsigned short dport;//16λĿ�Ķ˿�
	unsigned int seq;//32λ���к�
	unsigned int ack;//32λȷ�Ϻ�
	unsigned char lenres;//4λ�ײ�����/6λ������
	unsigned char flag;//6λ��־λ
	unsigned short win;//16λ���ڴ�С
	unsigned short sum;//16λ�����
	unsigned short urp;//16λ��������ƫ����
}TCPHEADER;

typedef struct udp_hdr//����UDP�ײ�
{
	unsigned short sport;//16λԴ�˿�
	unsigned short dport;//16λĿ�Ķ˿�
	unsigned short len;//UDP ����
	unsigned short cksum;//����
}UDPHEADER;

typedef struct icmp_hdr//����ICMP�ײ�
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
	char TcpFlag[6] = { 'F', 'S', 'R', 'A', 'U' };//����TCP��־λ
	WSAStartup(MAKEWORD(2, 1), &wsd);

	if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_IP)) == SOCKET_ERROR)//����һ��ԭʼ�׽���
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
	sa.sin_port = htons(1);//ԭʼ�׽���û�ж˿ڵĸ���������ֵ�������
	memcpy(&sa.sin_addr, pHostent->h_addr_list[0], pHostent->h_length);//���ñ�����ַ

	bind(sock, (SOCKADDR*)&sa, sizeof(sa));//��
	if (WSAGetLastError() == 10013)
	{
		exit(0);
	}

	//��������Ϊ����ģʽ��Ҳ�з���ģʽ�������������������еİ���
	WSAIoctl(sock, SIO_RCVALL, &optval, sizeof(optval), nullptr, 0, &dwBytesRet, nullptr, nullptr);

	UDPHEADER * pUdpheader;//UDPͷ�ṹ��ָ��
	IPHEADER * pIpheader;//IPͷ�ṹ��ָ��
	TCPHEADER * pTcpheader;//TCPͷ�ṹ��ָ��
	ICMPHEADER * pIcmpheader;//ICMPͷ�ṹ��ָ��
	char szSourceIP[MAX_ADDR_LEN] = {}, szDestIP[MAX_ADDR_LEN] = {};//ԴIP��Ŀ��IP
	SOCKADDR_IN saSource, saDest;//Դ��ַ�ṹ�壬Ŀ�ĵ�ַ�ṹ��

								 //���ø���ͷָ��
	pIpheader = (IPHEADER*)recvBuf;
	pTcpheader = (TCPHEADER*)(recvBuf + sizeof(IPHEADER));
	pUdpheader = (UDPHEADER*)(recvBuf + sizeof(IPHEADER));
	pIcmpheader = (ICMPHEADER*)(recvBuf + sizeof(IPHEADER));
	int iIphLen = sizeof(unsigned long)*(pIpheader->h_verlen & 0x0f);
	while (!m_HasStop)
	{

		memset(recvBuf, 0, sizeof(recvBuf));//��ջ�����
		recv(sock, recvBuf, sizeof(recvBuf), 0);//���հ�

		//���Դ��ַ��Ŀ�ĵ�ַ
		saSource.sin_addr.s_addr = pIpheader->sourceIP;
		strncpy(szSourceIP, inet_ntoa(saSource.sin_addr), MAX_ADDR_LEN);
		saDest.sin_addr.s_addr = pIpheader->destIP;
		strncpy(szDestIP, inet_ntoa(saDest.sin_addr), MAX_ADDR_LEN);

		//������ְ��ĳ��ȣ�ֻ���ж��Ƿ��Ǹð���������壬�ȼ��������
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
		//�ж��Ƿ���TCP��
		if (pIpheader->proto == IPPROTO_TCP&&lentcp != 0)
		{
			pCount++;//������һ
			dataip = (unsigned char *)recvBuf;
			datatcp = (unsigned char *)recvBuf + sizeof(IPHEADER) + sizeof(TCPHEADER);
			
			printf("\n#################���ݰ�[%i]=%d�ֽ�����#############", pCount,

				lentcp);
			printf("\n**********IPЭ��ͷ��***********");
			printf("\n��ʾ��%i", ntohs(pIpheader->ident));
			printf("\n�ܳ��ȣ�%i", ntohs(pIpheader->tatal_len));
			printf("\nƫ������%i", ntohs(pIpheader->frag_and_flags));
			printf("\n����ʱ�䣺%d", pIpheader->ttl);
			printf("\n�������ͣ�%d", pIpheader->tos);
			printf("\nЭ�����ͣ�%d", pIpheader->proto);
			printf("\n����ͣ�%i", ntohs(pIpheader->checksum));
			printf("\nԴIP��%s", szSourceIP);
			printf("\nĿ��IP��%s", szDestIP);
			printf("\n**********TCPЭ��ͷ��***********");
			printf("\nԴ�˿ڣ�%i", ntohs(pTcpheader->sport));
			printf("\nĿ�Ķ˿ڣ�%i", ntohs(pTcpheader->dport));
			printf("\n���кţ�%i", ntohs(pTcpheader->seq));
			printf("\nӦ��ţ�%i", ntohs(pTcpheader->ack));
			printf("\n����ͣ�%i", ntohs(pTcpheader->sum));
			printf("\n��־λ��");

			unsigned char FlagMask = 1;
			int k;

			//��ӡ��־λ
			for (k = 0; k < 6; k++)
			{
				if ((pTcpheader->flag)&FlagMask)
					printf("%c", TcpFlag[k]);
				else
					printf(" ");
				FlagMask = FlagMask << 1;
			}
			//��ӡ��ǰ100���ֽڵ�ʮ����������
			printf("\n���ݣ�\n");
			for (int i = 0; i < 100; i++)
			{
				printf("%x", datatcp[i]);
			}
		}
		//+++++++++++++++++++++++++++++
		//��������Լ�������������жϺʹ���
		//+++++++++++++++++++++++++++++
	}
}
