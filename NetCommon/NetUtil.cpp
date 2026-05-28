#include "pch.h"

#include "NetUtil.h"
#include<iostream>


int SendAll(SOCKET TargetSocket, const flatbuffers::FlatBufferBuilder& Builder)
{
	int SentBytes = 0;
	int PacketSize = Builder.GetSize();
	PacketSize = htons(PacketSize);

	//send(TargetSocket, (char*)PacketSize, 2, 0);

	//Header 보내기
	if (SentBytes=SendAll(TargetSocket, (char*)&PacketSize, 2) <= 0)
	{
		std::cout << "Header Send Error" << std::endl;
	}

	//DAta 보내기
	if (SentBytes=SendAll(TargetSocket, (char*)Builder.GetBufferPointer(), Builder.GetSize()) <= 0)
	{
		std::cout << "Data Send Error" << std::endl;
	}
	return SentBytes;
}


int RecvAll(SOCKET SourceSocket, char* OutData)
{
	int PacketSize = 0;
	int RecvLength = 0;

	//header, size
	RecvLength=::recv(SourceSocket, (char*)&PacketSize, 2, MSG_WAITALL);
	if (RecvLength <= 0)
	{
		return RecvLength;
	}
	PacketSize = ntohs(PacketSize);

	RecvLength = ::recv(SourceSocket, OutData, PacketSize, MSG_WAITALL);
	if (RecvLength <= 0)
	{
		return RecvLength;
	}
	return RecvLength;
	
}

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

//int RecvAll(SOCKET SourceSocket, char* OutData, int Size)
//{
//	int RecvBytes = recv(SourceSocket, OutData, Size, MSG_WAITALL);
//	return RecvBytes;
//}

