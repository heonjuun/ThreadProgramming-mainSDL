#pragma once
#include "Packet.h"
class ChatPacket : public IPacket
{
public:
	std::string UserID;
	std::string Message;
	int Gold;

	// Inherited via IPacket
	void Parse(std::string InString) override;

	std::string ToString() override;

	int Length() override;

};

