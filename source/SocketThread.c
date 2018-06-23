//////////////////////////////////////////////////////
//				   SocketThread.c					//
//				   The main file					//
//				 Processes requests					//
//											   2017 //
//											  M3103 //
//										   Pakhomov	//
//							   Aleksandr Sergeevich	//
//////////////////////////////////////////////////////

#include"../include/ThreadHeader.h"
 
extern FILE* pLOGFILE;

RETURN_THREAD_TYPE SocketThread(void* pPar) { 
	DWORD iError = X_NO_ERROR;
	DWORD iSocketError = 0;
	MUTEX hMutex;
	DWORD* pnWorkingThreads;
 	bool* pbFreeThread;
	//
	LOOP_THREAD_COM* pCom = 0;
	CPSOCKET Socket = 0;
	SOCKADDR_IN Sin;
	//
	DWORD cbRecvSize = 0;
	const DWORD cbMaxRecvSize = 1024 * 64 + 1;
	char* InBuf = 0;
	bool bSendCloseMessage = false;
	bool bEndWork = false;
	//
	pCom = (LOOP_THREAD_COM*)pPar;
	
	hMutex = pCom->LTC_hMainMutex;
	pnWorkingThreads = pCom->LTC_pnWorkingThreads; 
	pbFreeThread = pCom->LTC_pbFreeThreads;
	Socket = pCom->LTC_ClientInfo.CSI_Socket;
	memcpy(&Sin, &pCom->LTC_ClientInfo.CSI_SockAddr, sizeof(Sin));
	InBuf = (char*)malloc(cbMaxRecvSize);
	if (InBuf == 0) {
		EndWorkingThread(pCom, true, pnWorkingThreads, pbFreeThread);
		#ifdef WIN32
		return X_MEMORY_MALLOC_ERROR;
		#else
		return (void*)X_MEMORY_MALLOC_ERROR;
		#endif
	}

	bSendCloseMessage = true;

	while (true) {
		cbRecvSize = recv(Socket, InBuf, cbMaxRecvSize, 0);
		if (cbRecvSize == 0) {
			bSendCloseMessage = false;
			break;
		}
		if (cbRecvSize != -1) {
			logDBG("Got msg\n");
			if (cbRecvSize == cbMaxRecvSize) {
				SendStdMsg(Socket, PAYLOAD_TOO_LARGE, 0, true);
				break;
			}
			iError = ProcessRequest(Socket, InBuf, cbRecvSize, &bEndWork);
		}
		else
		{
			iSocketError = GetLastError();
			if (iSocketError == WSAEWOULDBLOCK) {
				iSocketError = iSocketError;
			}
			else {
				iSocketError = iSocketError;
				bSendCloseMessage = true;
				break;
			}
			Sleep(5);
			continue;
		}
		if (bEndWork)
			break;		
	}

	free(InBuf);
	EndWorkingThread(pCom, bSendCloseMessage, pnWorkingThreads, pbFreeThread);
	#ifdef WIN32
	return X_NO_ERROR;
	#else 
	return (void*)X_NO_ERROR;
	#endif
}

int EndWorkingThread(LOOP_THREAD_COM* pCom, bool bSendCloseMessage, DWORD* pnWorking, bool* pbFree) {
	MUTEX hMutex = pCom->LTC_hMainMutex;
	//
	if (!cpWaitMutex(hMutex)) {
		log("Mutex lock error\n");
		return X_SEMAPHORE_WAIT_ERROR;
	} 
	*pnWorking -= 1;
	pbFree[pCom->LTC_ThreadID] = true;
	log("Thread  %i ended work.	Working now %i\r\n", pCom->LTC_ThreadID, *pnWorking);
	if (bSendCloseMessage)
		SendCloseMessage(pCom->LTC_ClientInfo.CSI_Socket);
	
	cpclosesocket_X(&pCom->LTC_ClientInfo.CSI_Socket);
	ZeroMemory(pCom, sizeof(*pCom));
	pCom->LTC_ClientInfo.CSI_ConnectionTime = INVALID_TIME;
	cpFreeMutex(hMutex);
	return X_NO_ERROR;
}

