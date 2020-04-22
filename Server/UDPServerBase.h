#pragma once

#pragma comment (lib, "WS2_32.lib")

#include <WinSock2.h>
#include <iostream>
#include <WS2tcpip.h>
#include <unordered_set>

union UClientAddr
{
	UClientAddr() {}
	UClientAddr(ULONG _IPv4, USHORT _Port) : IPv4{ _IPv4 }, Port{ _Port } {}
	UClientAddr(ULONGLONG _Data) : Data{ _Data } {}
	ULONGLONG Data{};
	struct { ULONG IPv4; USHORT Port; };
};

class CUDPServerBase
{
public:
	CUDPServerBase(USHORT Port, timeval TimeOut) : m_TimeOut{ TimeOut }
	{
		StartUp();
		CreateSocket();
		SetHostAddr(Port);
		BindSocket();
	}
	virtual ~CUDPServerBase()
	{
		CloseSocket();
		CleanUp(); 
	}

protected:
	bool _Receive(int* OutReceivedByteCount = nullptr, SOCKADDR_IN* OutClientAddr = nullptr)
	{
		fd_set Copy{ m_SetToSelect };
		if (select(0, &Copy, nullptr, nullptr, &m_TimeOut) > 0)
		{
			SOCKADDR_IN ClientAddr{};
			int ClientAddrLen{ (int)sizeof(ClientAddr) };
			int ReceivedByteCount{ recvfrom(m_Socket, m_Buffer, KBufferSize, 0, (sockaddr*)&ClientAddr, &ClientAddrLen) };
			if (OutReceivedByteCount) *OutReceivedByteCount = ReceivedByteCount;
			if (OutClientAddr) *OutClientAddr = ClientAddr;
			if (ReceivedByteCount > 0)
			{
				UClientAddr ClientInfo{ ClientAddr.sin_addr.S_un.S_addr, ClientAddr.sin_port };
				if (m_usClientAddrs.find(ClientInfo.Data) == m_usClientAddrs.end())
				{
					m_usClientAddrs.insert(ClientInfo.Data);
				}
				return true;
			}
			return false;
		}
		return false;
	}

	bool _SendTo(const SOCKADDR_IN* Addr, const char* Buffer, int BufferSize = -1) const
	{
		if (!Addr || !Buffer) return false;
		if (BufferSize < 0) BufferSize = (int)strlen(Buffer);
		int SentByteCount{ sendto(m_Socket, Buffer, BufferSize, 0, (sockaddr*)Addr, sizeof(*Addr)) };
		return (SentByteCount > 0);
	}

public:
	bool IsTerminating() const { return !m_bRunning; }

public:
	void Terminate() 
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

	void SetHostAddr(USHORT Port)
	{
		char HostName[256]{};
		if (gethostname(HostName, 256) == SOCKET_ERROR)
		{
			printf("Failed to get host name: %d\n", WSAGetLastError());
			return;
		}

		ADDRINFO Hint{};
		Hint.ai_family = PF_INET; // IPv4
		ADDRINFO* AddrInfo{};
		int Error{ getaddrinfo(HostName, nullptr, &Hint, &AddrInfo) };
		if (Error)
		{
			printf("Failed to get address info: %d\n", Error);
			return;
		}
		memcpy(&m_HostAddr, AddrInfo->ai_addr, sizeof(m_HostAddr));
		m_HostAddr.sin_port = htons(Port);
		freeaddrinfo(AddrInfo);
	}

	void BindSocket()
	{
		SOCKADDR_IN Addr{};
		Addr.sin_family = AF_INET;
		Addr.sin_port = m_HostAddr.sin_port;
		Addr.sin_addr.S_un.S_addr = INADDR_ANY;
		if (bind(m_Socket, (sockaddr*)&Addr, sizeof(Addr)) == SOCKET_ERROR)
		{
			printf("Failed to bind socket: %d\n", WSAGetLastError());
		}
	}

	void CloseSocket()
	{
		if (m_Socket && closesocket(m_Socket) == SOCKET_ERROR)
		{
			printf("Failed to close socket: %d\n", WSAGetLastError());
		}
		m_Socket = 0;
	}

	void CleanUp()
	{
		int Error{ WSACleanup() };
		if (Error)
		{
			printf("Failed to clean up: %d\n", Error);
		}
	}

protected:
	static constexpr int KBufferSize{ 2048 };

protected:
	bool m_bRunning{};

protected:
	SOCKET m_Socket{};

protected:
	SOCKADDR_IN m_HostAddr{};

protected:
	char m_Buffer[KBufferSize]{};

protected:
	timeval m_TimeOut{};
	fd_set m_SetToSelect{};

protected:
	std::unordered_set<ULONGLONG> m_usClientAddrs{};
};
