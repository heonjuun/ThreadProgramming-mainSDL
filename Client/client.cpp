#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SDL_MAIN_HANDLED


#include "ChatPacket.h"
#include "NetUtil.h"

#include <winsock2.h>
#include <Windows.h>
#include <iostream>
#include <process.h>
#include <conio.h>
#include"SDL.h"
#include <mutex>
#include <queue>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "NetCommon")
#pragma comment(lib,"SDL2")
#pragma comment(lib,"SDL2main")

using namespace std;

char SendBuffer[1024] = { 0, };
char RecvBuffer[1024] = { 0, };

bool IsRecvThreadRunning = true;
bool IsSendThreadRunning = true;

SessionManager MySessionManager;
SOCKET MyClientID;


std::mutex GSessionMutex;
std::mutex KeyBufferLock;

std::queue<int> KeyBuffer;

//SDL전역변수
SDL_Window* GWindow = nullptr; //윈도우
SDL_Renderer* GRenderer = nullptr;//화면에 표시 연필같은거

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int TILE_SIZE = 32;



void Render()
{
	if (GRenderer == nullptr)
	{
		return;
	}
	//배경색
	SDL_SetRenderDrawColor(GRenderer, 30, 30, 30, 255);
	SDL_RenderClear(GRenderer);
	{
		lock_guard<std::mutex> lock(GSessionMutex);
		for (auto Player : MySessionManager.SessionList)
		{
			SDL_Rect PlayerRect;
			PlayerRect.x = Player.X * TILE_SIZE;
			PlayerRect.y = Player.Y * TILE_SIZE;
			PlayerRect.w = TILE_SIZE;
			PlayerRect.h = TILE_SIZE;

			SDL_SetRenderDrawColor(GRenderer, Player.R, Player.G, Player.B, 0);
			// 내 플레이어는 파란색
			//if (Player.ClientSocket == MyClientID)
			//{
			//	SDL_SetRenderDrawColor(GRenderer, 0, 180, 255, 255);
			//}
			//// 다른 플레이어는 빨간색
			//else
			//{
			//	SDL_SetRenderDrawColor(GRenderer, 255, 80, 80, 255);
			//}
			SDL_RenderFillRect(GRenderer, &PlayerRect);

			// 테두리
			SDL_SetRenderDrawColor(GRenderer, 255, 255, 255, 255);
			SDL_RenderDrawRect(GRenderer, &PlayerRect);
		}
	}
	SDL_RenderPresent(GRenderer);

	//system("cls");

	//for(auto Player : MySessionManager.SessionList)
	//{
	//	COORD Where;
	//	Where.X = Player.X;
	//	Where.Y = Player.Y;
	//	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Where);
	//	std::cout << (char)Player.Shape << endl;
	//}
	

}

void ProcessPacket(SOCKET ProcessSocket, const char* InBuffer, const Header& InHeader)
{
	switch ((EPacketType)InHeader.PacketType)
	{
	case EPacketType::S2C_Login:
		{
			S2C_Login LoginPacket;
			LoginPacket.Parse(InBuffer);
			std::cout << LoginPacket.ToString() << std::endl;
			MyClientID = LoginPacket.ClientSocketID;
		}
		break;
	case EPacketType::S2C_Spawn:
		{
			S2C_Spawn SpawnData;
			SpawnData.Parse(InBuffer);
			cout << SpawnData.ToString() << endl;

			Session InSession;
			InSession.ClientSocket = SpawnData.ClientSocket;
			InSession.Shape = SpawnData.Shape;
			InSession.X = SpawnData.X;
			InSession.Y = SpawnData.Y;
			InSession.R = SpawnData.R;
			InSession.G = SpawnData.G;
			InSession.B = SpawnData.B;

			{
				std::lock_guard<std::mutex> Lock(GSessionMutex);
				MySessionManager.Add(InSession);
			}
			//Render();
		}
		break;
	case EPacketType::S2C_Move:
		{
			S2C_Move MoveData;
			MoveData.Parse(InBuffer);
			{
				std::lock_guard<std::mutex> Lock(GSessionMutex);

				Session* FindSession = MySessionManager.GetSession(MoveData.ClientSocket);

				if (FindSession != nullptr)
				{
					FindSession->X = MoveData.X;
					FindSession->Y = MoveData.Y;
				}
			}

			/*std::cout << MoveData.ToString() << endl;
			Render();*/
		}
		break;
	case EPacketType::S2C_Destroy:
		{
			S2C_Destroy DestroyPacket;
			DestroyPacket.Parse(InBuffer);
			{
				std::lock_guard<std::mutex> Lock(GSessionMutex);

				Session* FindSession = MySessionManager.GetSession(DestroyPacket.ClientSocket);

				if (FindSession != nullptr)
				{
					std::cout << "Quit : " << FindSession->ClientSocket << endl;
					MySessionManager.Delete(*FindSession);
				}
			}

			//Render();

		}
		break;
	}


}

