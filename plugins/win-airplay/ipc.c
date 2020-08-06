#include "ipc.h"

static DWORD CALLBACK read_thread(LPVOID param)
{
	struct IPCServer *server = param;
	if (!server->cb)
		return 0;

	while (!server->m_exit) {
		struct Block *block = ipc_server_get_block(server, INFINITE);
		if (block && block->Amount > 0) {
			server->cb(server->m_cbParam, block->Data,
				   block->Amount);
		}
		if (block)
			ipc_server_ret_block(server, block);
	}
	return 0;
}

void ipc_server_create(struct IPCServer **input, read_cb cb, void *param)
{
	*input = (struct IPCServer *)calloc(1, sizeof(struct IPCServer));
	struct IPCServer *server = *input;

	server->m_sAddr = IPC_MEMORY_NAME;
	server->cb = cb;
	server->m_cbParam = param;

	char *m_sEvtAvail = (char *)malloc(IPC_MAX_ADDR);
	if (!m_sEvtAvail) {
		return;
	}
	sprintf_s(m_sEvtAvail, IPC_MAX_ADDR, "%s_evt_avail", server->m_sAddr);

	char *m_sEvtFilled = (char *)malloc(IPC_MAX_ADDR);
	if (!m_sEvtFilled) {
		free(m_sEvtAvail);
		return;
	}
	sprintf_s(m_sEvtFilled, IPC_MAX_ADDR, "%s_evt_filled", server->m_sAddr);

	char *m_sMemName = (char *)malloc(IPC_MAX_ADDR);
	if (!m_sMemName) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		return;
	}
	sprintf_s(m_sMemName, IPC_MAX_ADDR, "%s_mem", server->m_sAddr);

	// Create the events
	server->m_hSignal = CreateEventA(NULL, FALSE, FALSE, m_sEvtFilled);
	if (server->m_hSignal == NULL ||
	    server->m_hSignal == INVALID_HANDLE_VALUE) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}
	server->m_hAvail = CreateEventA(NULL, FALSE, FALSE, m_sEvtAvail);
	if (server->m_hAvail == NULL ||
	    server->m_hSignal == INVALID_HANDLE_VALUE) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}

	// Create the file mapping
	server->m_hMapFile =
		CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
				   0, sizeof(struct MemBuff), m_sMemName);
	if (server->m_hMapFile == NULL ||
	    server->m_hMapFile == INVALID_HANDLE_VALUE) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}

	// Map to the file
	server->m_pBuf = (struct MemBuff *)MapViewOfFile(
		server->m_hMapFile,  // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0, 0, sizeof(struct MemBuff));
	if (server->m_pBuf == NULL) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}

	// Clear the buffer
	ZeroMemory(server->m_pBuf, sizeof(struct MemBuff));

	// Create a circular linked list
	int N = 1;
	server->m_pBuf->m_Blocks[0].Next = 1;
	server->m_pBuf->m_Blocks[0].Prev = (IPC_BLOCK_COUNT - 1);
	for (; N < IPC_BLOCK_COUNT - 1; N++) {
		// Add this block into the available list
		server->m_pBuf->m_Blocks[N].Next = (N + 1);
		server->m_pBuf->m_Blocks[N].Prev = (N - 1);
	}
	server->m_pBuf->m_Blocks[N].Next = 0;
	server->m_pBuf->m_Blocks[N].Prev = (IPC_BLOCK_COUNT - 2);

	// Initialize the pointers
	server->m_pBuf->m_ReadEnd = 0;
	server->m_pBuf->m_ReadStart = 0;
	server->m_pBuf->m_WriteEnd = 1;
	server->m_pBuf->m_WriteStart = 1;

	// Release memory
	free(m_sEvtAvail);
	free(m_sEvtFilled);
	free(m_sMemName);

	// Create read thread
	server->m_readThread =
		CreateThread(NULL, 0, read_thread, server, 0, NULL);
}

void ipc_server_destroy(struct IPCServer **input)
{
	struct IPCServer *server = *input;
	server->m_exit = true;
	SetEvent(server->m_hSignal);
	WaitForSingleObject(server->m_readThread, INFINITE);

	// Close the event
	if (server->m_hSignal) {
		HANDLE handle = server->m_hSignal;
		server->m_hSignal = NULL;
		CloseHandle(handle);
	}

	// Close the event
	if (server->m_hAvail) {
		HANDLE handle = server->m_hAvail;
		server->m_hAvail = NULL;
		CloseHandle(handle);
	}

	// Unmap the memory
	if (server->m_pBuf) {
		struct MemBuff *pBuff = server->m_pBuf;
		server->m_pBuf = NULL;
		UnmapViewOfFile(pBuff);
	}

	// Close the file handle
	if (server->m_hMapFile) {
		HANDLE handle = server->m_hMapFile;
		server->m_hMapFile = NULL;
		CloseHandle(handle);
	}

	*input = NULL;
}

