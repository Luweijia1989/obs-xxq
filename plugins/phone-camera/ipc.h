#pragma once

#include <windows.h>
#include <memory.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


// Definitions
#define IPC_BLOCK_COUNT 4096
#define IPC_BLOCK_SIZE 4096

#define IPC_MAX_ADDR 256

#define IPC_MEMORY_NAME "MirrorMappingMemory"

typedef void (*read_cb)(void *param, uint8_t *data, size_t size);

struct Block {
	// Variables
	LONG Next; // Next block in the circular linked list
	LONG Prev; // Previous block in the circular linked list

	volatile LONG
		doneRead; // Flag used to signal that this block has been read
	volatile LONG
		doneWrite; // Flag used to signal that this block has been written

	DWORD Amount;   // Amount of data help in this block
	DWORD _Padding; // Padded used to ensure 64bit boundary

	BYTE Data[IPC_BLOCK_SIZE]; // Data contained in this block
};

struct MemBuff {
	// Block data, this is placed first to remove the offset (optimisation)
	struct Block m_Blocks
		[IPC_BLOCK_COUNT]; // Array of buffers that are used in the communication

	// Cursors
	volatile LONG m_ReadEnd;   // End of the read cursor
	volatile LONG m_ReadStart; // Start of read cursor

	volatile LONG
		m_WriteEnd; // Pointer to the first write cursor, i.e. where we are currently writting to
	volatile LONG
		m_WriteStart; // Pointer in the list where we are currently writting
};

struct IPCServer {
	char m_sAddr[IPC_MAX_ADDR];     // Address of this server
	HANDLE m_hMapFile; // Handle to the mapped memory file
	HANDLE m_hSignal;  // Event used to signal when data exists
	HANDLE m_hAvail; // Event used to signal when some blocks become available
	HANDLE m_workingMutex; // mutex for client to check server is still working
	struct MemBuff *m_pBuf; // Buffer that points to the shared memory
	BOOL m_exit;
	HANDLE m_readThread;
	read_cb cb;
	void *m_cbParam;
};

struct IPCClientPrivate {
	char m_sAddr[IPC_MAX_ADDR];     // Address of this server
	HANDLE m_hMapFile; // Handle to the mapped memory file
	HANDLE m_hSignal;  // Event used to signal when data exists
	HANDLE m_hAvail; // Event used to signal when some blocks become available
	struct MemBuff *m_pBuf; // Buffer that points to the shared memory
};

struct IPCClient {
	struct IPCClientPrivate *m_private;
	CRITICAL_SECTION m_mutex;
};

void ipc_server_create(struct IPCServer **input, const char *name, read_cb cb, void *param);
void ipc_server_destroy(struct IPCServer **input);
DWORD ipc_server_read(struct IPCServer *server, void *pBuff, DWORD buffSize, DWORD timeout);
struct Block *ipc_server_get_block(struct IPCServer *server, DWORD dwTimeout);
void ipc_server_ret_block(struct IPCServer *server, struct Block *pBlock);

void ipc_client_create(struct IPCClient **input, const char *name);
void ipc_client_destroy(struct IPCClient **input);
int ipc_client_write(struct IPCClient *client, void *pBuff, DWORD amount, DWORD dwTimeout);
int ipc_client_write_2(struct IPCClient *client, void *pBuff, DWORD amount, void *pBuff2, DWORD amount2, DWORD dwTimeout);

#ifdef __cplusplus
}
#endif
