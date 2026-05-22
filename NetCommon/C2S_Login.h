#pragma once
#include "Packet.h"

class C2S_Login : public IPacket
{
public:

	std::string UserID;
	std::string HashKey;


	// Inherited via IPacket
	void Parse(std::string InString) override;

	std::string ToString() override;
};

