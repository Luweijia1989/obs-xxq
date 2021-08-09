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


static void installBonjourService()
{
	BOOL is64 = Is64BitWindows();
	TCHAR path[MAX_PATH] = {0};
	DWORD len = GetTempPath(MAX_PATH, path);
	_tcscpy_s(path + len, MAX_PATH - len,
		  is64 ? L"Bonjour64.msi" : L"Bonjour.msi");

	if (!PathFileExists(path)) {
		HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ,
					  NULL, CREATE_ALWAYS,
					  FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE) {
			HRSRC res = FindResource(
				NULL,
				MAKEINTRESOURCE(is64 ? IDR_SETUPFILE2
						     : IDR_SETUPFILE1),
				RT_RCDATA);
			HGLOBAL res_handle = LoadResource(NULL, res);
			auto res_data = LockResource(res_handle);
			auto res_size = SizeofResource(NULL, res);

			DWORD written = 0;
			WriteFile(hFile, res_data, res_size, &written, NULL);
			CloseHandle(hFile);
		}
	}

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	TCHAR cmd[512] = {0};

	_stprintf_s(cmd, L"msiexec /i %s /qn", path);

	// Start the child process.
	if (!CreateProcess(NULL,  // No module name (use command line)
			   cmd,   // Command line
			   NULL,  // Process handle not inheritable
			   NULL,  // Thread handle not inheritable
			   FALSE, // Set handle inheritance to FALSE
			   0,     // No creation flags
			   NULL,  // Use parent's environment block
			   NULL,  // Use parent's starting directory
			   &si,   // Pointer to STARTUPINFO structure
			   &pi)   // Pointer to PROCESS_INFORMATION structure
	) {
		printf("CreateProcess failed (%d).\n", GetLastError());
		return;
	}

	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles.
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}


static void bonjourCheckInstall()
{
	DWORD dwOldCheckPoint;
	DWORD dwStartTickCount;
	DWORD dwWaitTime;
	SC_HANDLE schSCManager = OpenSCManager(
						NULL,                    // local computer
						NULL,                    // servicesActive database 
						SC_MANAGER_ALL_ACCESS);  // full access rights 
 
	if (NULL == schSCManager) 
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	SC_HANDLE schService = OpenService(schSCManager,        // SCM database
				 L"Bonjour Service",           // name of service
				 SERVICE_ALL_ACCESS); // full access

	if (schService == NULL) {
		installBonjourService();

		schService = OpenService(schSCManager,        // SCM database
					 L"Bonjour Service",  // name of service
					 SERVICE_ALL_ACCESS); // full access
		if (schService == NULL)
			goto END;
	}

	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwBytesNeeded;
	if (!QueryServiceStatusEx(
		    schService,                     // handle to service
		    SC_STATUS_PROCESS_INFO,         // information level
		    (LPBYTE)&ssStatus,              // address of structure
		    sizeof(SERVICE_STATUS_PROCESS), // size of structure
		    &dwBytesNeeded)) // size needed if buffer is too small
		goto END;

	if (ssStatus.dwCurrentState == SERVICE_STOPPED) {
		if (!StartService(
			schService,  // handle to service 
			0,           // number of arguments 
			NULL) )      // no arguments 
			goto END;

			// Check the status until the service is no longer start pending. 
 
		if (!QueryServiceStatusEx( 
			schService,                     // handle to service 
			SC_STATUS_PROCESS_INFO,         // info level
			(LPBYTE) &ssStatus,             // address of structure
			sizeof(SERVICE_STATUS_PROCESS), // size of structure
			&dwBytesNeeded ) )              // if buffer too small
			goto END;
 
		// Save the tick count and initial checkpoint.

		dwStartTickCount = GetTickCount();
		dwOldCheckPoint = ssStatus.dwCheckPoint;

		while (ssStatus.dwCurrentState == SERVICE_START_PENDING) 
		{ 
			// Do not wait longer than the wait hint. A good interval is 
			// one-tenth the wait hint, but no less than 1 second and no 
			// more than 10 seconds. 
 
			dwWaitTime = ssStatus.dwWaitHint / 10;

			if( dwWaitTime < 1000 )
				dwWaitTime = 1000;
			else if ( dwWaitTime > 10000 )
				dwWaitTime = 10000;

			Sleep( dwWaitTime );

			// Check the status again. 
 
			if (!QueryServiceStatusEx( 
						schService,             // handle to service 
						SC_STATUS_PROCESS_INFO, // info level
						(LPBYTE) &ssStatus,             // address of structure
						sizeof(SERVICE_STATUS_PROCESS), // size of structure
						&dwBytesNeeded ) )              // if buffer too small
			{
				printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
				break; 
			}
 
			if ( ssStatus.dwCheckPoint > dwOldCheckPoint )
			{
				// Continue to wait and check.

				dwStartTickCount = GetTickCount();
				dwOldCheckPoint = ssStatus.dwCheckPoint;
			}
			else
			{
				if(GetTickCount()-dwStartTickCount > ssStatus.dwWaitHint)
				{
					// No progress made within the wait hint.
					break;
				}
			}
		} 
	}

END:
	if (schService)
		CloseServiceHandle(schService);
	if (schSCManager)
		CloseServiceHandle(schSCManager);
}

int main(int argv, char *argc[])
{
	SetErrorMode(SEM_FAILCRITICALERRORS);
	_setmode(_fileno(stdin), O_BINARY);
	freopen("NUL", "w", stderr);

	bonjourCheckInstall();
	 
	CAirServer server;
	server.outputStatus(MIRROR_STOP);
	if (!server.start()) {
		server.outputStatus(MIRROR_ERROR);
		return -1;
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
