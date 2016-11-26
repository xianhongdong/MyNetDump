#pragma once
#include  "NetDumpInterface.h"
class CoreMonitor
{
private:
	CoreMonitor();
	CoreMonitor(CoreMonitor&  obj) {}
public:
	static CoreMonitor& Instance();	
	bool Start();
	bool Stop();
	bool RegisteNotifyHandler(NotifyHandler handler);
	void ListenHandler();
private:
	NotifyHandler	m_Handler;
	volatile bool	m_HasStop;
	HANDLE			m_ThreadId;
};
