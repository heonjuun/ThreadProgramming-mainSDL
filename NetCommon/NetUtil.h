#pragma once

#include "pch.h"

#include "C2S_Login.h"
#include "S2C_Login.h"
#include "C2S_Move.h"
#include "S2C_Move.h"
#include "S2C_Spawn.h"
#include "S2C_Destroy.h"

#include "SessionManager.h"


enum class EPacketType : unsigned short
{
	C2S_Login = 100,
	S2C_Login = 110,
	S2C_Spawn,
	S2C_Destroy,
	C2S_Move,
	S2C_Move,
	ChatPacket,
	Max
};

#pragma pack(push,1)
struct Header
{
	unsigned short PacketSize;
	unsigned short PacketType;

	void MakeHeader(int InPakcetSize, EPacketType InPacketType)
	{
		PacketSize = htons(InPakcetSize);
		PacketType = htons(static_cast<unsigned short>(InPacketType));
	}

	void NetworkToHost()
	{
		PacketSize = ntohs(PacketSize);
		PacketType = ntohs(PacketType);
	}
};
#pragma pack(pop)



constexpr unsigned short HeaderSize = sizeof(Header);

extern int RecvAll(SOCKET ReceiverSocket, char* OutData, int Size);
extern int SendAll(SOCKET ReceiverSocket, const char* InData, int Size);

extern void DisconnectSocket(SOCKET DisconnectedSocket, fd_set* Sockets);