DWORD ipc_server_read(struct IPCServer *server, void *pBuff, DWORD buffSize,
		      DWORD timeout)
{
	// Grab a block
	struct Block *pBlock = ipc_server_get_block(server, timeout);
	if (!pBlock)
		return 0;

	// Copy the data
	DWORD dwAmount = min(pBlock->Amount, buffSize);
	memcpy(pBuff, pBlock->Data, dwAmount);

	// Return the block
	ipc_server_ret_block(server, pBlock);

	// Success
	return dwAmount;
}

struct Block *ipc_server_get_block(struct IPCServer *server, DWORD dwTimeout)
{
	// Grab another block to read from
	// Enter a continous loop (this is to make sure the operation is atomic)
	for (;;) {
		// Check if there is room to expand the read start cursor
		LONG blockIndex = server->m_pBuf->m_ReadStart;
		struct Block *pBlock = server->m_pBuf->m_Blocks + blockIndex;
		if (pBlock->Next == server->m_pBuf->m_WriteEnd) {
			// No room is available, wait for room to become available
			if (WaitForSingleObject(server->m_hSignal, dwTimeout) ==
			    WAIT_OBJECT_0)
				if (!server->m_exit)
					continue;

			// Timeout
			return NULL;
		}

		// Make sure the operation is atomic
		if (InterlockedCompareExchange(&server->m_pBuf->m_ReadStart,
					       pBlock->Next,
					       blockIndex) == blockIndex)
			return pBlock;

		// The operation was interrupted by another thread.
		// The other thread has already stolen this block, try again
		if (!server->m_exit)
			continue;
	}
}

void ipc_server_ret_block(struct IPCServer *server, struct Block *pBlock)
{
	// Set the done flag for this block
	pBlock->doneRead = 1;

	// Move the read pointer as far forward as we can
	for (;;) {
		// Try and get the right to move the poitner
		DWORD blockIndex = server->m_pBuf->m_ReadEnd;
		pBlock = server->m_pBuf->m_Blocks + blockIndex;
		if (InterlockedCompareExchange(&pBlock->doneRead, 0, 1) != 1) {
			// If we get here then another thread has already moved the pointer
			// for us or we have reached as far as we can possible move the pointer
			return;
		}

		// Move the pointer one forward (interlock protected)
		InterlockedCompareExchange(&server->m_pBuf->m_ReadEnd,
					   pBlock->Next, blockIndex);

		// Signal availability of more data but only if a thread is waiting
		if (pBlock->Prev == server->m_pBuf->m_WriteStart)
			SetEvent(server->m_hAvail);
	}
}

void ipc_client_create(struct IPCClient **input)
{
	*input = (struct IPCClient *)calloc(1, sizeof(struct IPCClient));
	struct IPCClient *client = *input;

	client->m_sAddr = IPC_MEMORY_NAME;

	char *m_sEvtAvail = (char *)malloc(IPC_MAX_ADDR);
	if (!m_sEvtAvail)
		return;
	sprintf_s(m_sEvtAvail, IPC_MAX_ADDR, "%s_evt_avail", client->m_sAddr);

	char *m_sEvtFilled = (char *)malloc(IPC_MAX_ADDR);
	if (!m_sEvtFilled) {
		free(m_sEvtAvail);
		return;
	}
	sprintf_s(m_sEvtFilled, IPC_MAX_ADDR, "%s_evt_filled", client->m_sAddr);

	char *m_sMemName = (char *)malloc(IPC_MAX_ADDR);
	if (!m_sMemName) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		return;
	}
	sprintf_s(m_sMemName, IPC_MAX_ADDR, "%s_mem", client->m_sAddr);

	// Create the events
	client->m_hSignal =
		CreateEventA(NULL, FALSE, FALSE, (LPCSTR)m_sEvtFilled);
	if (client->m_hSignal == NULL ||
	    client->m_hSignal == INVALID_HANDLE_VALUE) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}
	client->m_hAvail =
		CreateEventA(NULL, FALSE, FALSE, (LPCSTR)m_sEvtAvail);
	if (client->m_hAvail == NULL ||
	    client->m_hSignal == INVALID_HANDLE_VALUE) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}

	// Open the shared file
	client->m_hMapFile =
		OpenFileMappingA(FILE_MAP_ALL_ACCESS, // read/write access
				 FALSE,               // do not inherit the name
				 m_sMemName);         // name of mapping object
	if (client->m_hMapFile == NULL ||
	    client->m_hMapFile == INVALID_HANDLE_VALUE) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}

	// Map to the file
	client->m_pBuf = (struct MemBuff *)MapViewOfFile(
		client->m_hMapFile,  // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0, 0, sizeof(struct MemBuff));
	if (client->m_pBuf == NULL) {
		free(m_sEvtAvail);
		free(m_sEvtFilled);
		free(m_sMemName);
		return;
	}

	// Release memory
	free(m_sEvtAvail);
	free(m_sEvtFilled);
	free(m_sMemName);
}

