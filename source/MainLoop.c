//////////////////////////////////////////////////////
//				     MainLoop.c						//
//				   The main loop					//
//				 Processes connections				//
//											   2017 //
//											  M3103 //
//										   Pakhomov	//
//							   Aleksandr Sergeevich	//
//////////////////////////////////////////////////////

#include"../include/MainLoopHeader.h"


FILE* pLOGFILE;


int main() {
	
	int iError = 0;
	MUTEX hMainMutex;
	bool bFreeThread[N_MAX_THREADS] = { true };
	DWORD nWorkingThreads = 0;
	//
	CPSOCKET MainListenSocket = INVALID_SOCKET;
	const short iMainListenAf = AF_INET;
	const int MainListenSockType = SOCK_STREAM;
	const int MainListenSockProto = IPPROTO_TCP;
	const u_short iPort = LISTEN_PORT_NUM;
	const int MainListenSockBacklog = 20;
	DWORD dwFlags = 0;
	//
	CPSOCKET ConnectedSocket = INVALID_SOCKET;
	int cbCSinSize;
	int* pcbCSinSize = 0;
	//
	DWORD iFirstFreeThread = 0;
	const DWORD nMaxWorkingThreads = N_MAX_THREADS;
	LOOP_THREAD_COM* pThreadComPorts = 0;
	//
	
	//
	memset(bFreeThread, true, N_MAX_HEADERS);
	pLOGFILE = fopen("../LogFile.txt", "w");

#ifdef WIN32
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
		EndWorkServer(MainListenSocket, pThreadComPorts, 0, bFreeThread);
		return X_WSA_STARTUP_ERROR;
	}
#endif
	MainListenSocket = socket((int)iMainListenAf, MainListenSockType, MainListenSockProto);
	if (MainListenSocket == SOCKET_ERROR) {
		EndWorkServer(MainListenSocket, pThreadComPorts, 0, bFreeThread);
		return X_SOCKET_CREATION_ERROR;
	}
	log("Main listen socket is %i\n", MainListenSocket);
	iError = PrepareSocketToAccept(MainListenSocket, iMainListenAf, iPort, MainListenSockBacklog);
	if (iError != X_NO_ERROR) {
		EndWorkServer(MainListenSocket, pThreadComPorts, 0, bFreeThread);
		return iError;
	}

	cpSetSocketNonBlock(MainListenSocket);
	
	cbCSinSize = sizeof(SOCKADDR);
	pcbCSinSize = &cbCSinSize;	

	pThreadComPorts = (LOOP_THREAD_COM*)malloc(sizeof(LOOP_THREAD_COM) * nMaxWorkingThreads);
	if (pThreadComPorts == 0) {
		EndWorkServer(MainListenSocket, pThreadComPorts, 0, bFreeThread);
		return  X_MEMORY_MALLOC_ERROR;
	}
	iError = InitializeThreadComPorts(pThreadComPorts, nMaxWorkingThreads);
	if (iError != X_NO_ERROR) {
		EndWorkServer(MainListenSocket, pThreadComPorts, 0, bFreeThread);
		return iError;
	}
	hMainMutex = cpCreateMutex(0);
	cpFreeMutex(hMainMutex);
	log("Server started		OK\r\n");
	logDBG("Port num is %d\n", iPort);
	while (true) { 
		ConnectedSocket = accept(MainListenSocket, 
								 (SOCKADDR*)&pThreadComPorts[iFirstFreeThread].LTC_ClientInfo.CSI_SockAddr, 
								 pcbCSinSize);
		
		logDBG("New socket is %i\n", ConnectedSocket);						
		if (ConnectedSocket == SOCKET_ERROR) {
			Sleep(4);
			//SearchFirstFreeThread(nMaxWorkingThreads, &iFirstFreeThread);
			continue;
		}
		pThreadComPorts[iFirstFreeThread].LTC_ClientInfo.CSI_Socket = ConnectedSocket;
		iError = DestributeNewClient(pThreadComPorts + iFirstFreeThread, 
		iFirstFreeThread, hMainMutex, &nWorkingThreads, bFreeThread);
		ConnectedSocket = 0;
		
		iError = SearchFirstFreeThread(nMaxWorkingThreads, &iFirstFreeThread,
										hMainMutex, bFreeThread);
		while (iError == X_NO_FREE_THREAD) {
			Sleep(20);
			iError = SearchFirstFreeThread(nMaxWorkingThreads, &iFirstFreeThread, 
											hMainMutex, bFreeThread);
		}
		
	}
	EndWorkServer(MainListenSocket, pThreadComPorts, hMainMutex, bFreeThread);
	cpDeleteMutex(hMainMutex);
	fclose(pLOGFILE);
	return iError;
}