int ProcessRequest(CPSOCKET Socket, char* InBuf, DWORD cbMsgSize, bool* pbEndWork) {
	int iError;
	//
	X_PARSING_CONTEXT ParsingContext;
	X_PARSED_MSG ParsedMsg;

	X_METHOD_CONTEXT MethodContext;

	X_HEADERS_CONTEXT HeadersContext;
	//

	ParsingContext.Buf = malloc(cbMsgSize + 1);
	memcpy(ParsingContext.Buf, InBuf, cbMsgSize);
	ParsingContext.cbBufSize = cbMsgSize;
	ParsingContext.iPos = 0;
	/*  Parsing:
			Split all strings into 3 groups:
				1> sStarting line
				2> Massive of headers
				3> sBody
	*/
	iError = ParseMsg(&ParsingContext, &ParsedMsg);
	if (iError != X_NO_ERROR) {
		logDBG("\tParsing error\n");
		SendStdMsg(Socket, BAD_REQUEST, 0, false);
		free(ParsingContext.Buf);
		return X_NO_ERROR;
	}
	logDBG("\tParse OK\n");
	if (ParsedMsg.PM_HttpVersion == HTTP_VERSION_UNKNOWN || ParsedMsg.PM_HttpVersion != HTTP_VERSION_1D1) {
		SendStdMsg(Socket, HTTP_VERSION_NOT_SUPPORTED, 0, false);
		free(ParsingContext.Buf);
		return X_NO_ERROR;
	}

	iError = IdentifyMethod(&ParsedMsg, &MethodContext);
	if (iError != X_NO_ERROR) {
		logDBG("\tMethod error\n");
		SendStdMsg(Socket, X_BAD_REQUEST, 0, false);
		free(ParsingContext.Buf);
		return X_NO_ERROR;
	}
	logDBG("\tMethod OK\n");
	if (MethodContext.MC_iMethod == HTTP_METHOD_UNKOWN) {
		SendStdMsg(Socket, NOT_IMPLEMENTED, 0, false);
		free(ParsingContext.Buf);
		return X_NO_ERROR;
	}
	if (MethodContext.MC_iMethod != HTTP_METHOD_GET) {
		SendStdMsg(Socket, METHOD_NOT_ALLOWED, "Allow: GET\r\n", false);
		free(ParsingContext.Buf);
		return X_NO_ERROR;
	}
	/*	Identifiing only important headers
			Connection, if-none-match		
	*/
	iError = IdentifyHeaders(&ParsedMsg, &HeadersContext);
	*pbEndWork = HeadersContext.HC_bCloseSession;
	if (iError != X_NO_ERROR) {
		logDBG("\tHeaders error\n");
		SendStdMsg(Socket, BAD_REQUEST, 0, false);
		free(ParsingContext.Buf);
		return X_NO_ERROR;
	}
	logDBG("\tHeaders OK\n");
	//
	if(MethodContext.MC_iMethod == HTTP_METHOD_GET) 
		iError = ProcessGET(Socket, &MethodContext, &HeadersContext);

	free(ParsingContext.Buf);
	logDBG("Processing request PS.buf free OK\n");
	return X_NO_ERROR;
}

int ParseMsg(X_PARSING_CONTEXT* pContext, X_PARSED_MSG* pParsedMsg) {
	DWORD iStart = 0;
	char* pStrings[N_MAX_HEADERS + 3] = { 0 };
	DWORD pcbStringsSizes[512] = { 0 };
	DWORD nStrings = 0;
	DWORD iString;
	//
	pContext->iPos = 0;
	
	pParsedMsg->PM_StartingLine = 0;
	pParsedMsg->PM_cbStartingLineSize = 0;
	ZeroMemory(pParsedMsg->PM_Headers, sizeof(*pParsedMsg->PM_Headers) * N_MAX_HEADERS);
	ZeroMemory(pParsedMsg->PM_cbHeadersSizes, sizeof(*pParsedMsg->PM_cbHeadersSizes) * N_MAX_HEADERS);
	pParsedMsg->PM_nHeaders = 0;
	pParsedMsg->PM_HttpVersion = HTTP_VERSION_UNKNOWN;
	pParsedMsg->PM_MessageBody = 0;
	pParsedMsg->PM_cbMessageBodySize = 0;
	
	iStart = 0;
	while (pContext->iPos < pContext->cbBufSize) {
		if (pContext->Buf[pContext->iPos] == '\n'){
			if (nStrings == N_MAX_HEADERS + 3)
				return X_BAD_REQUEST;
			pStrings[nStrings] = pContext->Buf + iStart;
			pcbStringsSizes[nStrings] = pContext->iPos - iStart;
			if (pContext->iPos > 1 && pContext->Buf[pContext->iPos - 1] == '\r') {
				--pcbStringsSizes[nStrings];
			}
			iStart = pContext->iPos + 1;
			++nStrings;
			++pContext->iPos;
		}
		else {
			++pContext->iPos;
		}
	}
	
	if (pContext->Buf[pContext->iPos - 1] != '\n' || nStrings <= 2) 
		return X_BAD_REQUEST;
	pParsedMsg->PM_StartingLine = pStrings[0];
	pParsedMsg->PM_cbStartingLineSize = pcbStringsSizes[0];

	for (iString = 1; iString < nStrings - 2; ++iString) {
		pParsedMsg->PM_Headers[pParsedMsg->PM_nHeaders] = pStrings[iString];
		pParsedMsg->PM_cbHeadersSizes[pParsedMsg->PM_nHeaders] = pcbStringsSizes[iString];
		++pParsedMsg->PM_nHeaders;
	}
	if (pcbStringsSizes[nStrings - 2] != 0) {
		pParsedMsg->PM_Headers[pParsedMsg->PM_nHeaders] = pStrings[nStrings - 2];
		pParsedMsg->PM_cbHeadersSizes[pParsedMsg->PM_nHeaders] = pcbStringsSizes[nStrings - 2];
		++pParsedMsg->PM_nHeaders;
		if (pcbStringsSizes[nStrings - 1] != 0) {
			return X_BAD_REQUEST;
		}
	}
	else {
		if (pcbStringsSizes[nStrings - 1] != 0) {
			pParsedMsg->PM_MessageBody = pStrings[nStrings - 1];
			pParsedMsg->PM_cbMessageBodySize = pcbStringsSizes[nStrings - 1];
		}
	}
	return GetHTTPVersion(pParsedMsg);
}

