#pragma once
//////////////////////////////////////////////////////
//			   SocketProcessorHeader.h				//
//			  The most simple functions				//
//				  just with socket					//
//											   2017 //
//											  M3103 //
//										   Pakhomov	//
//							   Aleksandr Sergeevich	//
//////////////////////////////////////////////////////

#include"AllHeader.h"
//
#define CB_STD_MSG_SIZE						2048
// 4xx
#define BAD_REQUEST							400
#define NOT_FOUND							404
#define METHOD_NOT_ALLOWED					405
#define PAYLOAD_TOO_LARGE					413
// 5xx
#define INTERNAL_SERVER_ERROR				500
#define NOT_IMPLEMENTED						501
#define HTTP_VERSION_NOT_SUPPORTED			505
//
int PrepareSocketToAccept(CPSOCKET Socket, short iAf, u_short iPort, int Backlog);

int SendCloseMessage(CPSOCKET Socket);

int SendStdMsg(CPSOCKET Socket, DWORD dwCode, char* szSpecialHeaders, bool bCloseSession);

size_t GetCurrentDateHTTP(char* Buf);

size_t CreateAndSendStdMsg(CPSOCKET Socket, char* Buf, char* szStartingLine, char* szHTMLString, char* szSpecialHeaders, bool bCloseConnection);

int EndMessage(char* Buf, char* szHTMLString, bool bCloseConnection);
//
