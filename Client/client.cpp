#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "NetUtil.h"

#include <Windows.h>
#include <iostream>
#include <process.h>
#include <conio.h>
#include "SDL.h"
#include <mutex>
#include <queue>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "NetCommon")
#pragma comment(lib, "SDL2")
#pragma comment(lib, "SDL2main")


using namespace std;

char RecvBuffer[65536] = { 0, };

bool IsRecvThreadRunning = true;
bool IsSendThreadRunning = true;

//ActorList
SessionManager MySessionManager;
SOCKET MyClientID;

SDL_Window* MyWindow;
SDL_Renderer* MyRenderer;


std::mutex SessionLock;
std::mutex KeyBufferLock;


void Render();
void ProcessPacket(SOCKET ProcessSocket, const char* InBuffer);
unsigned WINAPI RecvThread(void* Argument);
unsigned WINAPI SendThread(void* Argument);

//queue
std::queue<int> KeyBuffer;
//KeyBuffer -> PacketBuffer

int SDL_main(int Argc, char* Argv[])
{
	//Object 동기화(Lock, Lockfree)
	//GameThread(Render)
	//NetworkThread

	std::cout << "client " << endl;

	WSAData wsaData;

	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SDL_Init(SDL_INIT_EVERYTHING);
	MyWindow = SDL_CreateWindow("SDL", 100, 100, 640, 480, SDL_WINDOW_OPENGL);
	MyRenderer = SDL_CreateRenderer(MyWindow, -1, SDL_RENDERER_ACCELERATED || SDL_RENDERER_PRESENTVSYNC || SDL_RENDERER_TARGETTEXTURE);


	SOCKET ServerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	SOCKADDR_IN ServerSockAddr;
	memset(&ServerSockAddr, 0, sizeof(ServerSockAddr));
	ServerSockAddr.sin_family = AF_INET;
	ServerSockAddr.sin_addr.s_addr = inet_addr(" 192.168.0.194");
	ServerSockAddr.sin_port = htons(35000);

	connect(ServerSocket, (SOCKADDR*)&ServerSockAddr, sizeof(ServerSockAddr));

	std::cout << "client connect" << endl;

	//memory(Data) -> ByteArray(char []) -> Serialize(flatbuffer)
	flatbuffers::FlatBufferBuilder SendBuilder;
	auto C2S_LoginData = UserPacket::CreateC2S_Login(
		SendBuilder,
		SendBuilder.CreateString("ninja"),
		SendBuilder.CreateString("1as3f356dsd6gyhg")
	);

	auto UserPacketData = UserPacket::CreatePacketData(
		SendBuilder,
		UserPacket::PacketType_C2S_Login,
		C2S_LoginData.Union()
	);

	SendBuilder.Finish(UserPacketData);

	SendAll(ServerSocket, SendBuilder);

	HANDLE ThreadHandles[2] = { 0, };

	ThreadHandles[0] = (HANDLE)_beginthreadex(0, 0, RecvThread, &ServerSocket, 0, 0);
	ThreadHandles[1] = (HANDLE)_beginthreadex(0, 0, SendThread, &ServerSocket, 0, 0);


	const Uint8* KeyState = SDL_GetKeyboardState(NULL);

	while (true)
	{
		SDL_Event MyEvent;
		SDL_PollEvent(&MyEvent);
		if (MyEvent.type == SDL_QUIT)
		{
			IsRecvThreadRunning = false;
			IsSendThreadRunning = false;
			break;
		}
		else if (MyEvent.type == SDL_KEYDOWN)
		{
			if (KeyState[SDL_SCANCODE_ESCAPE])
			{
				IsRecvThreadRunning = false;
				IsSendThreadRunning = false;
				break;
			}
			int KeyCode = 0;
			if (KeyState[SDL_SCANCODE_W])
			{
				lock_guard<std::mutex> KeyLock(KeyBufferLock);
				KeyBuffer.push('W');
			}
			if (KeyState[SDL_SCANCODE_S])
			{
				lock_guard<std::mutex> KeyLock(KeyBufferLock);
				KeyBuffer.push('S');
			}
			if (KeyState[SDL_SCANCODE_A])
			{
				lock_guard<std::mutex> KeyLock(KeyBufferLock);
				KeyBuffer.push('A');
			}
			if (KeyState[SDL_SCANCODE_D])
			{
				lock_guard<std::mutex> KeyLock(KeyBufferLock);
				KeyBuffer.push('D');
			}
		}

		Render();
	}

	//blocking
	WaitForMultipleObjects(2, ThreadHandles, FALSE, INFINITE);

	closesocket(ServerSocket);

	cout << "End Thread" << endl;

	//TerminateThread(ThreadHandles[0], 0);
	//TerminateThread(ThreadHandles[1], 0);
	IsSendThreadRunning = false;
	IsRecvThreadRunning = false;


	CloseHandle(ThreadHandles[0]);
	CloseHandle(ThreadHandles[1]);

	WSACleanup();

	SDL_DestroyWindow(MyWindow);
	SDL_Quit();

	return 0;
}


