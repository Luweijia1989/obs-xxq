#ifndef _LIB_SHARED_MEM_H
#define _LIB_SHARED_MEM_H

#include <Windows.h>

namespace qBase
{

HANDLE SM_Create(DWORD mem_size, const char *mem_name);

bool SM_Read(HANDLE mem_handle, DWORD offset, DWORD read_size, void *out_buf);

bool SM_Write(HANDLE mem_handle, DWORD offset, DWORD write_size, void *data_buf);

HANDLE SM_Open(const char *mem_name);

void SM_Close(HANDLE mem_handle);

}    // namespace qBase

#endif
