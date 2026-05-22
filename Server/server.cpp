#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "NetUtil.h"

#include <winsock2.h>
#include <iostream>

#include "SessionManager.h"


#pragma comment(lib, "ws2_32")
#pragma comment(lib, "NetCommon")

using namespace std;

char Buffer[1024] = { 0, };

SessionManager MySessionManager;

void DisconnectSocket(SOCKET DisconnectedSocket, fd_set* Sockets)
{
	SOCKET ClosedSocket = DisconnectedSocket;

	SOCKADDR_IN ClosedSockAddr;
	memset(&ClosedSockAddr, 0, sizeof(ClosedSockAddr));
	int ClosedSockAddrLength = sizeof(ClosedSockAddr);

	getpeername(ClosedSocket, (SOCKADDR*)&ClosedSockAddr, &ClosedSockAddrLength);

	cout << "disconnect : " << ClosedSocket << endl;

	cout << "disconnect : " << inet_ntoa(ClosedSockAddr.sin_addr) << endl;

	FD_CLR(ClosedSocket, Sockets);
	closesocket(ClosedSocket);

	S2C_Destroy DestroyPacket;

	//dangling pointer
	Session* FindSession = MySessionManager.GetSession(ClosedSocket);
	DestroyPacket.ClientSocket = FindSession->ClientSocket;

	MySessionManager.Delete(*FindSession);

	Header DestroyHeader;
	DestroyHeader.MakeHeader((int)DestroyPacket.ToString().length(), EPacketType::S2C_Destroy);

	//모든 유저한테 이동 패킷 보내줌
	for (auto Receiver : MySessionManager.SessionList)
	{
		//header
		int SentBytes = SendAll(Receiver.ClientSocket, (char*)&DestroyHeader, HeaderSize);
		if (SentBytes <= 0)
		{
			std::cout << "header send fail." << endl;
		}

		//Data
		SentBytes = SendAll(Receiver.ClientSocket, DestroyPacket.ToString().c_str(), (int)(DestroyPacket.ToString().length()));
		if (SentBytes <= 0)
		{
			std::cout << "Data send fail." << endl;
		}
	}

}

void ProcessPacket(SOCKET ProcessSocket, const char* InBuffer, const Header& InHeader)
{
	switch ((EPacketType)InHeader.PacketType)
	{
	case EPacketType::C2S_Login:
	{
		C2S_Login LoginPacket;
		LoginPacket.Parse(InBuffer);
		//접속 한 유저가 정확한 사람인지 확인
		// AGameModeBase::PreLogin();
		//접속 한 유저 정보 업데이트(Session)
		Session InSession;
		InSession.ClientSocket = ProcessSocket;
		InSession.UserID = LoginPacket.UserID;
		InSession.X = rand() % 24 + 1; // 1 ~ 25;
		InSession.Y = rand() % 24 + 1; // 1 ~ 25;
		InSession.R = rand() % 255;
		InSession.G = rand() % 255;
		InSession.B = rand() % 255;

		InSession.Shape = 65 + (rand() % 26);

		MySessionManager.Add(InSession);
		//접속 한 아이한테 확인 패킷(S2C_Login)

		S2C_Login Data;
		Data.ClientSocketID = ProcessSocket;
		Data.Message = "Welcome.";

		//header
		Header DataHeader;
		DataHeader.MakeHeader((int)(Data.ToString().length()), EPacketType::S2C_Login);
		int SentBytes = SendAll(ProcessSocket, (char*)&DataHeader, HeaderSize);
		if (SentBytes <= 0)
		{
			std::cout << "header send fail." << endl;
		}

		//Data
		SentBytes = SendAll(ProcessSocket, Data.ToString().c_str(), (int)(Data.ToString().length()));
		if (SentBytes <= 0)
		{
			std::cout << "Data send fail." << endl;
		}

		//접속한 모든 유저한테 현재 모든 유저의 정보를 보내준다.
		for (auto Item : MySessionManager.SessionList)
		{
			S2C_Spawn SpawnData;
			SpawnData.ClientSocket = Item.ClientSocket;
			SpawnData.Shape = Item.Shape;
			SpawnData.X = Item.X;
			SpawnData.Y = Item.Y;
			SpawnData.R = Item.R;
			SpawnData.G = Item.G;
			SpawnData.B = Item.B;

			Header SpawnHeader;
			SpawnHeader.MakeHeader((int)SpawnData.ToString().length(), EPacketType::S2C_Spawn);
			for (auto Receiver : MySessionManager.SessionList)
			{
				//header
				int SentBytes = SendAll(Receiver.ClientSocket, (char*)&SpawnHeader, HeaderSize);
				if (SentBytes <= 0)
				{
					std::cout << "header send fail." << endl;
				}

				//Data
				SentBytes = SendAll(Receiver.ClientSocket, SpawnData.ToString().c_str(), (int)(SpawnData.ToString().length()));
				if (SentBytes <= 0)
				{
					std::cout << "Data send fail." << endl;
				}
			}
		}
	}
	break;

	case EPacketType::C2S_Move:
	{
		C2S_Move MovePacket;
		MovePacket.Parse(InBuffer);
		Session* FindSession = MySessionManager.GetSession(MovePacket.ClientSocket);;
		switch (MovePacket.Direction)
		{
		case 'W':
		case 'w':
			FindSession->Y--;
			break;
		case 'S':
		case 's':
			FindSession->Y++;
			break;
		case 'A':
		case 'a':
			FindSession->X--;
			break;
		case 'D':
		case 'd':
			FindSession->X++;
			break;
		}

		S2C_Move MoveData;
		MoveData.ClientSocket = FindSession->ClientSocket;
		MoveData.X = FindSession->X;
		MoveData.Y = FindSession->Y;

		Header MoveHeader;
		MoveHeader.MakeHeader((int)MoveData.ToString().length(), EPacketType::S2C_Move);

		//모든 유저한테 이동 패킷 보내줌
		for (auto Receiver : MySessionManager.SessionList)
		{
			//header
			int SentBytes = SendAll(Receiver.ClientSocket, (char*)&MoveHeader, HeaderSize);
			if (SentBytes <= 0)
			{
				std::cout << "header send fail." << endl;
			}

			//Data
			SentBytes = SendAll(Receiver.ClientSocket, MoveData.ToString().c_str(), (int)(MoveData.ToString().length()));
			if (SentBytes <= 0)
			{
				std::cout << "Data send fail." << endl;
			}
		}
	}
	break;
	}


}

