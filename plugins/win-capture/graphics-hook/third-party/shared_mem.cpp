#include "shared_mem.h"
#include <strsafe.h>

namespace qBase
{

HANDLE SM_Create(DWORD mem_size, const char *mem_name)
{
    SECURITY_ATTRIBUTES lsa;
    SECURITY_DESCRIPTOR lsd;
    BOOL                ret  = InitializeSecurityDescriptor(&lsd, SECURITY_DESCRIPTOR_REVISION);
    ret                      = SetSecurityDescriptorDacl(&lsd, TRUE, NULL, FALSE);
    lsa.nLength              = sizeof(lsa);
    lsa.lpSecurityDescriptor = &lsd;
    lsa.bInheritHandle       = true;

    HANDLE handle = CreateFileMappingA(INVALID_HANDLE_VALUE, &lsa, PAGE_READWRITE, 0, mem_size, mem_name);
    DWORD  error  = GetLastError();
    return handle;
}

bool SM_Read(HANDLE mem_handle, DWORD offset, DWORD read_size, void *out_buf)
{
    if(mem_handle == NULL)
    {
        return false;
    }

    if(!out_buf)
        return false;
    void *buf = MapViewOfFile(mem_handle, FILE_MAP_READ, 0, offset, read_size);
    if(!buf)
        return false;
    //StringCchCopyA((STRSAFE_LPSTR)out_buf, read_size, (STRSAFE_LPSTR)buf);
    CopyMemory(out_buf, buf, read_size);
    UnmapViewOfFile(buf);
    return true;
}

bool SM_Write(HANDLE mem_handle, DWORD offset, DWORD write_size, void *data_buf)
{
    if(mem_handle == NULL)
    {
        return false;
    }

    if(!data_buf)
        return false;
    void *buf = MapViewOfFile(mem_handle, FILE_MAP_ALL_ACCESS, 0, offset, write_size);
    if(!buf)
        return false;
    //StringCchCopyA((STRSAFE_LPSTR)buf, write_size, (STRSAFE_LPSTR)data_buf);
    CopyMemory(buf, data_buf, write_size);
    UnmapViewOfFile(buf);
    return true;
}

HANDLE SM_Open(const char *mem_name)
{
    return OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, mem_name);
}

void SM_Close(HANDLE mem_handle)
{
    CloseHandle(mem_handle);
}

}    // namespace qBase