int InitializeThreadComPorts(LOOP_THREAD_COM* pPorts, DWORD nMaxPorts) {
	DWORD i;
	ZeroMemory(pPorts, sizeof(*pPorts) * nMaxPorts);
	for (i = 0; i < nMaxPorts; ++i) {
		pPorts[i].LTC_ClientInfo.CSI_Socket = INVALID_SOCKET;
		pPorts[i].LTC_ClientInfo.CSI_ConnectionTime = INVALID_TIME;
		pPorts[i].LTC_ThreadID = -1;
	}
	return X_NO_ERROR;
}

int DestroyThreadComPorts(LOOP_THREAD_COM* pPorts, MUTEX hMainMutex, bool* pbFree) {
	DWORD i;
	//
	if (pPorts == 0) return X_NO_ERROR;
	cpWaitMutex(hMainMutex);
	for (i = 0; i < N_MAX_THREADS; ++i) {
		if (pbFree[i])
			continue;
		cpTerminateThread_X(&pPorts[i].LTC_hCurrentThread, 0);
		SendCloseMessage(pPorts[i].LTC_ClientInfo.CSI_Socket);
	}
	cpFreeMutex(hMainMutex);
	return X_NO_ERROR;
}

int SearchFirstFreeThread(DWORD nMaxThreads, DWORD* piFirstFree, MUTEX hMainMutex, bool* pbFree) {
	cpWaitMutex(hMainMutex);
	for (*piFirstFree = 0; *piFirstFree < nMaxThreads; ++*piFirstFree)
		if (pbFree[*piFirstFree])
			break;
	cpFreeMutex(hMainMutex);
	if (*piFirstFree == nMaxThreads)
		return X_NO_FREE_THREAD;
	return X_NO_ERROR;
}

int DestributeNewClient(LOOP_THREAD_COM* pCurPort, DWORD ThreadID, MUTEX hMainMutex, 
						DWORD* pnWorking, bool* pbFree) {
	cpWaitMutex(hMainMutex);
	pCurPort->LTC_ClientInfo.CSI_ConnectionTime = clock(); 
	pCurPort->LTC_hMainMutex = hMainMutex;
	pCurPort->LTC_ThreadID = ThreadID;
	pCurPort->LTC_pnWorkingThreads = pnWorking;
	pCurPort->LTC_pbFreeThreads = pbFree;
	pCurPort->LTC_hCurrentThread = cpCreateThread(SocketThread, pCurPort);
	pbFree[ThreadID] = false;
	++*pnWorking;
	log("Thread  %i started.	Working now %X \n", ThreadID, *pnWorking);
	cpFreeMutex(hMainMutex);	
	return X_NO_ERROR;
}

int EndWorkServer(CPSOCKET MainListenSocket, LOOP_THREAD_COM* pThreadComPorts,
				  MUTEX hMainMutex, bool* pbFree) {
	cpclosesocket_X(&MainListenSocket);
	if (pThreadComPorts != 0)
		DestroyThreadComPorts(pThreadComPorts, hMainMutex, pbFree);
	log("Server Ended work\r\n");
#ifdef WIN32
	WSACleanup();
#endif
	return 0;
}
