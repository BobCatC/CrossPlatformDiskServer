all:
	gcc MainLoop.c SocketThread.c CrossPlatformFunctions.c SocketProcessor.c -o CPDS.o -pthread
