#pragma once

#pragma comment (lib, "WS2_32.lib")

#include <WinSock2.h>
#include <iostream>
#include <WS2tcpip.h>

class CUDPClientBase
{
public:
	CUDPClientBase(const char* ServerIP, u_short Port, timeval TimeOut) : m_TimeOut{ TimeOut }
	{
		StartUp(); 
		CreateSocket();
		SetServerAddr(ServerIP, Port);
	}
	virtual ~CUDPClientBase()
	{
		CloseSocket();
		CleanUp(); 
	}

public:
	void SetServerAddr(const char* ServerIP, u_short Port)
	{
		m_ServerAddr.sin_family = AF_INET;
		inet_pton(AF_INET, ServerIP, &m_ServerAddr.sin_addr);
		m_ServerAddr.sin_port = htons(Port);
	}

protected:
	bool _Send(const char* Buffer, int BufferSize = -1) const
	{
		if (!Buffer) return false;
		if (BufferSize < 0) BufferSize = (int)strlen(Buffer);
		int SentByteCount{ sendto(m_Socket, Buffer, BufferSize, 0, (sockaddr*)&m_ServerAddr, sizeof(m_ServerAddr)) };
		if (SentByteCount > 0) return true;
		return false;
	}

	bool _Receive(int* OutReceivedByteCount = nullptr)
	{
		fd_set Copy{ m_SetToSelect };
		if (select(0, &Copy, nullptr, nullptr, &m_TimeOut) > 0)
		{
			m_TimeOutCounter = 0;
			int ServerAddrLen{ (int)sizeof(m_ServerAddr) };
			int ReceivedByteCount{ recvfrom(m_Socket, m_Buffer, KBufferSize, 0, (sockaddr*)&m_ServerAddr, &ServerAddrLen) };
			if (OutReceivedByteCount) *OutReceivedByteCount = ReceivedByteCount;
			return (ReceivedByteCount > 0);
		}
		else
		{
			++m_TimeOutCounter;
			return false;
		}
	}

public:
	bool IsTimedOut() const { return (m_TimeOutCounter >= KTimeOutCountLimit); }
	bool IsTerminating() const { return !m_bRunning; }

public:
	virtual void Terminate() 
	{
		m_bRunning = false; 
	}

protected:
	void StartUp()
	{
		m_bRunning = true;

		WSADATA Data{};
		int Error{ WSAStartup(MAKEWORD(2, 2), &Data) };
		if (Error)
		{
			printf("Failed to start up: %d\n", Error);
			return;
		}
	}

	void CreateSocket()
	{
		m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (m_Socket == INVALID_SOCKET)
		{
			printf("Failed to create socket: %d\n", WSAGetLastError());
			m_Socket = 0;
			return;
		}

		FD_SET(m_Socket, &m_SetToSelect);
	}

	void CloseSocket()
	{
		if (m_Socket && closesocket(m_Socket) == SOCKET_ERROR)
		{
			printf("Failed to close socket: %d", WSAGetLastError());
		}
		m_Socket = 0;
	}

	void CleanUp()
	{
		int Error{ WSACleanup() };
		if (Error)
		{
			printf("Failed to clean up: %d", Error);
		}
	}

protected:
	static constexpr int KBufferSize{ 2048 };
	static constexpr int KTimeOutCountLimit{ 10 };

protected:
	bool m_bRunning{};

protected:
	SOCKET m_Socket{};

protected:
	SOCKADDR_IN m_ServerAddr{};

protected:
	char m_Buffer[KBufferSize]{};

private:
	int m_TimeOutCounter{};
	timeval m_TimeOut{};
	fd_set m_SetToSelect{};
};