int GetHTTPVersion(X_PARSED_MSG* pParsedMsg) {
	int iPos;
	//
	if (pParsedMsg->PM_cbStartingLineSize < 8) {
		pParsedMsg->PM_HttpVersion = HTTP_VERSION_UNKNOWN;
		return X_BAD_REQUEST;
	}

	iPos = pParsedMsg->PM_cbStartingLineSize - 8;
	while (iPos >= 0) {
		if (*(DWORD*)(pParsedMsg->PM_StartingLine + iPos) == *((DWORD*)"HTTP")) {
			if (*(DWORD*)(pParsedMsg->PM_StartingLine + iPos + 4) == *(DWORD*)"/0.9")
				pParsedMsg->PM_HttpVersion = HTTP_VERSION_0D9;
			else {
				if (*(DWORD*)(pParsedMsg->PM_StartingLine + iPos + 4) == *(DWORD*)"/1.0")
					pParsedMsg->PM_HttpVersion = HTTP_VERSION_1D0;
				else {
					if (*(DWORD*)(pParsedMsg->PM_StartingLine + iPos + 4) == *(DWORD*)"/1.1")
						pParsedMsg->PM_HttpVersion = HTTP_VERSION_1D1;
					else {
						if(*(DWORD*)(pParsedMsg->PM_StartingLine + iPos + 4) == *(DWORD*)"/2.0")
							pParsedMsg->PM_HttpVersion = HTTP_VERSION_2D0;
						else
							pParsedMsg->PM_HttpVersion = HTTP_VERSION_UNKNOWN;
					}
				}
			}
			break;
		}
		--iPos;
	}

	return X_NO_ERROR;
}

int IdentifyMethod(X_PARSED_MSG* pParsedMsg, X_METHOD_CONTEXT* pMethodContext) {
	//
	pMethodContext->MC_URI = 0;
	pMethodContext->MC_cbURISize = 0;
	pMethodContext->MC_iMethod = HTTP_METHOD_UNKOWN;
	if (pParsedMsg->PM_cbStartingLineSize < 14) {
		pMethodContext->MC_iMethod = HTTP_METHOD_UNKOWN;
		return X_METHOD_UNKNOWN;
	}
	if (*(DWORD*)pParsedMsg->PM_StartingLine == *(DWORD*)"GET ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_GET;
		pMethodContext->MC_URI = pParsedMsg->PM_StartingLine + 4;
		if (pParsedMsg->PM_cbStartingLineSize < 13)
			return X_BAD_REQUEST;
		pMethodContext->MC_cbURISize = pParsedMsg->PM_cbStartingLineSize - 13;
		return X_NO_ERROR;
	}
	if (((*(DWORDLONG*)pParsedMsg->PM_StartingLine) & 0x000000FFFFFFFFFF) == *(DWORDLONG*)"HEAD ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_HEAD;
		pMethodContext->MC_URI = pParsedMsg->PM_StartingLine + 5;
		if (pParsedMsg->PM_cbStartingLineSize < 14)
			return X_BAD_REQUEST;
		pMethodContext->MC_cbURISize = pParsedMsg->PM_cbStartingLineSize - 14;
		return X_NO_ERROR;
	}
	if (*(DWORDLONG*)pParsedMsg->PM_StartingLine == *(DWORDLONG*)"OPTIONS ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_OPTIONS;
		pMethodContext->MC_URI = pParsedMsg->PM_StartingLine + 8;
		if (pParsedMsg->PM_cbStartingLineSize < 17)
			return X_BAD_REQUEST;
		pMethodContext->MC_cbURISize = pParsedMsg->PM_cbStartingLineSize - 17;
		return X_NO_ERROR;
	}
	if (*(DWORDLONG*)pParsedMsg->PM_StartingLine == *(DWORDLONG*)"CONNECT ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_CONNECT;
		pMethodContext->MC_URI = 0;
		pMethodContext->MC_cbURISize = 0;
		return X_NO_ERROR;
	}
	if (((*(DWORDLONG*)pParsedMsg->PM_StartingLine) & 0x0000FFFFFFFFFFFF) == *(DWORDLONG*)"TRACE ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_TRACE;
		pMethodContext->MC_URI = 0;
		pMethodContext->MC_cbURISize = 0;
		return X_NO_ERROR;
	}
	if (((*(DWORDLONG*)pParsedMsg->PM_StartingLine) & 0x000000FFFFFFFFFF) == *(DWORDLONG*)"POST ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_POST;
		pMethodContext->MC_URI = pParsedMsg->PM_StartingLine + 5;
		if (pParsedMsg->PM_cbStartingLineSize < 14)
			return X_BAD_REQUEST;
		pMethodContext->MC_cbURISize = pParsedMsg->PM_cbStartingLineSize - 14;
		return X_NO_ERROR;
	}
	if (*(DWORD*)pParsedMsg->PM_StartingLine == *(DWORD*)"PUT ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_PUT;
		pMethodContext->MC_URI = pParsedMsg->PM_StartingLine + 4;
		if (pParsedMsg->PM_cbStartingLineSize < 13)
			return X_BAD_REQUEST;
		pMethodContext->MC_cbURISize = pParsedMsg->PM_cbStartingLineSize - 13;
		return X_NO_ERROR;
	}
	if (((*(DWORDLONG*)pParsedMsg->PM_StartingLine) & 0x00FFFFFFFFFFFFFF) == *(DWORDLONG*)"DELETE ") {
		pMethodContext->MC_iMethod = HTTP_METHOD_DELETE;
		pMethodContext->MC_URI = pParsedMsg->PM_StartingLine + 7;
		if (pParsedMsg->PM_cbStartingLineSize < 16)
			return X_BAD_REQUEST;
		pMethodContext->MC_cbURISize = pParsedMsg->PM_cbStartingLineSize - 16;
		return X_NO_ERROR;
	}
	pMethodContext->MC_iMethod = HTTP_METHOD_UNKOWN;
	return X_METHOD_UNKNOWN;
}

