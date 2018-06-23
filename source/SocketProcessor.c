//////////////////////////////////////////////////////
//				  SocketProcessor.c					//
//			  The most simple functions				//
//				  just with socket					//
//											   2017 //
//											  M3103 //
//										   Pakhomov	//
//							   Aleksandr Sergeevich	//
//////////////////////////////////////////////////////

#include"../include/SocketProcessorHeader.h"

int PrepareSocketToAccept(CPSOCKET Socket, short iAf, u_short iPort, int Backlog) {
	SOCKADDR_IN sin;
	//
	if (Socket == SOCKET_ERROR) {
		return X_SOCKET_CREATION_ERROR;
	}
	
	ZeroMemory(&sin, sizeof(sin));
	sin.sin_family = iAf;
	sin.sin_port = htons(iPort);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(Socket, (SOCKADDR*)&sin, sizeof(sin)) == SOCKET_ERROR) {
		printf("Error code is %i\n", errno);
		PAUSE;
		return X_SOCKET_BIND_ERROR;
	}
	if (listen(Socket, Backlog) == SOCKET_ERROR) {
		return X_SOCKET_LISTEN_ERROR;
	}
	
	return X_NO_ERROR;
}

int SendCloseMessage(CPSOCKET Socket) {
	SendStdMsg(Socket, INTERNAL_SERVER_ERROR, 0, true);
	return X_NO_ERROR;
}

int SendStdMsg(CPSOCKET Socket, DWORD dwCode, char* szSpecialHeaders, bool bCloseSession) {
	char* Buf = (char*)malloc(CB_STD_MSG_SIZE);
	DWORD cbBufSize = 0;
	log("Request ended with %i code\n", dwCode);
	//
	switch (dwCode) {
	// 4xx
	case BAD_REQUEST:					// 400
	{
		CreateAndSendStdMsg(Socket, Buf, "HTTP/1.1 400 Bad Request\r\nDate: ",
			"<h1>BAD REQUEST</h1>", szSpecialHeaders, bCloseSession);
		break;
	}
	case NOT_FOUND:						// 404
	{
		CreateAndSendStdMsg(Socket, Buf, "HTTP/1.1 404 Not found\r\n",
			"<h1>ERROR 404 NOT FOUND</h1>", szSpecialHeaders, bCloseSession);
		break;
	}
	case METHOD_NOT_ALLOWED:			// 405
	{
		CreateAndSendStdMsg(Socket, Buf, "HTTP/1.1 405 Not found\r\n",
			"<h1>Method not allowed</h1>", szSpecialHeaders, bCloseSession);
		break;
	}
	case PAYLOAD_TOO_LARGE:				// 413
	{
		CreateAndSendStdMsg(Socket, Buf, "HTTP/1.1 413 Payload Too Large\r\n",
			"<h1>Payload Too Large</h1>", szSpecialHeaders, bCloseSession);
		break;
	}
	// 5xx
	case INTERNAL_SERVER_ERROR:			// 500
	{
		CreateAndSendStdMsg(Socket, Buf, "HTTP/1.1 500 Internal Server Error\r\n", 
			"<h1>Internal server error</h1>", szSpecialHeaders, bCloseSession);
		break;
	}
	case NOT_IMPLEMENTED:				// 501
	{
		CreateAndSendStdMsg(Socket, Buf, "HTTP/1.1 501 Not Implemented\r\n", 
			"<h1>Method not implemented</h1>", szSpecialHeaders, bCloseSession);
		break;
	}
	case HTTP_VERSION_NOT_SUPPORTED:	// 505
	{
		CreateAndSendStdMsg(Socket, Buf, "HTTP/1.1 505 HTTP Version Not Supported\r\n", 
			"HTTP Version Not Supported", szSpecialHeaders, bCloseSession);
		break;
	}

	}

	free(Buf);
	return X_NO_ERROR;
}

int GetCurrentDateHTTP(char* Buf) {
	time_t CurTime;
	struct tm* pTime;
	//
	time(&CurTime);
	pTime = gmtime(&CurTime);
	return strftime(Buf, 80, "DATE: %a, %d %b %Y %H:%M:%S GMT\r\n", pTime);
}

int CreateAndSendStdMsg(CPSOCKET Socket, char* Buf, char* szStartingLine, char* szHTMLString, char* szSpecialHeaders, bool bCloseConnection) {
	DWORD cbBufSize = 0;
	//
	cbBufSize += sprintf(Buf, "%s", szStartingLine);
	cbBufSize += GetCurrentDateHTTP(Buf + cbBufSize);
	if (szSpecialHeaders != 0)
		cbBufSize += sprintf(Buf + cbBufSize, "%s", szSpecialHeaders);
	cbBufSize += EndMessage(Buf + cbBufSize, szHTMLString, bCloseConnection);
	return send(Socket, Buf, cbBufSize, 0);
}

int EndMessage(char* Buf, char* szHTMLString, bool bCloseConnection) {
	if (bCloseConnection) return sprintf(Buf, "Connection: close\r\n\r\n<html><body><h1>%s</h1></body></html>\r\n", szHTMLString);
	else return sprintf(Buf, "Connection: keep-alive\r\n\r\n<html><body><h1>%s</h1></body></html>\r\n", szHTMLString);
}
