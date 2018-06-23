#include"../include/AllHeader.h"
#ifdef WIN32
int cpSetSocketNonBlock(CPSOCKET Socket) {
	DWORD dw = true;
	ioctlsocket(Socket, FIONBIO, &dw);
	return 0;
}

int cpclosesocket_X(CPSOCKET* pSocket) {
	if (*pSocket != 0) {
		if (*pSocket != SOCKET_ERROR) {
			closesocket(*pSocket);
		}
		*pSocket = 0;
	}
	return 0;
}

int cpTerminateThread_X(THREAD* phThread, DWORD dwExitCode) {
	if (*phThread != 0) {
		TerminateThread(*phThread, dwExitCode);
		*phThread = 0;
	}
	return 0;
}

THREAD cpCreateThread(void* lpStartAddress, void* lpParameter) {
	return CreateThread(0, 0, lpStartAddress, lpParameter, 0, 0);
}

MUTEX cpCreateMutex(bool bInit) {
	return CreateMutexW(0, bInit, 0);
}

int cpWaitMutex(MUTEX Mutex) {
	return (WaitForSingleObject(Mutex, INFINITE) == WAIT_OBJECT_0);
}

int cpFreeMutex(MUTEX Mutex) {
	return ReleaseMutex(Mutex);
}

int cpDeleteMutex(MUTEX Mutex) {
	return CloseHandle(Mutex);
}

FINDFILE cpFindFirstFile(char* szFileName, FILEINFO* pFileInfo) {
	FINDFILE hFindFile;
	DWORDLONG cbSize;
	logDBG("FFF ASK: %s\n", szFileName);
	WIN32_FIND_DATAA FindData;
	SYSTEMTIME stUTC, stLocal;
	hFindFile = FindFirstFileA(szFileName, &FindData);
	if (hFindFile == INVALID_HANDLE_VALUE)
		return hFindFile;
	strcpy(pFileInfo->FI_szFileName, FindData.cFileName);

	FileTimeToSystemTime(&FindData.ftLastAccessTime, &stUTC);
	SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
	sprintf(pFileInfo->FI_szLastModificationTime, "%02d.%02d.%d  %02d:%02d",
		stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute);
	cbSize = ((DWORDLONG)FindData.nFileSizeLow >> 10) + ((DWORDLONG)FindData.nFileSizeHigh << 22);
	_i64toa(cbSize, pFileInfo->FI_szSize, 10);
	if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		pFileInfo->FI_bFolder = true;
		pFileInfo->FI_szSize[0] = '-';
		pFileInfo->FI_szSize[1] = 0;
	}
	else
		pFileInfo->FI_bFolder = false;
	return hFindFile;
}

bool cpFindNextFile(char* szPath, FINDFILE hFindFile, FILEINFO* pFileInfo) {
	DWORDLONG cbSize;
	WIN32_FIND_DATAA FindData;
	if (!FindNextFileA(hFindFile, &FindData))
		return false;
	SYSTEMTIME stUTC, stLocal;
	strcpy(pFileInfo->FI_szFileName, FindData.cFileName);

	FileTimeToSystemTime(&FindData.ftLastAccessTime, &stUTC);
	SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
	sprintf(pFileInfo->FI_szLastModificationTime, "%02d.%02d.%d  %02d:%02d",
		stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute);
	cbSize = ((DWORDLONG)FindData.nFileSizeLow >> 10) + ((DWORDLONG)FindData.nFileSizeHigh << 22);
	_i64toa(cbSize, pFileInfo->FI_szSize, 10);
	if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		pFileInfo->FI_bFolder = true;
		pFileInfo->FI_szSize[0] = '-';
		pFileInfo->FI_szSize[1] = 0;
	}
	else
		pFileInfo->FI_bFolder = false;
	return true;
}

int cpCloseFindFile(FINDFILE hFindFile) {
	return FindClose(hFindFile);
}

#else

int cpSetSocketNonBlock(CPSOCKET Socket) {
	fcntl(Socket, F_GETFL, 0);
	return 0;
}

int cpclosesocket_X(CPSOCKET* pSocket) {
	if (*pSocket != 0) {
		if (*pSocket != SOCKET_ERROR) {
			close(*pSocket);
		}
		*pSocket = 0;
	}
	return 0;
}

int cpTerminateThread_X(THREAD* phThread, DWORD dwExitCode) {
	if (*phThread != 0) {
		pthread_cancel(*phThread);
		*phThread = 0;
	}
	return 0;
}

THREAD cpCreateThread(void* lpStartAddress, void* lpParameter) {
	THREAD Thread;
	pthread_create(&Thread, 0, lpStartAddress, lpParameter);
	return Thread;
}

