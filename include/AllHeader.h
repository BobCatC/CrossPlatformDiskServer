#pragma once
//////////////////////////////////////////////////////
//					AllHeader.h						//
//			  The main header for all files			//
//													//
//											   2017 //
//											  M3103 //
//										   Pakhomov	//
//							   Aleksandr Sergeevich	//
//////////////////////////////////////////////////////
#include<stdbool.h>
#include<stdio.h>
#include<time.h>

// Cross platforming
///////////////////////////////////////////////
#define WIN32 1
///////////////////////////////////////////////
#ifdef WIN32
#include<Windows.h>
#include<winsock.h>
#pragma comment(lib, "wsock32.lib")

#define LISTEN_PORT_NUM					80
#define PAUSE
#define RETURN_THREAD_TYPE				DWORD __stdcall 
#define PTHREAD_TYPE					PTHREAD_START_ROUTINE
#define INVALID_FIND_FILE_VALUE			INVALID_HANDLE_VALUE
#define PTHREAD_MUTEX_INITIALIZER		0
#define SL								"\\"
#define __SL__							'\\'

typedef HANDLE MUTEX;
typedef SOCKET CPSOCKET;
typedef HANDLE THREAD;
typedef HANDLE FINDFILE;

#else /////////////////////////////////
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include<fcntl.h>
#include<unistd.h>
#include<netinet/in.h> 
#include <dirent.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>

#define LISTEN_PORT_NUM						1025
#define PAUSE								{char cPAUSE; scanf("%c", &cPAUSE);}
#define RETURN_THREAD_TYPE					void*
#define PTHREAD_TYPE(pStartRoutine)			void* (*pStarRoutine)(void*)
#define INVALID_FIND_FILE_VALUE				(0)
#define SOCKET_ERROR						(-1)
#define INVALID_SOCKET						SOCKET_ERROR
#define WSAEWOULDBLOCK						EWOULDBLOCK
#define Sleep(ms)							usleep(ms)
#define inline
#define ZeroMemory(p, size)					memset(p, 0, size);
#define SL									"/"
#define __SL__								'/'
typedef void* HANDLE;
typedef pthread_mutex_t* MUTEX;
typedef int CPSOCKET;
typedef	pthread_t THREAD;
typedef DIR* FINDFILE;

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned long long DWORDLONG;

#define GetLastError() errno
void _i64toa(DWORDLONG i, char* Buf, int SS);
int fopen_s(FILE** ppFile, char* szFileName, char* szMode);
#endif
//
#define X_NO_ERROR						0x00
#define X_WSA_STARTUP_ERROR				0x01
#define X_SOCKET_CREATION_ERROR			0x02
#define X_SOCKET_BIND_ERROR				0x03
#define X_SOCKET_LISTEN_ERROR			0x04
#define X_SOCKET_CLOSED					0x05
#define X_MEMORY_MALLOC_ERROR			0x10
#define X_SEMAPHORE_CREATION_ERROR		0x20
#define X_SEMAPHORE_WAIT_ERROR			0x21
#define X_NO_FREE_THREAD				0x30
#define X_THREAD_CREATION_ERROR			0x40
#define X_METHOD_NO_END					0x50
#define X_METHOD_UNKNOWN				0x51	
#define X_METHOD_NOT_ALLOWED			0x52
#define X_BAD_REQUEST					0x60
#define X_NOT_FOUND_URI					0x70
#define X_SERVER_ERROR					0x80
#define X_CLIENT_DISCONNECTED			0x90
//
#define N_MAX_THREADS					64
#define C_MAX_PATH_STRING_LEN			520
// N max times, when mainloop can't get the semaphore for more then certain time, after it terminates the thread
#define N_MAX_MISTAKES					5
#define N_MAX_HEADERS					256
// Just a habit
#define bool _Bool
// for time_t
#define INVALID_TIME					0xFFFFFFFFFFFFFFFF
// Mode of not to send to socket, just save as a file (doesn't work in this project, was made for special offline tester)
// It'll save a file, but it'll need a client connection and a request
#define OFFLINE_MODE					0
#define WRITE_CONSOLE					
//#define WRITE_FILE						
//
typedef struct tagCLIENT_SOCKET_INFO {
	SOCKADDR_IN CSI_SockAddr;
	CPSOCKET CSI_Socket;
	time_t CSI_ConnectionTime;
}CLIENT_SOCKET_INFO;

typedef struct tagLOOP_THREAD_COM {
	CLIENT_SOCKET_INFO LTC_ClientInfo;
	THREAD LTC_hCurrentThread;
	int LTC_ThreadID;
	MUTEX LTC_hMainMutex;
	DWORD* LTC_pnWorkingThreads;
	bool* LTC_pbFreeThreads;
} LOOP_THREAD_COM;

typedef struct tagFILEINFO {
	char FI_szFileName[MAX_PATH];
	char FI_szLastModificationTime[80];
	char FI_szSize[40];
	bool FI_bFolder;
}FILEINFO;
//
int cpSetSocketNonBlock(CPSOCKET Socket);

int cpclosesocket_X(CPSOCKET* pSocket);

int free_X(void** pp);

int cpTerminateThread_X(THREAD* phThread, DWORD dwExitCode);

int cpGetFileSize(FILE* pFile, DWORD* pcbFileSize);

THREAD cpCreateThread(void* lpStartAddress, void* lpParameter);

MUTEX cpCreateMutex(bool bInit);

int cpWaitMutex(MUTEX Mutex);

int cpFreeMutex(MUTEX Mutex);

int cpDeleteMutex(MUTEX Mutex);

FINDFILE cpFindFirstFile(char* szFileName, FILEINFO* pFileInfo);

bool cpFindNextFile(char* szPathFINDFILE, FINDFILE hFindFile, FILEINFO* pFileInfo);

int cpCloseFindFile(FINDFILE hFindFile);
//
#ifdef WRITE_CONSOLE 
#define log printf

#else
#ifdef WRITE_FILE
#define log(String, ...) fprintf(pLOGFILE, String, __VA_ARGS__) 
#else 
#define log(...)
#endif
#endif
// NO log Debug
#define logDBG 