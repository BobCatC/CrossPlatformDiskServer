#pragma once
//////////////////////////////////////////////////////
//				  MainLoopHeader.h					//
//				   The main loop					//
//				 Processes connections				//
//											   2017 //
//											  M3103 //
//										   Pakhomov	//
//							   Aleksandr Sergeevich	//
//////////////////////////////////////////////////////

#include"AllHeader.h"
#include"ThreadHeader.h"
#include"SocketProcessorHeader.h"
//
int main();

int InitializeThreadComPorts(LOOP_THREAD_COM* pPorts, DWORD nMaxPorts);

int DestroyThreadComPorts(LOOP_THREAD_COM* pPorts, MUTEX hMainMutex, bool* pbFree);

inline int SearchFirstFreeThread(DWORD nMaxThreads, DWORD* piFirstFree, MUTEX hMainMutex, bool* pbFree);

int DestributeNewClient(LOOP_THREAD_COM* pCurPort, DWORD ThreadID, MUTEX hMainMutex, DWORD* pnWorking, bool* pbFree);

int EndWorkServer(CPSOCKET MainListenSocket, LOOP_THREAD_COM* pThreadComPorts, MUTEX hMainMutex, bool* pbFree);
//