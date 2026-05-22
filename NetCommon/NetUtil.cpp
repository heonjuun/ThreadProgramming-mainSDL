#include "pch.h"

#include "NetUtil.h"

int SendAll(SOCKET ReceiverSocket, const char* Data, int Size)
{
	int TotalSendDataSize = 0;
	int WantSendDataSize = Size;
	int SentBytes = 0;
	int Count = 0;
	do
	{
		SentBytes = send(ReceiverSocket, Data + TotalSendDataSize, WantSendDataSize - TotalSendDataSize, 0);
		TotalSendDataSize += SentBytes;
		if (SentBytes <= 0)
		{
			return SentBytes;
		}
	} while (TotalSendDataSize < WantSendDataSize);

	return WantSendDataSize;
}

int RecvAll(SOCKET ReceiverSocket, char* OutData, int Size)
{
	int RecvBytes = recv(ReceiverSocket, OutData, Size, MSG_WAITALL);
	return RecvBytes;
}

