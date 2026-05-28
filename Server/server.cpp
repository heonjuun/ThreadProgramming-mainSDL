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
	flatbuffers::FlatBufferBuilder SendBuilder;
	auto DestroyData = UserPacket::CreateS2C_Destroy(
		SendBuilder,
		(uint16_t)closesocket
	);
	SendBuilder.Finish(DestroyData);

	//dangling pointer
	Session* FindSession = MySessionManager.GetSession(ClosedSocket);
	MySessionManager.Delete(*FindSession);

	//모든 유저한테 이동 패킷 보내줌
	for (auto Receiver : MySessionManager.SessionList)
	{
		SendAll(Receiver.ClientSocket, SendBuilder);
	}

}

void ProcessPacket(SOCKET ProcessSocket, const char* InBuffer)
{
	auto UserPacketData = UserPacket::GetPacketData(InBuffer);

	switch (UserPacketData->data_type())
	{
	case UserPacket::PacketType_C2S_Login:
	{
		auto LoginData = UserPacketData->data_as_C2S_Login();
		
		//접속 한 유저가 정확한 사람인지 확인
		// AGameModeBase::PreLogin();
		//접속 한 유저 정보 업데이트(Session)
		Session InSession;
		InSession.ClientSocket = ProcessSocket;
		InSession.UserID = LoginData->user_id()->c_str();
		InSession.X = rand() % 640;
		InSession.Y = rand() % 480;
		InSession.R = rand() % 255;
		InSession.G = rand() % 255;
		InSession.B = rand() % 255;

		InSession.Shape = 65 + (rand() % 26);

		MySessionManager.Add(InSession);
		//접속 한 아이한테 확인 패킷(S2C_Login)
		
		flatbuffers::FlatBufferBuilder SendBuilder;

		auto S2C_Login_Data= UserPacket::CreateS2C_Login(
				SendBuilder,
				(uint64_t)ProcessSocket,
				SendBuilder.CreateString("Welcome")
		);
		auto UserPacketData = UserPacket::CreatePacketData(
				SendBuilder,
				UserPacket::PacketType_S2C_Login,
				S2C_Login_Data.Union()
		);
		SendBuilder.Finish(UserPacketData);
		SendAll(ProcessSocket,SendBuilder);
	
		//접속한 모든 유저한테 현재 모든 유저의 정보를 보내준다.
		for (auto Item : MySessionManager.SessionList)
		{
			flatbuffers::FlatBufferBuilder LoginSendBuilder;
			UserPacket::FVector2D Position(Item.X, Item.Y);
			UserPacket::FColor Color(Item.R, Item.G, Item.B);

			auto SpawnData = UserPacket::CreateS2C_Spawn(
				LoginSendBuilder,
				(uint16_t)Item.ClientSocket,
				&Position,
				&Color,
				Item.Shape
			);
			auto UserSpawnPacketData = UserPacket::CreatePacketData(
				LoginSendBuilder,
				UserPacket::PacketType_S2C_Spawn,
				SpawnData.Union()
			);
			LoginSendBuilder.Finish(UserSpawnPacketData);

			for (auto Receiver : MySessionManager.SessionList)
			{
				int SentBytes = SendAll(Receiver.ClientSocket, LoginSendBuilder);
				if (SentBytes <= 0)
					std::cout << "header send fail." << endl;				
			}
		}
	}
	break;

	case UserPacket::PacketType_C2S_Move:
	{
		flatbuffers::FlatBufferBuilder SendBuilder;
		auto MovePacket = UserPacketData->data_as_C2S_Move();

		Session* FindSession = MySessionManager.GetSession((SOCKET)MovePacket->client_socket_id());
		switch (MovePacket->direction())
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
		UserPacket::FVector2D Position(FindSession->X, FindSession->Y);
		auto S2C_MoveData = UserPacket::CreateS2C_Move(
			SendBuilder,
			(uint16_t)FindSession->ClientSocket,
			&Position
		);
		std::cout << FindSession->ClientSocket << std::endl;
		auto MoveData = UserPacket::CreatePacketData(
			SendBuilder,
			UserPacket::PacketType_S2C_Move,
			S2C_MoveData.Union()
		);

		SendBuilder.Finish(MoveData);
		//모든 유저한테 이동 패킷 보내줌
		for (auto Receiver : MySessionManager.SessionList)
		{
			//header
			int SentBytes = SendAll(Receiver.ClientSocket, SendBuilder);
			if (SentBytes <= 0)
				std::cout << "send fail." << endl;
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
					flatbuffers::FlatBufferBuilder RecvBuilder;
					

					//Data Receive
					int RecvBytes = RecvAll(ReadSockets.fd_array[i], Buffer);
					if (RecvBytes <= 0)
					{
						cout << "data recv fail " << endl;
						DisconnectSocket(ReadSockets.fd_array[i], &ReadSockets);
						continue;
					}
					else
					{
						ProcessPacket(ReadSockets.fd_array[i], Buffer);
					}
				}
			}
		}
	}






	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}