unsigned WINAPI RecvThread(void* Argument)
{
	SOCKET ServerSocket = *(SOCKET*)Argument;

	while (IsRecvThreadRunning)
	{
		unsigned short PacketSize = 0;

		//header
		Header DataHeader;
		int RecvBytes = RecvAll(ServerSocket, (char*)&DataHeader, HeaderSize);
		if (RecvBytes <= 0)
		{
			std::cout << "header recv fail " << std::endl;
			break;
		}

		DataHeader.NetworkToHost();

		memset(RecvBuffer, 0, sizeof(RecvBuffer));
		//data JSON
		RecvBytes = RecvAll(ServerSocket, RecvBuffer, DataHeader.PacketSize);
		if (RecvBytes <= 0)
		{
			std::cout << "Data recv fail " << std::endl;
			break;
		}

		ProcessPacket(ServerSocket, RecvBuffer, DataHeader);
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

		C2S_Move MoveData;
		MoveData.ClientSocket = MyClientID;
		{
			lock_guard<std::mutex> KeyLock(KeyBufferLock);
			MoveData.Direction = KeyBuffer.front();
			KeyBuffer.pop();
		}

		//header
		Header DataHeader;
		DataHeader.MakeHeader((int)(MoveData.ToString().length()), EPacketType::C2S_Move);
		int SentBytes = SendAll(ServerSocket, (char*)&DataHeader, HeaderSize);
		if (SentBytes <= 0)
		{
			std::cout << "header send fail." << endl;
		}

		//Data
		SentBytes = SendAll(ServerSocket, MoveData.ToString().c_str(), (int)(MoveData.ToString().length()));
		if (SentBytes <= 0)
		{
			std::cout << "Data send fail." << endl;
		}
		////책임은 사용하는 놈이 진다.
		//SOCKET ServerSocket = *(SOCKET*)Argument;

		//while (IsSendThreadRunning)
		//{
		//	int KeyCode = _getch();

		//	if (!(KeyCode == 'w' ||
		//		KeyCode == 'W' ||
		//		KeyCode == 'a' ||
		//		KeyCode == 'A' ||
		//		KeyCode == 's' ||
		//		KeyCode == 'S' ||
		//		KeyCode == 'd' ||
		//		KeyCode == 'D'))
		//	{
		//		continue;
		//	}
		//	C2S_Move MoveData;
		//	MoveData.ClientSocket = MyClientID;
		//	MoveData.Direction = KeyCode;


		//	//header
		//	Header DataHeader;
		//	DataHeader.MakeHeader((int)(MoveData.ToString().length()), EPacketType::C2S_Move);
		//	int SentBytes = SendAll(ServerSocket, (char*)&DataHeader, HeaderSize);
		//	if (SentBytes <= 0)
		//	{
		//		std::cout << "header send fail." << std::endl;
		//	}

		//	//Data
		//	SentBytes = SendAll(ServerSocket, MoveData.ToString().c_str(), (int)(MoveData.ToString().length()));
		//	if (SentBytes <= 0)
		//	{
		//		std::cout << "Data send fail." << std::endl;
		//	}
		//

		//}
	}
	return 0;
}

