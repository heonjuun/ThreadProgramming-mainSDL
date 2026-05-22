#include "pch.h"
#include "S2C_Spawn.h"

void S2C_Spawn::Parse(std::string InString)
{
    JSONDocument.Parse(InString.c_str());

    ClientSocket = JSONDocument["ClientSocket"].GetInt();
    X = JSONDocument["X"].GetInt();
    Y = JSONDocument["Y"].GetInt();
    R = JSONDocument["R"].GetInt();
    G = JSONDocument["G"].GetInt();
    B = JSONDocument["B"].GetInt();
    Shape = JSONDocument["Shape"].GetInt();

}

std::string S2C_Spawn::ToString()
{
    JSONDocument.SetObject();
    JSONDocument.AddMember("ClientSocket", ClientSocket, JSONDocument.GetAllocator());
    JSONDocument.AddMember("X", X, JSONDocument.GetAllocator());
    JSONDocument.AddMember("Y", Y, JSONDocument.GetAllocator());
    JSONDocument.AddMember("R", R, JSONDocument.GetAllocator());
    JSONDocument.AddMember("G", G, JSONDocument.GetAllocator());
    JSONDocument.AddMember("B", B, JSONDocument.GetAllocator());
    JSONDocument.AddMember("Shape", Shape, JSONDocument.GetAllocator());



    rapidjson::StringBuffer Buffer;
    rapidjson::Writer<rapidjson::StringBuffer> Writer(Buffer);
    JSONDocument.Accept(Writer);

    return Buffer.GetString();
}