int IdentifyHeaders(X_PARSED_MSG* pParsedMsg, X_HEADERS_CONTEXT* pHeadersContext) {
	DWORD iHeader;
	char* sCurHeader;
	DWORD cbCurHeaderSize;
	DWORD iStart, iEnd;
	DWORD cbCurHeaderArgSize;
	// init
	pHeadersContext->HC_bCloseSession = true;
	pHeadersContext->HC_cbETagSize = 0;
	pHeadersContext->HC_sETag = 0;
	//
	for (iHeader = 0; iHeader < pParsedMsg->PM_nHeaders; ++iHeader) {
		sCurHeader = pParsedMsg->PM_Headers[iHeader];
		iStart = iEnd = 0;
		while (iEnd < pParsedMsg->PM_cbHeadersSizes[iHeader] &&
			sCurHeader[iEnd] != '\n' && sCurHeader[iEnd] != ':')
			++iEnd;
		if (iEnd == pParsedMsg->PM_cbHeadersSizes[iHeader] && sCurHeader[iEnd] == '\n')
			continue;
		cbCurHeaderSize = iEnd - iStart;
		if (cbCurHeaderSize == 10 && !strncmp(sCurHeader, "Connection", 10)) {
			iStart = iEnd + 1;
			while (iStart < pParsedMsg->PM_cbHeadersSizes[iHeader] &&
				sCurHeader[iStart] == ' ') ++iStart;
			if (iStart == pParsedMsg->PM_cbHeadersSizes[iHeader])
				continue;
			iEnd = iStart + 1;
			while (iEnd < pParsedMsg->PM_cbHeadersSizes[iHeader] &&
				sCurHeader[iEnd] != '\n') ++iEnd;
			if (iEnd == pParsedMsg->PM_cbHeadersSizes[iHeader] && sCurHeader[iEnd] != '\r')
				continue;
			cbCurHeaderArgSize = iEnd - iStart;
			if (cbCurHeaderArgSize == 5 && !strncmp(sCurHeader + iStart, "close", 5)) {
				pHeadersContext->HC_bCloseSession = true;
			}
			else {
				if (cbCurHeaderArgSize == 10 && !strncmp(sCurHeader + iStart, "keep-alive", 10)) {
					pHeadersContext->HC_bCloseSession = false;
				}
			}
		}
		if (cbCurHeaderSize == 13 && !strncmp(sCurHeader, "If-None-Match", 13)) {
			iStart = iEnd + 1;
			while (iStart < pParsedMsg->PM_cbHeadersSizes[iHeader] &&
				sCurHeader[iStart] != '\"') ++iStart;
			if (iStart == pParsedMsg->PM_cbHeadersSizes[iHeader])
				continue;
			iEnd = iStart + 1;
			while (iEnd < pParsedMsg->PM_cbHeadersSizes[iHeader] &&
				sCurHeader[iEnd] != '\"') ++iEnd;
			if (iEnd == pParsedMsg->PM_cbHeadersSizes[iHeader])
				continue;
			++iStart;
			cbCurHeaderArgSize = iEnd - iStart;
			pHeadersContext->HC_cbETagSize = cbCurHeaderArgSize;
			pHeadersContext->HC_sETag = sCurHeader + iStart;
		}
	}

	return X_NO_ERROR;
}