void ipc_client_destroy(struct IPCClient **input)
{
	struct IPCClient *client = *input;
	// Close the event
	CloseHandle(client->m_hSignal);

	// Close the event
	CloseHandle(client->m_hAvail);

	// Unmap the memory
	UnmapViewOfFile(client->m_pBuf);

	// Close the file handle
	CloseHandle(client->m_hMapFile);

	*input = NULL;
}

DWORD ipc_client_write(struct IPCClient *client, void *pBuff, DWORD amount,
		       DWORD dwTimeout)
{
	if (!client->m_pBuf)
		return 0;
	DWORD remainBytes = amount;
	DWORD index = 0;
	while (remainBytes > 0) {
		// Grab a block
		struct Block *pBlock = ipc_client_get_block(client, dwTimeout);
		if (!pBlock)
			return 0;

		// Copy the data
		DWORD dwAmount = min(remainBytes, IPC_BLOCK_SIZE);
		memcpy(pBlock->Data, (uint8_t *)pBuff + index, dwAmount);
		pBlock->Amount = dwAmount;

		// Post the block
		ipc_client_post_block(client, pBlock);

		index += dwAmount;
		remainBytes -= dwAmount;
	}

	// Fail
	return 0;
}

bool ipc_client_wait_available(struct IPCClient *client, DWORD dwTimeout)
{
	// Wait on the available event
	if (WaitForSingleObject(client->m_hAvail, dwTimeout) != WAIT_OBJECT_0)
		return false;

	// Success
	return true;
}

struct Block *ipc_client_get_block(struct IPCClient *client, DWORD dwTimeout)
{
	// Grab another block to write too
	// Enter a continous loop (this is to make sure the operation is atomic)
	for (;;) {
		// Check if there is room to expand the write start cursor
		LONG blockIndex = client->m_pBuf->m_WriteStart;
		struct Block *pBlock = client->m_pBuf->m_Blocks + blockIndex;
		if (pBlock->Next == client->m_pBuf->m_ReadEnd) {
			// No room is available, wait for room to become available
			if (WaitForSingleObject(client->m_hAvail, dwTimeout) ==
			    WAIT_OBJECT_0)
				continue;

			// Timeout
			return NULL;
		}

		// Make sure the operation is atomic
		if (InterlockedCompareExchange(&client->m_pBuf->m_WriteStart,
					       pBlock->Next,
					       blockIndex) == blockIndex)
			return pBlock;

		// The operation was interrupted by another thread.
		// The other thread has already stolen this block, try again
		continue;
	}
}

void ipc_client_post_block(struct IPCClient *client, struct Block *pBlock)
{
	// Set the done flag for this block
	pBlock->doneWrite = 1;

	// Move the write pointer as far forward as we can
	for (;;) {
		// Try and get the right to move the poitner
		DWORD blockIndex = client->m_pBuf->m_WriteEnd;
		pBlock = client->m_pBuf->m_Blocks + blockIndex;
		if (InterlockedCompareExchange(&pBlock->doneWrite, 0, 1) != 1) {
			// If we get here then another thread has already moved the pointer
			// for us or we have reached as far as we can possible move the pointer
			return;
		}

		// Move the pointer one forward (interlock protected)
		InterlockedCompareExchange(&client->m_pBuf->m_WriteEnd,
					   pBlock->Next, blockIndex);

		// Signal availability of more data but only if threads are waiting
		if (pBlock->Prev == client->m_pBuf->m_ReadStart)
			SetEvent(client->m_hSignal);
	}
}

bool ipc_client_is_ok(struct IPCClient *client)
{
	if (client->m_pBuf)
		return true;
	else
		return false;
}
