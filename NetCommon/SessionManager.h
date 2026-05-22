#pragma once
#include "pch.h"

#include <vector>

struct Session
{
	SOCKET ClientSocket;
	std::string UserID;

	int X;
	int Y;
	int R;
	int G;
	int B;
	char Shape = ' ';

	bool operator==(const Session& RHS)
	{
		return this->ClientSocket == RHS.ClientSocket;
	}
};



class SessionManager
{
public:
	void Add(Session InSession);
	void Delete(Session InSession);

	Session* GetSession(int Index);
	Session* GetSession(const SOCKET& InClentSocket);
	Session* GetSession(const Session& InSession);


//protected:
	std::vector<Session> SessionList;
};