int ProcessGET(CPSOCKET Socket, X_METHOD_CONTEXT* pMethodContext, X_HEADERS_CONTEXT* pHeadersContext) {
	int iError;
	X_GET_CONTEXT GetContext;
	// For sending HTML of folder content
	char szDirectoryName[C_MAX_PATH_STRING_LEN] = SZ_START_FOLDER;
	DWORD cbNameSize;
	// For sending server files, like images, icons...
	char szServerDirectoryName[C_MAX_PATH_STRING_LEN];
	//
	DWORD iPos;
	DWORD iParamStart;
	DWORD nSlashes;
	// Virtual file of folder content. Parametr of URI Indicates, what folder client wants	
	// e.g. URI = FolderContent?Program files/* 
	const char szFolderContentFile[] = SZ_CONTENT_FOLDER_FILE; 
	const DWORD cbFolderContentFile = strlen(szFolderContentFile);
	// standartizeing the form of the URI. No first '/', no last '/', all '/' => '\\', decrypting russian symbols from UTF8 
	if (pMethodContext->MC_URI[0] == '/') {
		++pMethodContext->MC_URI;
		--pMethodContext->MC_cbURISize;
	}
	if (pMethodContext->MC_cbURISize > 1 && pMethodContext->MC_URI[pMethodContext->MC_cbURISize - 1] == '/') {
		--pMethodContext->MC_cbURISize;
	}
#ifdef WIN32
	for (DWORD i = 0; i < pMethodContext->MC_cbURISize; ++i) {
		if (pMethodContext->MC_URI[i] == '/')
			pMethodContext->MC_URI[i] = '\\';
	}
#endif
	pMethodContext->MC_URI[pMethodContext->MC_cbURISize] = 0;
	if (pMethodContext->MC_cbURISize != 0)
		pMethodContext->MC_cbURISize = DecryptRussian(pMethodContext->MC_URI, pMethodContext->MC_cbURISize);
	// Checking the word "FolderContent"
	if (pMethodContext->MC_cbURISize >= cbFolderContentFile &&
		!strncmp(pMethodContext->MC_URI, szFolderContentFile, cbFolderContentFile)) {
		cbNameSize = strlen(szDirectoryName);
		iParamStart = nSlashes = 0;
		iPos = pMethodContext->MC_cbURISize - 1;
		// Looking for param
		while (iParamStart < pMethodContext->MC_cbURISize &&
			pMethodContext->MC_URI[iParamStart] != '?') ++iParamStart;
		while (iPos > iParamStart) {
			if (pMethodContext->MC_URI[iPos] == __SL__) {
				++nSlashes;
				if (nSlashes == 2) break;
			}
			--iPos;
		}
		// All types of params
		"?*"; "?Users\\*"; "?Users\\All\\*";
		// Filling the context for creating HTML
		strncpy(szDirectoryName + cbNameSize, 
			pMethodContext->MC_URI + iParamStart + 1, 
			pMethodContext->MC_cbURISize - iParamStart - 1);
		strncpy(GetContext.GC_szCurrentDirrectory, pMethodContext->MC_URI + iParamStart + 1,
			pMethodContext->MC_cbURISize - iParamStart - 1);
		GetContext.GC_szCurrentDirrectory[pMethodContext->MC_cbURISize - iParamStart - 2] = 0;
		cbNameSize += pMethodContext->MC_cbURISize - iParamStart - 1;
		szDirectoryName[cbNameSize] = 0;
		GetContext.GC_Type = GET_FOLDER_CONTEXT;
		GetContext.GC_szFindData = szDirectoryName;
		GetContext.GC_cbFindDataSize = cbNameSize;
		if (nSlashes == 2) {
			strncpy(GetContext.GC_szMotherFolder, pMethodContext->MC_URI + iParamStart, iPos - iParamStart + 1);
			GetContext.GC_szMotherFolder[iPos - iParamStart + 1] = '*';
			GetContext.GC_szMotherFolder[iPos - iParamStart + 2] = 0;
		}
		else {
			if (nSlashes == 1) {
				GetContext.GC_szMotherFolder[0] = '*';
				GetContext.GC_szMotherFolder[1] = 0;
			}
			else
				GetContext.GC_szMotherFolder[0] = 0;
		}
		CreateHTMLofFolderContent(Socket, &GetContext);
		return X_NO_ERROR;
	}

	if (pMethodContext->MC_cbURISize == 0 || 
		pMethodContext->MC_cbURISize == 1 && pMethodContext->MC_URI[0] == '*') {
		// Filling the context for index request
		cbNameSize = strlen(szDirectoryName);
		szDirectoryName[cbNameSize] = '*';
		szDirectoryName[cbNameSize + 1] = 0;
		cbNameSize += 2;
		GetContext.GC_Type = GET_FOLDER_CONTEXT;
		GetContext.GC_szFindData = szDirectoryName;
		GetContext.GC_cbFindDataSize = cbNameSize;
		GetContext.GC_szCurrentDirrectory[0] = 0;
		GetContext.GC_szMotherFolder[0] = 0;
		CreateHTMLofFolderContent(Socket, &GetContext);
		return X_NO_ERROR;
	}
	// Filling the context for sending a file
	for (iPos = 0; iPos < pMethodContext->MC_cbURISize; ++iPos)
		if (pMethodContext->MC_URI[iPos] == '.')
			break;
	if (iPos == pMethodContext->MC_cbURISize) {
		SendStdMsg(Socket, NOT_FOUND, 0, false);
		return X_NOT_FOUND_URI;
	}
	cbNameSize = 0;
	strncpy(szServerDirectoryName + cbNameSize, pMethodContext->MC_URI, pMethodContext->MC_cbURISize);
	szServerDirectoryName[pMethodContext->MC_cbURISize + cbNameSize] = 0;
	cbNameSize = strlen(szServerDirectoryName);
	GetContext.GC_Type = GET_FILE;
	GetContext.GC_szFindData = szServerDirectoryName;
	GetContext.GC_cbFindDataSize = cbNameSize;
	iError = SendFile(Socket, &GetContext, pHeadersContext);
	return X_NO_ERROR;
}

