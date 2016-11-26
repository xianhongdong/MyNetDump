#pragma once

extern "C"
{
	struct NetPackage
	{
		char dstip[16];
		char srcip[16];
	};

	typedef void (_stdcall *NotifyHandler)(NetPackage package);

	__declspec(dllexport) bool Init(NotifyHandler notifyFunc);
	__declspec(dllexport) bool Start();
	__declspec(dllexport) bool Stop();
}