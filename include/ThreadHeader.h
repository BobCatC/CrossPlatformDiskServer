#pragma once
//////////////////////////////////////////////////////
//				SocketThreadHeader.h				//
//				   The main file					//
//				 Processes requests					//
//											   2017 //
//											  M3103 //
//										   Pakhomov	//
//							   Aleksandr Sergeevich	//
//////////////////////////////////////////////////////

#include"AllHeader.h"
#include"SocketProcessorHeader.h"
//
#define SZ_START_FOLDER				(""SL)				// The root folder of FolderContent request, it can be any folder
#define SZ_SERVER_FILES_DIRECTORY	"ServerFiles"			// The dirrectory with files like favicon.ico
#define SZ_META_FILE_ENDING			"META.bin"				// Ending of file with ETag 
#define SZ_HEADERS_FILE_ENDING		"HEADERS.txt"			// Ending of file with Headers like Content-type, Content-Encoding..
#define SZ_CONTENT_FOLDER_FILE		"FolderContent"			// Name of the virtual file
#define CB_STD_HTML_SIZE			(1024 * 1024)			// Std size of HTML answer 1MB
//
typedef struct tagX_PARSING_CONTEXT {
	char* Buf;
	DWORD cbBufSize;
	DWORD iPos;
}X_PARSING_CONTEXT;

typedef enum tagX_HTTP_VERSION { 
	HTTP_VERSION_UNKNOWN,
	HTTP_VERSION_0D9,
	HTTP_VERSION_1D0,
	HTTP_VERSION_1D1,
	HTTP_VERSION_2D0
}X_HTTP_VERSION;

typedef struct tagX_PARSED_MSG {
	char* PM_StartingLine;
	DWORD PM_cbStartingLineSize;
	char* PM_Headers[N_MAX_HEADERS];
	DWORD PM_cbHeadersSizes[N_MAX_HEADERS];
	char* PM_MessageBody;
	DWORD PM_cbMessageBodySize;
	DWORD PM_nHeaders;

	X_HTTP_VERSION PM_HttpVersion;
}X_PARSED_MSG;

//

typedef enum tagX_METHOD {
	HTTP_METHOD_UNKOWN,
	HTTP_METHOD_GET,
	HTTP_METHOD_HEAD,
	HTTP_METHOD_OPTIONS,
	HTTP_METHOD_CONNECT,
	HTTP_METHOD_TRACE,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE
}X_METHOD;

typedef struct tagX_METHOD_CONTEXT {
	X_METHOD MC_iMethod;
	char* MC_URI;
	DWORD MC_cbURISize;
}X_METHOD_CONTEXT;
//
typedef enum tagX_GET_TYPE {
	GET_FOLDER_CONTEXT,							// Request for virtual file FolderContent
	GET_FILE									// Request for real file e.g. favicon.ico
}X_GET_TYPE;

typedef struct tagX_GET_CONTEXT {
	X_GET_TYPE GC_Type;
	char* GC_szFindData;						// String for func FindFirstFile e.g. "C:\\Program files\\*"
	DWORD GC_cbFindDataSize;
	char GC_szMotherFolder[C_MAX_PATH_STRING_LEN];			// e.g. C:\\*
	char GC_szCurrentDirrectory[C_MAX_PATH_STRING_LEN];		// String, where we are looking for files e.g. "C:\\Program files\\"		
}X_GET_CONTEXT;
//
typedef struct tagX_HEADERS_CONTEXT {
	bool HC_bCloseSession;
	char* HC_sETag;
	DWORD HC_cbETagSize;

}X_HEADERS_CONTEXT;
//
typedef struct tagX_META_FILE {
	//16 bytes
	char MF_s16BYTES_ETag[16];
}X_META_FILE;

//
RETURN_THREAD_TYPE SocketThread(void* pPar);

int EndWorkingThread(LOOP_THREAD_COM* pCom, bool bSendCloseMessage, DWORD* pnWorking, bool* pbFree);

int ProcessRequest(CPSOCKET Socket, char* InBuf, DWORD cbMsgSize, bool* pbEndWork);

int ParseMsg(X_PARSING_CONTEXT* pContext, X_PARSED_MSG* pParsedMsg);

int GetHTTPVersion(X_PARSED_MSG* pParsedMsg);

int IdentifyMethod(X_PARSED_MSG* pParsedMsg, X_METHOD_CONTEXT* pMethodContext);

int IdentifyHeaders(X_PARSED_MSG* pParsedMsg, X_HEADERS_CONTEXT* pHeadersContext);

int ProcessGET(CPSOCKET Socket, X_METHOD_CONTEXT* pMethodContext, X_HEADERS_CONTEXT* pHeadersContext);

int CreateHTMLofFolderContent(CPSOCKET Socket, X_GET_CONTEXT* pContext);

int WriteHTMLStart(char* Buf, char* szDirectoryName);

int WriteHTMLRefsUpper(char* Buf, char* szMotherDirectory);

inline int WriteHTMLEmpty(char* Buf);

inline int WriteHTMLNotAvailable(char* Buf);

inline int WriteHTMLStartTable(char* Buf);

inline int WriteHTMLEndTable(char* Buf);

inline int WriteHTMLEnd(char* Buf);

int AddRecordFolder(char* Buf, char* szAbsoluteFolderName, FILEINFO* pFindData, char* szImage);

int AddRecordFile(char* Buf, char* szAbsoluteFolderName, FILEINFO* pFindData, char* szImage);

int SendHTMLAnswer(CPSOCKET Socket, char* Body, DWORD cbBodySize);

int SendFile(CPSOCKET Socket, X_GET_CONTEXT* pContext, X_HEADERS_CONTEXT* pHeadersContext);

bool CheckETag(char* sETag, DWORD cbETagSize, FILE* pMETAFile);

int EncryptRussian(char* Buf, DWORD cbBufSize);

int DecryptRussian(char* Buf, DWORD cbBufSize);

inline int GetHEX(const char c);