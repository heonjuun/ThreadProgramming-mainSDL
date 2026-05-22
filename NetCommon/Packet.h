#pragma once
#include "pch.h"

class IPacket
{
public:
	virtual void Parse(std::string InString) = 0;
	virtual std::string ToString() = 0;
	virtual int Length() { return 0; };

	rapidjson::Document JSONDocument;
};