void Render()
{
	//system("cls");

	//for (auto Player : MySessionManager.SessionList)
	//{
	//	COORD Where;
	//	Where.X = Player.X;
	//	Where.Y = Player.Y;
	//	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Where);
	//	std::cout << (char)Player.Shape << endl;
	//}


	SDL_SetRenderDrawColor(MyRenderer, 0, 0, 0, 0);
	SDL_RenderClear(MyRenderer);

	{
		lock_guard<std::mutex> lock(SessionLock);
		//SessionLock.lock();
		for (auto Player : MySessionManager.SessionList)
		{
			SDL_SetRenderDrawColor(MyRenderer, Player.R, Player.G, Player.B, 0);
			SDL_Rect MyRect = { Player.X, Player.Y, 30, 30 };
			SDL_RenderFillRect(MyRenderer, &MyRect);
		}
		//SessionLock.unlock();
	}


	SDL_RenderPresent(MyRenderer);

}

void ProcessPacket(SOCKET ProcessSocket, const char* InBuffer)
{
	auto UserPacketData = UserPacket::GetPacketData(InBuffer);

	//std::cout << EnumNamePacketType(UserPacketData->data_type()) << std::endl;

	switch (UserPacketData->data_type())
	{
	case UserPacket::PacketType_S2C_Login:
	{
		MyClientID = UserPacketData->data_as_S2C_Login()->client_socket_id();
	}
	break;
	case UserPacket::PacketType_S2C_Spawn:
	{
		Session InSession;
		auto SpawnData = UserPacketData->data_as_S2C_Spawn();
		InSession.ClientSocket = SpawnData->client_socket_id();
		InSession.Shape = SpawnData->shape();
		InSession.X = SpawnData->position()->x();
		InSession.Y = SpawnData->position()->y();
		InSession.R = SpawnData->color()->r();
		InSession.G = SpawnData->color()->g();
		InSession.B = SpawnData->color()->b();

		{
			lock_guard<std::mutex> lock(SessionLock);
			MySessionManager.Add(InSession);
		}
		//		Render();
	}
	break;
	case UserPacket::PacketType_S2C_Move:
	{
		auto MoveData = UserPacketData->data_as_S2C_Move();

		SOCKET SocketID = MoveData->client_socket_id();
		Session* FindSession = MySessionManager.GetSession(SocketID);
		FindSession->X = MoveData->position()->x();
		FindSession->Y = MoveData->position()->y();
	}
	break;
	case UserPacket::PacketType_S2C_Destroy:
	{
		auto DestroyPacket = UserPacketData->data_as_S2C_Destroy();

		Session* FindSession = MySessionManager.GetSession((SOCKET)DestroyPacket->client_socket_id());
		{
			lock_guard<std::mutex> lock(SessionLock);
			MySessionManager.Delete(*FindSession);
		}
	}
	break;
	}
}



unsigned WINAPI RecvThread(void* Argument)
{
	SOCKET ServerSocket = *(SOCKET*)Argument;

	while (IsRecvThreadRunning)
	{
		memset(RecvBuffer, 0, sizeof(RecvBuffer));
		int RecvBytes = RecvAll(ServerSocket, RecvBuffer);
		if (RecvBytes <= 0)
		{
			std::cout << "recv fail " << endl;
			break;
		}

		ProcessPacket(ServerSocket, RecvBuffer);
	}


	return 0;
}