MUTEX cpCreateMutex(bool bInit) {
	MUTEX Mutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_t t1 = PTHREAD_MUTEX_INITIALIZER;
	memcpy(Mutex, &t1, sizeof(t1));
	logDBG("pthread_mutex_init returned %i\n", pthread_mutex_init(Mutex, 0));
	logDBG("Mutex address is %p\n", Mutex);
	return Mutex;
}

int cpWaitMutex(MUTEX Mutex) {
	int iError = pthread_mutex_lock(Mutex);
	logDBG("ask to lock mutex %i\n", iError);
	return (iError == 0);
}

int cpFreeMutex(MUTEX Mutex) {
	int iError = pthread_mutex_unlock(Mutex);
	logDBG("Mutex umlock %i\n", iError);
	return (iError == 0);
}

int cpDeleteMutex(MUTEX Mutex) {
	logDBG("Mutex free %p\n", Mutex);
	int iError = pthread_mutex_destroy(Mutex);
	free(Mutex);
	return (iError == 0);
}

FINDFILE cpFindFirstFile(char* szFileName, FILEINFO* pFileInfo) {
	FINDFILE hFindFile;
	logDBG("FFF ASK: %s\n", szFileName);
	struct dirent* pDirent;
	struct stat FileStat;
	struct tm* ptime;
	DWORDLONG cbFileNameSize = strlen(szFileName);
	char* Buf = (char*)malloc(cbFileNameSize + 1);
	strcpy(Buf, szFileName);
	hFindFile = opendir(Buf);
	free(Buf);
	logDBG("\t%X\n", hFindFile);
	if (!hFindFile)
		return hFindFile;
	pDirent = readdir(hFindFile);
	strcpy(pFileInfo->FI_szFileName, pDirent->d_name);
	Buf = (char*)malloc(cbFileNameSize + strlen(pDirent->d_name) + 1);
	sprintf(Buf, "%s%s", szFileName, pDirent->d_name);
	if (stat(Buf, &FileStat) != 0) {
		free(Buf);
		return hFindFile;
	}
	logDBG("\t%s\n", pDirent->d_name);
	ptime = localtime(&FileStat.st_mtime);
	strftime(pFileInfo->FI_szLastModificationTime, 80, "%d.%m.%Y %H:%M", ptime);
	_i64toa(FileStat.st_size, pFileInfo->FI_szSize, 10);

	if ((FileStat.st_mode & S_IFDIR) != 0) {
		pFileInfo->FI_bFolder = true;
		pFileInfo->FI_szSize[0] = '-';
		pFileInfo->FI_szSize[1] = 0;
	}
	else
		pFileInfo->FI_bFolder = false;
	free(Buf);
	return hFindFile;
}

bool cpFindNextFile(char* szPath, FINDFILE hFindFile, FILEINFO* pFileInfo) {
	struct dirent* pDirent;
	struct stat FileStat;
	struct tm* ptime;
	char* Buf;

	if ((pDirent = readdir(hFindFile)) == 0) {
		return false;
	}
	Buf = (char*)malloc(strlen(szPath) + strlen(pDirent->d_name) + 1);
	sprintf(Buf, "%s%s", szPath, pDirent->d_name);
	if (stat(Buf, &FileStat) != 0) {
		free(Buf);
		return false;
	}
	logDBG("\t%s\n", pDirent->d_name);
	strcpy(pFileInfo->FI_szFileName, pDirent->d_name);
	ptime = localtime(&FileStat.st_mtime);
	strftime(pFileInfo->FI_szLastModificationTime, 80, "%d.%m.%Y %H:%M", ptime);
	_i64toa(FileStat.st_size, pFileInfo->FI_szSize, 10);

	if ((FileStat.st_mode & S_IFDIR) != 0) {
		pFileInfo->FI_bFolder = true;
		pFileInfo->FI_szSize[0] = '-';
		pFileInfo->FI_szSize[1] = 0;
	}
	else
		pFileInfo->FI_bFolder = false;
	free(Buf);
	return true;
}

int cpCloseFindFile(FINDFILE hFindFile) {
	closedir(hFindFile);
	return 0;
}

void _i64toa(DWORDLONG i, char* Buf, int SS) {
	sprintf(Buf, "%lli", i);
}

int fopen_s(FILE** ppFile, char* szFileName, char* szMode) {
	*ppFile = fopen(szFileName, szMode);
	return 0;
}
#endif
int cpGetFileSize(FILE* pFile, DWORD* pcbFileSize) {
	fseek(pFile, 0, SEEK_END);
	*pcbFileSize = (DWORD)ftell(pFile);
	rewind(pFile);
	return 0;
}

int free_X(void** pp) {
	if (*pp != 0) {
		free(*pp);
		*pp = 0;
	}
	return 0;
}
