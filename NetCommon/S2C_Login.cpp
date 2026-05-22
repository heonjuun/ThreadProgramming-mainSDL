#include "pch.h"
#include "S2C_Login.h"

void S2C_Login::Parse(std::string InString)
{
    JSONDocument.Parse(InString.c_str());

    Message = JSONDocument["Message"].GetString();
    ClientSocketID = JSONDocument["ClientSocketID"].GetInt();
}

std::string S2C_Login::ToString()
{
    JSONDocument.SetObject();
    JSONDocument.AddMember("Message", Message, JSONDocument.GetAllocator());
    JSONDocument.AddMember("ClientSocketID", ClientSocketID, JSONDocument.GetAllocator());


    rapidjson::StringBuffer Buffer;
    rapidjson::Writer<rapidjson::StringBuffer> Writer(Buffer);
    JSONDocument.Accept(Writer);

    return Buffer.GetString();
}