unsigned WINAPI SendThread(void* Argument)
{
	//책임은 사용하는 놈이 진다.
	SOCKET ServerSocket = *(SOCKET*)Argument;

	while (IsSendThreadRunning)
	{
		if (KeyBuffer.empty())
		{
			YieldProcessor();
			//Sleep(0);
			continue;

		}
		flatbuffers::FlatBufferBuilder SendBuilder;

		flatbuffers::Offset<UserPacket::C2S_Move> C2S_MoveData;
		{
			lock_guard<std::mutex> KeyLock(KeyBufferLock);
			C2S_MoveData = UserPacket::CreateC2S_Move(
				SendBuilder,
				(uint16_t)MyClientID,
				KeyBuffer.front()
			);
			KeyBuffer.pop();
		}

		auto UserPacketData = UserPacket::CreatePacketData(
			SendBuilder,
			UserPacket::PacketType_C2S_Move,
			C2S_MoveData.Union()
		);

		SendBuilder.Finish(UserPacketData);

		SendAll(ServerSocket, SendBuilder);
	}

	return 0;
}

//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//#define SDL_MAIN_HANDLED
//
//
//#include "ChatPacket.h"
//#include "NetUtil.h"
//
//#include <winsock2.h>
//#include <Windows.h>
//#include <iostream>
//#include <process.h>
//#include <conio.h>
//#include"SDL.h"
//#include <mutex>
//#include <queue>
//
//#pragma comment(lib, "ws2_32")
//#pragma comment(lib, "NetCommon")
//#pragma comment(lib,"SDL2")
//#pragma comment(lib,"SDL2main")
//
//using namespace std;
//
//char SendBuffer[1024] = { 0, };
//char RecvBuffer[1024] = { 0, };
//
//bool IsRecvThreadRunning = true;
//bool IsSendThreadRunning = true;
//
//SessionManager MySessionManager;
//SOCKET MyClientID;
//
//
//std::mutex GSessionMutex;
//std::mutex KeyBufferLock;
//
//std::queue<int> KeyBuffer;
//
////SDL전역변수
//SDL_Window* GWindow = nullptr; //윈도우
//SDL_Renderer* GRenderer = nullptr;//화면에 표시 연필같은거
//
//const int WINDOW_WIDTH = 800;
//const int WINDOW_HEIGHT = 600;
//const int TILE_SIZE = 32;
//
//
//
//void Render()
//{
//	if (GRenderer == nullptr)
//	{
//		return;
//	}
//	//배경색
//	SDL_SetRenderDrawColor(GRenderer, 30, 30, 30, 255);
//	SDL_RenderClear(GRenderer);
//	{
//		lock_guard<std::mutex> lock(GSessionMutex);
//		for (auto Player : MySessionManager.SessionList)
//		{
//			SDL_Rect PlayerRect;
//			PlayerRect.x = Player.X * TILE_SIZE;
//			PlayerRect.y = Player.Y * TILE_SIZE;
//			PlayerRect.w = TILE_SIZE;
//			PlayerRect.h = TILE_SIZE;
//
//			SDL_SetRenderDrawColor(GRenderer, Player.R, Player.G, Player.B, 0);
//			SDL_RenderFillRect(GRenderer, &PlayerRect);
//
//			// 테두리
//			SDL_SetRenderDrawColor(GRenderer, 255, 255, 255, 255);
//			SDL_RenderDrawRect(GRenderer, &PlayerRect);
//		}
//	}
//	SDL_RenderPresent(GRenderer);
//	
//
//}
//
//void ProcessPacket(SOCKET ProcessSocket, const char* InBuffer)
//{
//	auto UserPacketData = UserPacket::GetPacketData(InBuffer);
//
//	switch (UserPacketData->data_type())
//	{
//	case UserPacket::PacketType_S2C_Login:
//		{
//		MyClientID = UserPacketData->data_as_S2C_Login()->client_socket_id();
//		}
//		break;
//
//	case UserPacket::PacketType_S2C_Spawn:
//		{
//			Session InSession;
//			auto SpawnData = UserPacketData->data_as_S2C_Spawn();
//			InSession.ClientSocket = SpawnData->client_socket_id();
//			InSession.Shape = SpawnData->shape();
//			InSession.X = SpawnData->position()->x();
//			InSession.Y = SpawnData->position()->y();
//			InSession.R = SpawnData->color()->r();
//			InSession.G = SpawnData->color()->g();
//			InSession.B = SpawnData->color()->b();
//
//			{
//				std::lock_guard<std::mutex> Lock(GSessionMutex);
//				MySessionManager.Add(InSession);
//			}
//			//Render();
//		}
//		break;
//	case UserPacket::PacketType_S2C_Move:
//		{
//			auto MoveData = UserPacketData->data_as_S2C_Spawn();
//
//			Session* FindSession = MySessionManager.GetSession(MoveData->client_socket_id());
//			FindSession->X = MoveData->position()->x();
//			FindSession->Y = MoveData->position()->y();
//
//		}
//		break;
//	case UserPacket::PacketType_S2C_Destroy:
//		{
//		auto DestroyPacket = UserPacketData->data_as_S2C_Destroy();
//						
//			Session* FindSession = MySessionManager.GetSession(DestroyPacket->client_socket_id());
//			{
//				lock_guard<std::mutex> lock(GSessionMutex);
//				MySessionManager.Delete(*FindSession);
//
//			}
//
//			
//
//			//Render();
//
//		}
//		break;
//	}
//
//
//}
//
//unsigned WINAPI RecvThread(void* Argument)
//{
//	SOCKET ServerSocket = *(SOCKET*)Argument;
//
//	while (IsRecvThreadRunning)
//	{
//		int RecvBytes = RecvAll(ServerSocket, RecvBuffer);
//		if (RecvBytes <= 0)
//		{
//			std::cout << "recv fail " << std::endl;
//			break;
//		}
//		ProcessPacket(ServerSocket, RecvBuffer);
//	}
//
//
//	return 0;
//}
//
//unsigned WINAPI SendThread(void* Argument)
//{
//	//책임은 사용하는 놈이 진다.
//	SOCKET ServerSocket = *(SOCKET*)Argument;
//
//	while (IsSendThreadRunning)
//	{
//		if (KeyBuffer.empty())
//		{
//			YieldProcessor();
//			//Sleep(0);
//			continue;
//		}
//		flatbuffers::FlatBufferBuilder SendBuilder;
//
//		flatbuffers::Offset<UserPacket::C2S_Move> C2S_MoveData;
//		{
//			lock_guard<std::mutex> KeyLock(KeyBufferLock);
//			C2S_MoveData = UserPacket::CreateC2S_Move(
//				SendBuilder,
//				(uint16_t)MyClientID,
//				KeyBuffer.front()
//			);
//			KeyBuffer.pop();
//		}
//		auto UserPacketData = UserPacket::CreatePacketData(
//			SendBuilder,
//			UserPacket::PacketType_C2S_Move,
//			C2S_MoveData.Union()
//		);
//		SendBuilder.Finish(UserPacketData);
//		SendAll(ServerSocket,SendBuilder);
//	}
//	return 0;
//}
//
//int SDL_main(int Argc, char* Argv[])
//{
//	//Object sync(Lock)
//	// GameThread(Render)
//	// NetWrokThread
//	std::cout << "client " << std::endl;
//
//	WSAData wsaData;
//
//	WSAStartup(MAKEWORD(2, 2), &wsaData);
//
//	SOCKET ServerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
//
//	SOCKADDR_IN ServerSockAddr;
//	memset(&ServerSockAddr, 0, sizeof(ServerSockAddr));
//	ServerSockAddr.sin_family = AF_INET;
//	ServerSockAddr.sin_addr.s_addr = inet_addr("192.168.0.194");
//	ServerSockAddr.sin_port = htons(35000);
//
//	connect(ServerSocket, (SOCKADDR*)&ServerSockAddr, sizeof(ServerSockAddr));
//
//	std::cout << "client connect" << std::endl;
//
//
//	//sdl 준비
//	SDL_SetMainReady();
//	//sdl 초기화
//	if (SDL_Init(SDL_INIT_VIDEO) != 0)
//	{
//		std::cout << "SDL init Error: " << SDL_GetError() << std::endl;
//		return 1;
//	}
//	//sdl Window 생성
//	GWindow = SDL_CreateWindow(
//		"SDL_Client",
//		SDL_WINDOWPOS_CENTERED,
//		SDL_WINDOWPOS_CENTERED,
//		WINDOW_WIDTH,
//		WINDOW_HEIGHT,
//		SDL_WINDOW_SHOWN
//	);
//	if (GWindow == nullptr)
//	{
//		std::cout << "SDL_CREATEWINDOW ERROR: " << SDL_GetError() << std::endl;
//		SDL_Quit();
//		return 1;
//	}
//	//SDL Renderer 생성
//	GRenderer = SDL_CreateRenderer(
//		GWindow,
//		-1, //렌더링 순서
//		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC //하드웨어 가속 및 모니터 주사율에 맞춰주세요
//	);
//	if (GRenderer == nullptr)
//	{
//		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
//		SDL_DestroyWindow(GWindow);
//		SDL_Quit();
//		return 1;
//	}
//
//	//Memory(Data)->ByteArray(char[] )->Serialize(flatBuffer)
//	flatbuffers::FlatBufferBuilder SendBuilder;
//
//	auto C2S_LoginData = UserPacket::CreateC2S_Login(
//		SendBuilder,
//		SendBuilder.CreateString("Ninja"),
//		SendBuilder.CreateString("1as3f356dsd6gyhg")
//	);
//
//	auto UserPacketData = UserPacket::CreatePacketData(
//		SendBuilder,
//		UserPacket::PacketType_C2S_Login,
//		C2S_LoginData.Union()
//	);
//	SendBuilder.Finish(UserPacketData);
//
//	SendAll(ServerSocket, SendBuilder);
//
//
//	HANDLE ThreadHandles[2] = { 0, };
//	
//	//nonblocking, asynchrous
//	ThreadHandles[0] = (HANDLE)_beginthreadex(0, 0, RecvThread, &ServerSocket, 0, 0);
//	ThreadHandles[1] = (HANDLE)_beginthreadex(0, 0, SendThread, &ServerSocket, 0, 0);
//
//
//	const Uint8* KeyState = SDL_GetKeyboardState(NULL);
//
//	bool IsClientRunning = true;
//
//	while (IsClientRunning)
//	{
//		SDL_Event MyEvent;
//		SDL_PollEvent(&MyEvent);
//		if (MyEvent.type == SDL_QUIT)
//		{
//			IsRecvThreadRunning = false;
//			IsSendThreadRunning = false;
//			break;
//		}
//		else if (MyEvent.type == SDL_KEYDOWN)
//		{
//			if (KeyState[SDL_SCANCODE_ESCAPE])
//			{
//				IsRecvThreadRunning = false;
//				IsSendThreadRunning = false;
//				break;
//			}
//			int KeyCode = 0;
//			if (KeyState[SDL_SCANCODE_W])
//			{
//				lock_guard<std::mutex> KeyLock(KeyBufferLock);
//				KeyBuffer.push('W');
//			}
//			if (KeyState[SDL_SCANCODE_S])
//			{
//				lock_guard<std::mutex> KeyLock(KeyBufferLock);
//				KeyBuffer.push('S');
//			}
//			if (KeyState[SDL_SCANCODE_A])
//			{
//				lock_guard<std::mutex> KeyLock(KeyBufferLock);
//				KeyBuffer.push('A');
//			}
//			if (KeyState[SDL_SCANCODE_D])
//			{
//				lock_guard<std::mutex> KeyLock(KeyBufferLock);
//				KeyBuffer.push('D');
//			}
//		}
//		Render();
//
//	}
//	//blocking
//	WaitForMultipleObjects(2, ThreadHandles, FALSE, INFINITE);
//
//	closesocket(ServerSocket);
//
//	cout << "End Thread" << endl;
//
//	IsSendThreadRunning = false;
//	IsRecvThreadRunning = false;
//
//	// RecvThread가 recv에서 막혀 있을 수 있으므로 깨워줌
//	shutdown(ServerSocket, SD_BOTH);
//	closesocket(ServerSocket);
//
//	std::cout << "End Thread" << std::endl;
//
//
//	CloseHandle(ThreadHandles[0]);
//	CloseHandle(ThreadHandles[1]);
//
//	WSACleanup();
//	// SDL 정리
//	if (GRenderer != nullptr)
//	{
//		SDL_DestroyRenderer(GRenderer);
//		GRenderer = nullptr;
//	}
//
//	if (GWindow != nullptr)
//	{
//		SDL_DestroyWindow(GWindow);
//		GWindow = nullptr;
//	}
//
//	SDL_Quit();
//
//	return 0;
//}