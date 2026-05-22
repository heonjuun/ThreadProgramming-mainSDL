#pragma once
#include "Packet.h"
class S2C_Spawn : public IPacket
{
public:

	SOCKET ClientSocket;
	int X;
	int Y;
	char Shape = ' ';
	int R;
	int G;
	int B;
	// Inherited via IPacket
	void Parse(std::string InString) override;
	std::string ToString() override;
};