//blocking, synchrous, multiplexing(polling)
int main()
{
	srand((unsigned int)time(nullptr));

	cout << "server start" << endl;

	WSAData wsaData;

	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET ListenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	SOCKADDR_IN ListenSockAddr;
	memset(&ListenSockAddr, 0, sizeof(ListenSockAddr));
	ListenSockAddr.sin_family = AF_INET;
	ListenSockAddr.sin_addr.s_addr = INADDR_ANY;
	ListenSockAddr.sin_port = htons(35000);

	//already use port 이미 포트 사용중
	::bind(ListenSocket, (SOCKADDR*)&ListenSockAddr, sizeof(ListenSockAddr));

	listen(ListenSocket, SOMAXCONN);



	//blocking, synchronous(TimeOut)
	TIMEVAL TimeOut;
	TimeOut.tv_sec = 0;
	TimeOut.tv_usec = 500000;

	fd_set ReadSockets;
	fd_set CopyReadSockets;

	FD_ZERO(&ReadSockets);
	FD_SET(ListenSocket, &ReadSockets);

	while (true)
	{
		CopyReadSockets = ReadSockets;

		//0.5초씩 blocking
		int ChangeCount = select(0, &CopyReadSockets, 0, 0, &TimeOut);

		if (ChangeCount <= 0)
		{
			//Server Work
			//0.5초한번 서버 작업을 하는거
			continue;
		}

		//몬가 자료 있다.
		for (int i = 0; i < (int)ReadSockets.fd_count; ++i)
		{
			if (FD_ISSET(ReadSockets.fd_array[i], &CopyReadSockets))
			{
				if (ReadSockets.fd_array[i] == ListenSocket)
				{
					//connect process
					SOCKADDR_IN ClientSockAddr;
					memset(&ClientSockAddr, 0, sizeof(ClientSockAddr));
					int ClientSockSockLength = sizeof(ClientSockAddr);

					//blocking, synchronous
					SOCKET ClientSocket = accept(ListenSocket, (SOCKADDR*)&ClientSockAddr, &ClientSockSockLength);

					cout << "connect client " << inet_ntoa(ClientSockAddr.sin_addr) << endl;

					FD_SET(ClientSocket, &ReadSockets);
				}
				else
				{
					//Data Receive

					//header
					Header DataHeader;
					int RecvBytes = RecvAll(ReadSockets.fd_array[i], (char*)&DataHeader, HeaderSize);
					if (RecvBytes <= 0)
					{
						cout << "header recv fail " << endl;
						DisconnectSocket(ReadSockets.fd_array[i], &ReadSockets);
						continue;
					}

					DataHeader.NetworkToHost();

					memset(Buffer, 0, sizeof(Buffer));
					//data JSON
					RecvBytes = RecvAll(ReadSockets.fd_array[i], Buffer, DataHeader.PacketSize);
					if (RecvBytes <= 0)
					{
						cout << "data recv fail " << endl;
						DisconnectSocket(ReadSockets.fd_array[i], &ReadSockets);
						continue;
					}
					else
					{
						ProcessPacket(ReadSockets.fd_array[i], Buffer, DataHeader);
					}
				}
			}
		}
	}






	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}