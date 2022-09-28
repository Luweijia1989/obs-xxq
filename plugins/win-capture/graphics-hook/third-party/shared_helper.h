#pragma once

#include <Windows.h>

namespace qBase {

extern HANDLE g_danmuShare;

extern HANDLE g_danmuEvent;

 bool connect(DWORD size, const char* mem_name);

 void disConnect();

 void readShare(DWORD size, char* buff);

 void writeShare(char* buff, int length);

}

