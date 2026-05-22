#pragma once
#include "Packet.h"
class S2C_Destroy : public IPacket
{
public:

	SOCKET ClientSocket;

	// Inherited via IPacket
	void Parse(std::string InString) override;

	std::string ToString() override;

};