int CreateHTMLofFolderContent(CPSOCKET Socket, X_GET_CONTEXT* pContext) {
	DWORD iPos = 0;
	const DWORD cbStartFolderSize = strlen(SZ_START_FOLDER);
	//
	FINDFILE hFindFile = 0;
	FILEINFO FileInfo;
	//
	char szFolderImage[] = SL SZ_SERVER_FILES_DIRECTORY"/Folder.jpg";
	char szFileImage[] = SL SZ_SERVER_FILES_DIRECTORY"/File.jpg";
	//
	char* OutBuf = 0;
	DWORD cbOutBufSize = 0;
	//
	#ifndef WIN32
	pContext->GC_szFindData[strlen(pContext->GC_szFindData) - 1] = 0;
	#endif
	OutBuf = (char*)malloc(CB_STD_HTML_SIZE);
	cbOutBufSize += WriteHTMLStart(OutBuf + cbOutBufSize, pContext->GC_szCurrentDirrectory);
	cbOutBufSize += WriteHTMLRefsUpper(OutBuf + cbOutBufSize, pContext->GC_szMotherFolder);

	hFindFile = cpFindFirstFile(pContext->GC_szFindData, &FileInfo);
	
	if (hFindFile == INVALID_FIND_FILE_VALUE) {
		cbOutBufSize += WriteHTMLNotAvailable(OutBuf + cbOutBufSize);
		cbOutBufSize += WriteHTMLEnd(OutBuf + cbOutBufSize);
	}
	else {
		if(FileInfo.FI_szFileName[0] == '.')cpFindNextFile(pContext->GC_szFindData, hFindFile, &FileInfo);
		if (FileInfo.FI_szFileName[0] == '.' &&
			!cpFindNextFile(pContext->GC_szFindData, hFindFile, &FileInfo)) {
			pContext->GC_szMotherFolder[0] = 0;
			cbOutBufSize += WriteHTMLEmpty(OutBuf + cbOutBufSize);
			cbOutBufSize += WriteHTMLEnd(OutBuf + cbOutBufSize);
		}
		else {
			cbOutBufSize += WriteHTMLStartTable(OutBuf + cbOutBufSize);
			while (1) {
				// Write all folders
				
				if (FileInfo.FI_bFolder && FileInfo.FI_szFileName[0] != '.') {
					cbOutBufSize += AddRecordFolder(OutBuf + cbOutBufSize,
						pContext->GC_szCurrentDirrectory, &FileInfo, szFolderImage);
				}
				if (!cpFindNextFile(pContext->GC_szFindData, hFindFile, &FileInfo))
					break;
			}
			cpCloseFindFile(hFindFile);
			
			hFindFile = cpFindFirstFile(pContext->GC_szFindData, &FileInfo);
			cpFindNextFile(pContext->GC_szFindData, hFindFile, &FileInfo);
			if (hFindFile != INVALID_FIND_FILE_VALUE &&
				cpFindNextFile(pContext->GC_szFindData, hFindFile, &FileInfo)) {
				while (1) {
					// Write all files
					
					if (!FileInfo.FI_bFolder) {
						cbOutBufSize += AddRecordFile(OutBuf + cbOutBufSize,
							pContext->GC_szCurrentDirrectory, &FileInfo, szFileImage);
					}
					if (!cpFindNextFile(pContext->GC_szFindData, hFindFile, &FileInfo))
						break;
				}
				cbOutBufSize += WriteHTMLEndTable(OutBuf + cbOutBufSize);
			}
			if(hFindFile != INVALID_FIND_FILE_VALUE)
				cpCloseFindFile(hFindFile);
			cbOutBufSize += WriteHTMLEnd(OutBuf + cbOutBufSize);
		}
	}
#if OFFLINE_MODE
	FILE* pFile = 0;
	fopen_s(&pFile, "HTML_ANSWER.html", "wb");
	//logDBG("pFile is %p\n", pFile);
	if (pFile != 0) {
		fwrite(OutBuf, 1, cbOutBufSize, pFile);
	}
	fclose(pFile);
#else
	SendHTMLAnswer(Socket, OutBuf, cbOutBufSize);
#endif
	free(OutBuf); 
	return X_NO_ERROR;
}

int WriteHTMLStart(char* Buf, char* szDirrectoryName) {
	DWORD cbReturnSize;
	if(szDirrectoryName[0] != 0)
		cbReturnSize = sprintf(Buf, "<html>\n<head>\n<meta charset = \"utf-8\">\n</head>\n<body><h1>Content of folder \"%s\"</h1>\n<hr>\n", szDirrectoryName);
	else
		cbReturnSize = sprintf(Buf, "<html>\n<head>\n<meta charset = \"utf-8\">\n</head>\n<body><h1>Content of the root folder</h1>\n<hr>\n");
	return EncryptRussian(Buf, cbReturnSize); 
}

int WriteHTMLRefsUpper(char* Buf, char* szMotherDirectory) {
	DWORD cbSize;
	if (szMotherDirectory[0] != 0) {
		cbSize = sprintf(Buf, "<h2><a href=\"/\"> Root  </a>\n<p><a href=\"%s\"> Mother directory </a></p></h2>\n", szMotherDirectory);
		return EncryptRussian(Buf, cbSize);
	}
	else {
		cbSize = sprintf(Buf, "<h2><p><a href=\"/\"> Root  </a></p></h2>\n");
		return EncryptRussian(Buf, cbSize);
	}
}

