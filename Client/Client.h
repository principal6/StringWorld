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

		int ReceivedByteCount{};
		while (!IsTimedOut())
		{
			if (Listen(&ReceivedByteCount))
			{
				CByteData ByteData{ m_Buffer, ReceivedByteCount };
				auto Bytes{ ByteData.Get() };
				if (Bytes[0] == (char)EServerPacketType::EnterPermission)
				{
					memcpy(m_MyStringID.String, StringID, KStringIDSize);
					memcpy(&m_MyDatum.ID, &Bytes[1], sizeof(ID_t));
					return true;
				}
				return false;
			}
		}
		return false;
	}

	void Chat(const char* Content)
	{
		int ContentLength{ (int)strlen(Content) };
		if (ContentLength == 0) return;

		_LogChat(Content, true);

		Send(EClientPacketType::Chat, m_vChatLog.back().String, KChatTextSize + 1);
	}

private:
	void _LogChat(const char* Content, bool bIsMyChat = false)
	{
		std::string _Content{ (bIsMyChat) ? m_MyStringID.String : Content };
		if (bIsMyChat)
		{
			_Content += ": ";
			_Content += Content;
		}

		SChatText Chat{};
		memcpy(Chat.String, &_Content[0], min((int)_Content.size(), KChatTextSize));
		m_vChatLog.emplace_back(Chat);
	}

public:
	void Input(EInput eInput)
	{
		Send(EClientPacketType::Input, (char)eInput);
	}

private:
	bool Send(EClientPacketType eType, const char* Buffer, int BufferSize)
	{
		CByteData ByteData{};
		ByteData.Append((char)eType);
		// Append ID given by ther server when entered.
		if (eType != EClientPacketType::Enter) ByteData.Append(m_MyDatum.ID);
		ByteData.Append(Buffer, BufferSize);

		int SentByteCount{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)&m_Server, sizeof(m_Server)) };
		if (SentByteCount > 0) return true;
		return false;
	}

	bool Send(EClientPacketType eType, char Char)
	{
		CByteData ByteData{};
		ByteData.Append((char)eType);
		// Append ID given by ther server when entered.
		if (eType != EClientPacketType::Enter) ByteData.Append(m_MyDatum.ID);
		ByteData.Append(Char);

		int SentByteCount{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)&m_Server, sizeof(m_Server)) };
		if (SentByteCount > 0) return true;
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

	bool Listen(int* pReceivedByteCount = nullptr)
	{
		fd_set fdSet;
		FD_ZERO(&fdSet);
		FD_SET(m_Socket, &fdSet);

		if (select(0, &fdSet, nullptr, nullptr, &m_TimeOut) > 0)
		{
			m_TimeOutCount = 0;
			int ServerAddrLen{ (int)sizeof(m_Server) };
			int ReceivedByteCount{ recvfrom(m_Socket, m_Buffer, KBufferSize, 0, (sockaddr*)&m_Server, &ServerAddrLen) };
			if (ReceivedByteCount > 0)
			{
				if (pReceivedByteCount) *pReceivedByteCount = ReceivedByteCount;
				
				CByteData ByteData{ m_Buffer, ReceivedByteCount };
				auto ReceivedData{ ByteData.Get() };
				EServerPacketType eType{ (EServerPacketType)ReceivedData[0] };
				auto WithoutType{ &ReceivedData[1] };
				if (eType == EServerPacketType::Echo)
				{
					//printf("GOT SERVER ECHO: %s\n", WithoutHeader);
				}
				else if (eType == EServerPacketType::EnterPermission)
				{
					//printf("GOT ENTER PERMISSION.\n");
				}
				else if (eType == EServerPacketType::Denial)
				{
					//printf("GOT DENIAL: %s\n", WithoutHeader);
				}
				else if (eType == EServerPacketType::Update)
				{
					//printf("GOT UPDATE: %d bytes\n", ReceivedByteCount);
					short ClientCount{ WithoutType[0] };
					memcpy(&ClientCount, &WithoutType[0], sizeof(ClientCount));

					m_vClientData.resize(ClientCount);
					memcpy(&m_vClientData[0], &WithoutType[sizeof(ClientCount)], sizeof(SClientDatum) * ClientCount);

					ID_t SavedID{ m_MyDatum.ID };
					m_MyDatum = m_vClientData[m_MyDatum.ID];
					m_MyDatum.ID = SavedID;
				}
				else if (eType == EServerPacketType::Chat)
				{
					short ClientID{ WithoutType[0] };
					memcpy(&ClientID, &WithoutType[0], sizeof(ID_t));
					if (ClientID != m_MyDatum.ID)
					{
						_LogChat(&WithoutType[sizeof(ID_t)]);
					}
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

public:
	const SClientDatum& GetMyDatum() const
	{
		return m_MyDatum;
	}

	const std::vector<SClientDatum>& GetClientData() const
	{
		return m_vClientData;
	}

	bool HasChatLog() const
	{
		return m_vChatLog.size();
	}

	const std::vector<SChatText>& GetChatLog() const
	{
		return m_vChatLog;
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
			if (closesocket(m_Socket) == SOCKET_ERROR)
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
	SClientDatum m_MyDatum{};
	SStringID m_MyStringID{};
	std::vector<SClientDatum> m_vClientData{};

private:
	char m_Buffer[KBufferSize]{};

private:
	std::vector<SChatText> m_vChatLog{};
};