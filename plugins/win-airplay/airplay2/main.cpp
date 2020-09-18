#include "CAirServer.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>

void installBonjour()
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	TCHAR cmd[] = L"msiexec /i C:\\Users\\luweijia.YUPAOPAO\\Desktop\\Bonjour64.msi /qn";

	// Start the child process.
	if (!CreateProcess(
		    NULL, // No module name (use command line)
		    cmd, // Command line
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

int main(int argv, char *argc[])
{
	installBonjour();

	SetErrorMode(SEM_FAILCRITICALERRORS);
	_setmode(_fileno(stdin), O_BINARY);
	freopen("NUL", "w", stderr);

	CAirServer server;
	if (!server.start())
		return -1;

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
