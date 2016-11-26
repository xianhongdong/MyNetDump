#include "stdafx.h"
#include "NetDumpInterface.h"
#include "CoreMonitor.h"

bool Init(NotifyHandler notifyFunc)
{
	return CoreMonitor::Instance().RegisteNotifyHandler(notifyFunc);
}

bool Start()
{
	return CoreMonitor::Instance().Start();
}

bool Stop()
{
	return CoreMonitor::Instance().Stop();
}