int SDL_main(int Argc, char* Argv[])
{
	//Object sync(Lock)
	// GameThread(Render)
	// NetWrokThread
	std::cout << "client " << std::endl;

	WSAData wsaData;

	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET ServerSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	SOCKADDR_IN ServerSockAddr;
	memset(&ServerSockAddr, 0, sizeof(ServerSockAddr));
	ServerSockAddr.sin_family = AF_INET;
	ServerSockAddr.sin_addr.s_addr = inet_addr("192.168.0.194");
	ServerSockAddr.sin_port = htons(35000);

	connect(ServerSocket, (SOCKADDR*)&ServerSockAddr, sizeof(ServerSockAddr));

	std::cout << "client connect" << std::endl;


	//sdl 준비
	SDL_SetMainReady();
	//sdl 초기화
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::cout << "SDL init Error: " << SDL_GetError() << std::endl;
		return 1;
	}
	//sdl Window 생성
	GWindow = SDL_CreateWindow(
		"SDL_Client",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		SDL_WINDOW_SHOWN
	);
	if (GWindow == nullptr)
	{
		std::cout << "SDL_CREATEWINDOW ERROR: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}
	//SDL Renderer 생성
	GRenderer = SDL_CreateRenderer(
		GWindow,
		-1, //렌더링 순서
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC //하드웨어 가속 및 모니터 주사율에 맞춰주세요
	);
	if (GRenderer == nullptr)
	{
		std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_DestroyWindow(GWindow);
		SDL_Quit();
		return 1;
	}


	C2S_Login LoginData;
	LoginData.UserID = "Ninja";
	LoginData.HashKey = "1as3f356dsd6gyhg";

	Header LoginHeader;
	LoginHeader.MakeHeader(static_cast<unsigned short>(LoginData.ToString().length()), EPacketType::C2S_Login);

	//Login 요청
	if (SendAll(ServerSocket, (char*)&LoginHeader, HeaderSize) <= 0)
	{
		std::cout << "login header Error" << std::endl;
	}

	if ( SendAll(ServerSocket, LoginData.ToString().c_str(), (int)LoginData.ToString().length()) <= 0)
	{
		std::cout << "login data Error" << std::endl;
	}



	HANDLE ThreadHandles[2] = { 0, };
	
	//nonblocking, asynchrous
	ThreadHandles[0] = (HANDLE)_beginthreadex(0, 0, RecvThread, &ServerSocket, /*CREATE_SUSPENDED*/0, 0);
	ThreadHandles[1] = (HANDLE)_beginthreadex(0, 0, SendThread, &ServerSocket, /*CREATE_SUSPENDED*/0, 0);


	const Uint8* KeyState = SDL_GetKeyboardState(NULL);

	bool IsClientRunning = true;

	while (IsClientRunning)
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

	IsSendThreadRunning = false;
	IsRecvThreadRunning = false;

	// RecvThread가 recv에서 막혀 있을 수 있으므로 깨워줌
	shutdown(ServerSocket, SD_BOTH);
	closesocket(ServerSocket);

	std::cout << "End Thread" << std::endl;


	////ResumeThread(ThreadHandles[0]);
	////ResumeThread(ThreadHandles[1]);
	////SuspendThread(ThreadHandles[0]);
	////SuspendThread(ThreadHandles[1]);


	////blocking
	//WaitForMultipleObjects(2, ThreadHandles, FALSE, INFINITE);

	//closesocket(ServerSocket);

	//std::cout << "End Thread" << std::endl;

	//TerminateThread(ThreadHandles[0], 0);
	//TerminateThread(ThreadHandles[1], 0);


	/*IsSendThreadRunning = false;
	IsRecvThreadRunning = false;*/


	CloseHandle(ThreadHandles[0]);
	CloseHandle(ThreadHandles[1]);

	WSACleanup();
	// SDL 정리
	if (GRenderer != nullptr)
	{
		SDL_DestroyRenderer(GRenderer);
		GRenderer = nullptr;
	}

	if (GWindow != nullptr)
	{
		SDL_DestroyWindow(GWindow);
		GWindow = nullptr;
	}

	SDL_Quit();

	return 0;
}



//while (SDL_PollEvent(&Event))
//{
//	if (Event.type == SDL_QUIT)
//	{
//		IsClientRunning = false;
//	}

//	if (Event.type == SDL_KEYDOWN)
//	{
//		if (Event.key.keysym.sym == SDLK_ESCAPE)
//		{
//			IsClientRunning = false;
//		}
//	}
//}

// RecvThread 또는 SendThread 중 하나가 종료되었는지 확인
//DWORD WaitResult = WaitForMultipleObjects(
//	2,
//	ThreadHandles,
//	FALSE,
//	0
//);

//if (WaitResult >= WAIT_OBJECT_0 && WaitResult < WAIT_OBJECT_0 + 2)
//{
//	IsClientRunning = false;
//}

//SDL_Delay(16);