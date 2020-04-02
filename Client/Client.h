#pragma once

#pragma comment (lib, "WS2_32.lib")

#include <WinSock2.h>
#include <iostream>
#include <WS2tcpip.h>
#include <vector>
#include "../Server/SharedDefinitions.h"

class CClient
{
public:
	CClient(u_short Port, timeval TimeOut) : m_Port{ Port }, m_TimeOut{ TimeOut }
	{
		StartUp();
	}
	CClient(const char* ServerIP, u_short Port, timeval TimeOut) : m_Port{ Port }, m_TimeOut{ TimeOut }
	{
		StartUp(); 
		Connect(ServerIP);
	}
	~CClient() { CleanUp(); }

public:
	bool Enter(const char* StringID)
	{
		if (!StringID || strlen(StringID) == 0) return false;

		CByteData _StringID{};
		_StringID.Append((short)strlen(StringID));
		_StringID.Append(StringID, (int)strlen(StringID));
		Send(EClientPacketType::Enter, _StringID.Get(), _StringID.Size());

		int ReceivedBytes{};
		while (!IsTimedOut())
		{
			if (Listen(&ReceivedBytes))
			{
				CByteData ByteData{ m_Buffer, ReceivedBytes };
				auto Bytes{ ByteData.Get() };
				if (Bytes[0] == (char)EServerPacketType::EnterPermission)
				{
					memcpy(&m_ID, &Bytes[1], sizeof(ID_t));
					return true;
				}
				return false;
			}
		}
		return false;
	}

	void Talk(const char* Content)
	{
		int ContentLength{ (int)strlen(Content) };
		if (ContentLength == 0) return;
		Send(EClientPacketType::Talk, Content, ContentLength + 1);
	}

private:
	bool Send(EClientPacketType eType, const char* Buffer, int BufferSize)
	{
		CByteData ByteData{};
		ByteData.Append((char)eType);
		
		if (eType == EClientPacketType::Enter)
		{
			ByteData.Append(Buffer, BufferSize);
			
			int SentBytes{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)&m_Server, sizeof(m_Server)) };
			if (SentBytes > 0) return true;
		}
		else
		{
			ByteData.Append(m_ID);
			ByteData.Append(Buffer, BufferSize);
			int SentBytes{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)&m_Server, sizeof(m_Server)) };
			if (SentBytes > 0) return true;
		}
		return false;
	}

public:
	bool Connect(const char* ServerIP)
	{
		if (m_Socket > 0)
		{
			printf("Already connected to server.\n");
			return false;
		}

		m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (m_Socket == INVALID_SOCKET)
		{
			printf("Failed to connect.\n");
			return false;
		}
		m_bSocketCreated = true;

		ADDRINFO Hints{};
		Hints.ai_family = AF_INET;
		Hints.ai_socktype = SOCK_DGRAM;
		Hints.ai_protocol = IPPROTO_UDP;

		char szPort[255]{};
		_itoa_s(htons(m_Port), szPort, 10);

		ADDRINFO* Server{};
		getaddrinfo(ServerIP, szPort, &Hints, &Server);
		m_Server = *(SOCKADDR_IN*)(Server->ai_addr);
		m_Server.sin_port = htons(m_Server.sin_port);

		return true;
	}

	bool Listen(int* pReceivedBytes = nullptr)
	{
		fd_set fdSet;
		FD_ZERO(&fdSet);
		FD_SET(m_Socket, &fdSet);

		int Result{ select(0, &fdSet, 0, 0, &m_TimeOut) };
		if (Result > 0)
		{
			m_TimeOutCount = 0;
			int ServerAddrLen{ (int)sizeof(m_Server) };
			int ReceivedBytes{ recvfrom(m_Socket, m_Buffer, KBufferSize, 0, (sockaddr*)&m_Server, &ServerAddrLen) };
			if (ReceivedBytes > 0)
			{
				if (pReceivedBytes) *pReceivedBytes = ReceivedBytes;

				CByteData ByteData{ m_Buffer, ReceivedBytes };
				if (ByteData.Get()[0] == (char)EServerPacketType::Echo)
				{
					printf("SERVER ECHOED: %s\n", ByteData.Get(1));
				}
				else if (ByteData.Get()[0] == (char)EServerPacketType::Denial)
				{
					printf("DENIAL: %s\n", ByteData.Get(1));
				}

				return true;
			}
		}
		else
		{
			++m_TimeOutCount;
		}
		return false;
	}

	bool IsTimedOut()
	{
		return (m_TimeOutCount >= 3);
	}

private:
	void StartUp()
	{
		WSADATA wsaData{};
		int Result{ WSAStartup(MAKEWORD(2, 2), &wsaData) };
		if (Result)
		{
			printf("Failed to start up WSA.\n");
			return;
		}
		m_bStartUp = true;
	}

	void CleanUp()
	{
		if (m_bSocketCreated)
		{
			if (closesocket(m_Socket))
			{
				printf("failed to closesocket(): %d", WSAGetLastError());
			}
			else
			{
				m_Socket = 0;
			}
		}
		if (m_bStartUp)
		{
			WSACleanup();
		}
	}

private:
	static constexpr int KBufferSize{ 2048 };

private:
	bool m_bStartUp{};
	bool m_bSocketCreated{};
	SOCKET m_Socket{};
	u_short m_Port{};
	SOCKADDR_IN m_Server{};

private:
	int m_TimeOutCount{};
	timeval m_TimeOut{};

private:
	ID_t m_ID{ -1 };

private:
	char m_Buffer[KBufferSize]{};
};