#ifndef _LIB_SHARED_MEM_H
#define _LIB_SHARED_MEM_H

#ifdef LIBQTBASE_LIB
#define LIBQTBASE_EXPORT_HELPER __declspec(dllexport)
#else
#define LIBQTBASE_EXPORT_HELPER __declspec(dllimport)
#endif

#include <Windows.h>

namespace qBase
{

LIBQTBASE_EXPORT_HELPER HANDLE SM_Create(size_t mem_size, const char *mem_name);

LIBQTBASE_EXPORT_HELPER bool SM_Read(HANDLE mem_handle, size_t offset, size_t read_size, void *out_buf);

LIBQTBASE_EXPORT_HELPER bool SM_Write(HANDLE mem_handle, size_t offset, size_t write_size, void *data_buf);

LIBQTBASE_EXPORT_HELPER HANDLE SM_Open(const char *mem_name);

LIBQTBASE_EXPORT_HELPER void SM_Close(HANDLE mem_handle);

}    // namespace qBase

#endif