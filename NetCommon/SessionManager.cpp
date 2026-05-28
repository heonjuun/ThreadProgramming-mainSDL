#include "pch.h"
#include "SessionManager.h"
#include <algorithm>

void SessionManager::Add(Session InSession)
{
	//추가 등록 막기
	for (auto Iter = SessionList.begin(); Iter != SessionList.end(); ++Iter)
	{
		if ((*Iter).ClientSocket == InSession.ClientSocket)
		{
			return;
		}
	}

	SessionList.push_back(InSession);
}

//[][][][][][]
void SessionManager::Delete(Session InSession)
{
	SessionList.erase(std::find(SessionList.begin(), SessionList.end(), InSession));

	//for (auto Iter = SessionList.begin(); Iter != SessionList.end(); ++Iter)
	//{
	//	if ((*Iter).ClientSocket == InSession.ClientSocket)
	//	{
	//		SessionList.erase(Iter);
	//		break;
	//	}
	//}
}

Session* SessionManager::GetSession(int Index)
{
	// TODO: insert return statement here
	return &SessionList[Index];
}

Session* SessionManager::GetSession(const SOCKET& InClientSocket)
{
	// TODO: insert return statement here
	for (auto Iter = SessionList.begin(); Iter != SessionList.end(); ++Iter)
	{
		if ((*Iter).ClientSocket == InClientSocket)
		{
			return &(*Iter);
		}
	}

	return nullptr;
}

Session* SessionManager::GetSession(const Session& InSession)
{
	return &(*std::find(SessionList.begin(), SessionList.end(), InSession));
}
