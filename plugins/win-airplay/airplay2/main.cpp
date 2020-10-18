#include "CAirServer.h"
#include <tchar.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <Shlwapi.h>
#include "resource.h"

static BOOL Is64BitWindows()
{
#if defined(_WIN64)
	return TRUE; // 64-bit programs run only on Win64
#elif defined(_WIN32)
	// 32-bit programs run on both 32-bit and 64-bit Windows
	// so must sniff
	BOOL f64 = FALSE;
	return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
	return FALSE; // Win64 does not support Win16
#endif
}

static void bonjourCheckInstall()
{
	BOOL is64 = Is64BitWindows();
	TCHAR path[MAX_PATH] ={0};
	DWORD len = GetTempPath(MAX_PATH, path);
	_tcscpy_s(path+len, MAX_PATH-len, is64 ? L"Bonjour64.msi" : L"Bonjour.msi");

	if (!PathFileExists(path))
	{
		HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			HRSRC res = FindResource(NULL, MAKEINTRESOURCE(is64 ? IDR_SETUPFILE2 : IDR_SETUPFILE1), RT_RCDATA);
			HGLOBAL res_handle = LoadResource(NULL, res);
			auto res_data = LockResource(res_handle);
			auto res_size = SizeofResource(NULL, res);

			WriteFile(hFile, res_data, res_size, NULL, NULL);
			CloseHandle(hFile);

			STARTUPINFO si;
			PROCESS_INFORMATION pi;

			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));

			TCHAR cmd[512] ={0};

			_stprintf_s(cmd, L"msiexec /i %s /qn", path);

			// Start the child process.
			if (!CreateProcess(
				    NULL,  // No module name (use command line)
				    cmd,   // Command line
				    NULL,  // Process handle not inheritable
				    NULL,  // Thread handle not inheritable
				    FALSE, // Set handle inheritance to FALSE
				    0,     // No creation flags
				    NULL,  // Use parent's environment block
				    NULL,  // Use parent's starting directory
				    &si,   // Pointer to STARTUPINFO structure
				    &pi) // Pointer to PROCESS_INFORMATION structure
			) {
				printf("CreateProcess failed (%d).\n",
				       GetLastError());
				return;
			}

			// Wait until child process exits.
			WaitForSingleObject(pi.hProcess, INFINITE);

			// Close process and thread handles.
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
}

int main(int argv, char *argc[])
{
	SetErrorMode(SEM_FAILCRITICALERRORS);
	_setmode(_fileno(stdin), O_BINARY);
	freopen("NUL", "w", stderr);
	 
	CAirServer server;
	server.outputStatus(MIRROR_STOP);
	if (!server.start()) {
		bonjourCheckInstall();
		if (!server.start())
		{
			server.outputStatus(MIRROR_ERROR);
			return -1;
		}
		
	}

	uint8_t buf[1024] = {0};
	while (true) {
		int read_len =
			fread(buf, 1, 1024,
			      stdin); // read 0 means parent has been stopped
		if (read_len) {
			if (buf[0] == 1) {
				server.stop();
				break;
			}
		} else {
			server.stop();
			break;
		}
	}

	return 0;
}
