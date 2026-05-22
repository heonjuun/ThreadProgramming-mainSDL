#pragma once
#include "Packet.h"

class S2C_Login : public IPacket
{
public:

	SOCKET ClientSocketID;
	std::string Message;


	// Inherited via IPacket
	void Parse(std::string InString) override;
	std::string ToString() override;
};