inline int WriteHTMLEmpty(char* Buf) {
	return sprintf(Buf, "<h2>This folder is <i>empty</i></h2>\n");
}

inline int WriteHTMLNotAvailable(char* Buf) {
	return sprintf(Buf, "<h2>This folder is <i>not available</i> or <i> doesn't exist</i></h2>\n");
}

inline int WriteHTMLStartTable(char* Buf) {
	return sprintf(Buf, "<table width=800><tr><th></th><th>Name</th><th>Size, KB</th><th>Last Modification time</th><tr>\n");
}

inline int WriteHTMLEndTable(char* Buf) {
	return sprintf(Buf, "</table><hr>\n");
}

inline int WriteHTMLEnd(char* Buf) {
	return sprintf(Buf, "</body>\n</html>");
}

int AddRecordFolder(char* Buf, char* szCurDirrectory, FILEINFO* pFindData, char* szImage) {
	DWORD cbBufSize;
	//
	cbBufSize = sprintf(Buf, "<tr><td><img src=\"%s\"></td><td><a href=\""SZ_CONTENT_FOLDER_FILE"?%s%s"SL"*\">%s</a></td><td align=center> %s </td><td align=center> %s </td></tr>\n",
		szImage, szCurDirrectory, pFindData->FI_szFileName, pFindData->FI_szFileName, 
		pFindData->FI_szSize, pFindData->FI_szLastModificationTime);
	cbBufSize = EncryptRussian(Buf, cbBufSize);
	return cbBufSize;
}

int AddRecordFile(char* Buf, char* szCurDirrectory, FILEINFO* pFindData, char* szImage) {
	DWORD cbBufSize;
	//
	cbBufSize = sprintf(Buf, "<tr><td><img src=\"%s\"></td><td>%s</td><td align=center> %s </td><td align=center> %s </td></tr>\n",
		szImage, pFindData->FI_szFileName, pFindData->FI_szSize, pFindData->FI_szLastModificationTime);
	cbBufSize = EncryptRussian(Buf, cbBufSize);
	return cbBufSize;
}

int SendHTMLAnswer(CPSOCKET Socket, char* Body, DWORD cbBodySize) {
	char* Buf;
	int iError;
	Buf = (char*)malloc(cbBodySize + CB_STD_MSG_SIZE);
	int Len = sprintf(Buf, "HTTP/1.1 200 OK\r\nContent-Encoding: identity\r\nContent-Length: %i\r\nContent-Type: text/html; charset=uft-8\r\nConnection: keep-alive\r\n\r\n%s\r\n", 
		cbBodySize, Body);
	iError = send(Socket, Buf, Len, 0);
	free(Buf);
	if (iError == -1) return X_CLIENT_DISCONNECTED;
	return X_NO_ERROR;
}

int SendFile(CPSOCKET Socket, X_GET_CONTEXT* pContext, X_HEADERS_CONTEXT* pHeadersContext) {
	FILE* pFile = 0;
	FILE* pMetaFile = 0;
	FILE* pHeadersFile = 0;
	DWORD cbFileSize = 0;
	DWORD cbHeadersFileSize = 0;
	char FileName[C_MAX_PATH_STRING_LEN];
	//
	char* OutBuf = 0;
	DWORD cbOutBufSize = 0;
	//
	logDBG("Send file name is %s\n", pContext->GC_szFindData);
	sprintf(FileName, "%s%s", pContext->GC_szFindData, SZ_META_FILE_ENDING);
	// The file
	fopen_s(&pFile, pContext->GC_szFindData, "rb");
	if (pFile == 0) {
		SendStdMsg(Socket, NOT_FOUND, 0, false);
		return X_BAD_REQUEST;
	}
	// File with ETag
	fopen_s(&pMetaFile, FileName, "rb");
	if (pMetaFile == 0) {
		fclose(pFile);
		SendStdMsg(Socket, INTERNAL_SERVER_ERROR, 0, false);
		return X_BAD_REQUEST;
	}
	sprintf(FileName, "%s%s", pContext->GC_szFindData, SZ_HEADERS_FILE_ENDING);
	// File with describing headers like Content-Type
	fopen_s(&pHeadersFile, FileName, "rb");
	if (pHeadersFile == 0) {
		fclose(pFile);
		fclose(pMetaFile);
		SendStdMsg(Socket, INTERNAL_SERVER_ERROR, 0, false);
		return X_BAD_REQUEST;
	}

	cpGetFileSize(pFile, &cbFileSize);
	cpGetFileSize(pHeadersFile, &cbHeadersFileSize);

	if (CheckETag(pHeadersContext->HC_sETag, pHeadersContext->HC_cbETagSize, pMetaFile)) {
		OutBuf = (char*)malloc(CB_STD_MSG_SIZE);
		cbOutBufSize = sprintf(OutBuf, "HTTP/1.1 304 Not Modified\r\n");
		cbOutBufSize += GetCurrentDateHTTP(OutBuf + cbOutBufSize);
		if (pHeadersContext->HC_bCloseSession)
			cbOutBufSize += sprintf(OutBuf + cbOutBufSize, "Connection: close\r\n\r\n");
		else
			cbOutBufSize += sprintf(OutBuf + cbOutBufSize, "Connection: keep-alive\r\n\r\n");
		send(Socket, OutBuf, cbOutBufSize, 0);
		free(OutBuf);
		return X_NO_ERROR;
	}

	OutBuf = (char*)malloc(cbHeadersFileSize + cbFileSize + CB_STD_MSG_SIZE);

	cbOutBufSize = sprintf(OutBuf, "HTTP/1.1 200 OK\r\nContent-Length: %i\r\n", cbFileSize);
	if (pHeadersContext->HC_bCloseSession)
		cbOutBufSize += sprintf(OutBuf + cbOutBufSize, "Connection: close\r\n");
	else
		cbOutBufSize += sprintf(OutBuf + cbOutBufSize, "Connection: keep-alive\r\n");
	cbOutBufSize += fread(OutBuf + cbOutBufSize, 1, cbHeadersFileSize, pHeadersFile);
	cbOutBufSize += fread(OutBuf + cbOutBufSize, 1, cbFileSize, pFile);

	OutBuf[cbOutBufSize] = '\r';
	OutBuf[cbOutBufSize + 1] = '\n';
	cbOutBufSize += 2;

	send(Socket, OutBuf, cbOutBufSize, 0);

	fclose(pFile);
	fclose(pMetaFile);
	fclose(pHeadersFile);
	free(OutBuf);
	return X_NO_ERROR;
}

bool CheckETag(char* sETag, DWORD cbETagSize, FILE* pMETAFile) {
	X_META_FILE MetaFile;
	int n;
	//
	n = fread(&MetaFile, sizeof(MetaFile), 1, pMETAFile);
	if(n != 1)
		return false;
	if (cbETagSize != 16 || strncmp(sETag, MetaFile.MF_s16BYTES_ETag, 16))
		return false;
	return true;
}

int EncryptRussian(char* Buf, DWORD cbBufSize) {
#ifdef WIN32
	wchar_t* WideBuf = 0;
	char* BufNew = 0;
	DWORD cbNewBytes = 0;
	DWORD cbReturnBytes = 0;
	//
	WideBuf = (wchar_t*)malloc(sizeof(wchar_t) * cbBufSize + 2);
	BufNew = (char*)malloc(cbBufSize * 2 + 1);
	MultiByteToWideChar(CP_ACP, 0, Buf, cbBufSize, WideBuf, cbBufSize);
	cbNewBytes = WideCharToMultiByte(CP_UTF8, 0, WideBuf, cbBufSize, BufNew, cbBufSize * 2, 0, 0);
	memcpy(Buf, BufNew, cbNewBytes);
	free(BufNew);
	free(WideBuf);
	return cbNewBytes;
#else
	return cbBufSize;
#endif
}

int DecryptRussian(char* Buf, DWORD cbBufSize) { 
	DWORD cbReturnSize = 0;
	char* BufNew = 0;
	//
	BufNew = (char*)malloc(cbBufSize + 1);
	for (DWORD i = 0; i < cbBufSize; ++i) {
		if (Buf[i] != '%') {
			BufNew[cbReturnSize] = Buf[i];
			++cbReturnSize;
			continue;
		}
		BufNew[cbReturnSize] = ((GetHEX(Buf[i + 1])) << 4) + GetHEX(Buf[i + 2]);
		++cbReturnSize;
		i += 2;
	}
#ifdef WIN32
	wchar_t* WideBuf = 0;
	DWORD nWide = 0;
	WideBuf = (wchar_t*)malloc(cbReturnSize * 2 + 2);
	nWide = MultiByteToWideChar(CP_UTF8, 0, BufNew, cbReturnSize, WideBuf, cbReturnSize * 2);
	cbReturnSize = WideCharToMultiByte(CP_ACP, 0, WideBuf, nWide, Buf, cbBufSize, 0, 0);
	free(WideBuf);
#else
	strncpy(Buf, BufNew, cbReturnSize);
#endif
	free(BufNew);
	return cbReturnSize;
}

inline int GetHEX(const char c) {
	if (c >= '0' && c <= '9')
		return (c - '0');
	else {
		if (c >= 'A' && c <= 'F')
			return (c - 'A' + 10);
		else return (c - 'a' + 10);
	}
}
#ifdef WIN32
int CopyFD64Bytes(struct fd_set* pSetDst, struct fd_set* pSetScr) {
	DWORDLONG* pDst, *pScr;
	pDst = (DWORDLONG*)pSetDst->fd_array;
	pScr = (DWORDLONG*)pSetScr->fd_array;
	*(pDst) = *(pScr);
	*(pDst + 1) = *(pScr + 1);
	*(pDst + 2) = *(pScr + 2);
	*(pDst + 3) = *(pScr + 3);
	*(pDst + 4) = *(pScr + 4);
	*(pDst + 5) = *(pScr + 5);
	*(pDst + 6) = *(pScr + 6);
	*(pDst + 7) = *(pScr + 7);
	return 0;
}

#endif
