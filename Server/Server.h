#pragma once

#pragma comment (lib, "WS2_32.lib")

#include <WinSock2.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <thread>
#include "SharedDefinitions.h"

class CServer
{
public:
	CServer() { StartUp(); }
	~CServer() { CleanUp(); }

public:
	bool Open()
	{
		if (m_bGotHost)
		{
			m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (m_Socket == INVALID_SOCKET)
			{
				printf("%s socket(): %d\n", KFailureHead, WSAGetLastError());
				return false;
			}
			m_bSocketCreated = true;

			SOCKADDR_IN addr{};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(KPort);
			addr.sin_addr = m_HostAddr.sin_addr;
			if (bind(m_Socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
			{
				printf("%s bind(): %d\n", KFailureHead, WSAGetLastError());
				return false;
			}

			m_bOpen = true;
			return true;
		}
		return false;
	}

	void Close()
	{
		m_bClosing = true;
	}

	bool IsClosing()
	{
		return m_bClosing;
	}

	void Listen(bool bShouldEcho = false)
	{
		if (!m_bOpen) return;
		
		timeval TimeOut{ 1, 0 };
		fd_set fdSet;
		FD_ZERO(&fdSet);
		FD_SET(m_Socket, &fdSet);

		if (select(0, &fdSet, 0, 0, &TimeOut) > 0)
		{
			SOCKADDR_IN Client{};
			int ClientAddrLen{ (int)sizeof(Client) };
			int ReceivedByteCount{ recvfrom(m_Socket, m_Buffer, KBufferSize, 0, (sockaddr*)&Client, &ClientAddrLen) };
			if (ReceivedByteCount > 0)
			{
				CByteData ByteData{ m_Buffer, ReceivedByteCount };
				auto ReceivedData{ ByteData.Get() };
				EClientPacketType eType{ (EClientPacketType)ReceivedData[0] };
				if (eType == EClientPacketType::Enter)
				{
					short StringIDLength{};
					memcpy(&StringIDLength, &ReceivedData[1], sizeof(short));

					char StringID[KStringIDSize + 1]{};
					memcpy(StringID, &ReceivedData[1 + sizeof(short)], StringIDLength);

					if (m_umapClientStringIDtoID.find(StringID) == m_umapClientStringIDtoID.end())
					{
						ID_t NewClientID{ m_IDCounter };
						++m_IDCounter;

						m_vClients.emplace_back(Client);
						m_vClientData.emplace_back();
						m_vClientStringIDs.emplace_back();
						memcpy(&m_vClientStringIDs.back(), StringID, KStringIDSize); // Save StringID
						m_vClientData.back().ID = NewClientID;

						m_umapClientStringIDtoID[StringID] = NewClientID;
						m_umapClientIDtoIP[NewClientID] = Client.sin_addr.S_un.S_addr;
						m_umapClientIDtoIndex[NewClientID] = m_vClientData.size() - 1;

						CByteData IDBytes{};
						IDBytes.Append(NewClientID);
						Send(EServerPacketType::EnterPermission, IDBytes.Get(), IDBytes.Size(), &Client);
					}
					else
					{
						Send(EServerPacketType::Denial, "Nickname collision occured.", 28, &Client);
					}
				}
				else
				{
					ID_t ClientID{};
					memcpy(&ClientID, ByteData.Get(1), sizeof(ID_t));
					size_t ClientIndex{ m_umapClientIDtoIndex.at(ClientID) };
					if (m_umapClientIDtoIP.at(ClientID) != Client.sin_addr.S_un.S_addr) return; // Validation check

					auto WithoutHeader{ ByteData.Get(3) };
					auto WithoutHeaderSize{ ByteData.Size(3) };
					switch (eType)
					{
					case EClientPacketType::Leave:
						break;
					case EClientPacketType::Input:
						if (WithoutHeader[0] == (char)EInput::Right)
						{
							++m_vClientData[ClientIndex].X;
						}
						else if (WithoutHeader[0] == (char)EInput::Left)
						{
							--m_vClientData[ClientIndex].X;
						}
						else if (WithoutHeader[0] == (char)EInput::Up)
						{
							--m_vClientData[ClientIndex].Y;
						}
						else if (WithoutHeader[0] == (char)EInput::Down)
						{
							++m_vClientData[ClientIndex].Y;
						}
						break;
					case EClientPacketType::Chat:
					{
						CByteData Chat{};
						Chat.Append(ClientID);
						Chat.Append(WithoutHeader, WithoutHeaderSize);
						// Broadcast chat to all the clients
						for (auto& ClientEntry : m_vClients)
						{
							Send(EServerPacketType::Chat, Chat.Get(), Chat.Size(), &ClientEntry);
						}
						break;
					}
					default:
						const auto& IPv4{ Client.sin_addr.S_un.S_un_b };
						printf("[ID %d | IP %d.%d.%d.%d | BC %d]: %s\n",
							ClientID, IPv4.s_b1, IPv4.s_b2, IPv4.s_b3, IPv4.s_b4, ReceivedByteCount, WithoutHeader);

						// echo
						if (bShouldEcho) Send(EServerPacketType::Echo, WithoutHeader, WithoutHeaderSize, &Client);
						break;
					}
				}
			}
		}
	}

	// Update once per tick
	bool Update()
	{
		if (!m_bOpen) return false;
		if (m_vClientData.empty()) return false;

		// Data for update
		CByteData ByteData{};
		ByteData.Append((char)EServerPacketType::Update);
		ByteData.Append((short)m_vClientData.size()); // Client count
		ByteData.Append(&m_vClientData[0], (int)(m_vClientData.size() * sizeof(SClientDatum)));
		
		// Broadcast updated data to all the clients
		for (auto& Client : m_vClients)
		{
			sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)&Client, sizeof(Client));
		}

		return true;
	}

private:
	bool Send(EServerPacketType eType, const char* Buffer, int BufferSize, SOCKADDR_IN* Client)
	{
		CByteData ByteData{};
		ByteData.Append((char)eType);
		ByteData.Append(Buffer, BufferSize);
		
		int SentByteCount{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)Client, sizeof(SOCKADDR_IN)) };
		if (SentByteCount > 0) return true;
		return false;
	}

public:
	void DisplayInfo()
	{
		auto& IPv4{ m_HostAddr.sin_addr.S_un.S_un_b };
		printf("========================================\n");
		printf(" Server IP: %d.%d.%d.%d\n", IPv4.s_b1, IPv4.s_b2, IPv4.s_b3, IPv4.s_b4);
		printf(" Service Port: %d\n", KPort);
		printf("========================================\n");
	}

private:
	void StartUp()
	{
		WSADATA wsaData{};
		int Result{ WSAStartup(MAKEWORD(2, 2), &wsaData) };
		if (Result)
		{
			printf("%s WSAStartup(): %d\n", KFailureHead, Result);
			return;
		}
		m_bStartUp = true;

		m_bGotHost = GetHostIP();
	}

	bool GetHostIP()
	{
		SOCKET Socket{ socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) };
		if (Socket == SOCKET_ERROR)
		{
			printf("%s socket(): %d\n", KFailureHead, WSAGetLastError());
			return false;
		}

		SOCKADDR_IN loopback{};
		loopback.sin_family = AF_INET;
		loopback.sin_addr.S_un.S_addr = INADDR_LOOPBACK;
		loopback.sin_port = htons(9);
		if (connect(Socket, (sockaddr*)&loopback, sizeof(loopback)) == SOCKET_ERROR)
		{
			printf("%s connect(): %d\n", KFailureHead, WSAGetLastError());
			return false;
		}

		int host_length{ (int)sizeof(m_HostAddr) };
		if (getsockname(Socket, (sockaddr*)&m_HostAddr, &host_length) == SOCKET_ERROR)
		{
			printf("%s getsockname(): %d\n", KFailureHead, WSAGetLastError());
			return false;
		}

		if (closesocket(Socket) == SOCKET_ERROR)
		{
			printf("%s closesocket(): %d\n", KFailureHead, WSAGetLastError());
		}
		return true;
	}

	void CleanUp()
	{
		if (m_bSocketCreated)
		{
			if (closesocket(m_Socket) == SOCKET_ERROR)
			{
				printf("%s closesocket(): %d\n", KFailureHead, WSAGetLastError());
			}
			else
			{
				m_bSocketCreated = false;
				m_Socket = 0;
			}
		}
		if (m_bStartUp)
		{
			int Result{ WSACleanup() };
			if (Result)
			{
				printf("%s WSACleanup(): %d\n", KFailureHead, Result);
			}
			else
			{
				m_bStartUp = false;
			}
		}
	}

private:
	static constexpr const char* KFailureHead{ "[Failed]" };
	static constexpr int KBufferSize{ 2048 };
	static constexpr int KPort{ 9999 };

private:
	bool m_bStartUp{};
	bool m_bGotHost{};
	bool m_bOpen{};
	bool m_bClosing{};

private:
	SOCKET m_Socket{};
	bool m_bSocketCreated{};
	SOCKADDR_IN m_HostAddr{};

private:
	char m_Buffer[KBufferSize]{};

private:
	ID_t m_IDCounter{};
	std::unordered_map<ID_t, size_t> m_umapClientIDtoIndex{};
	std::unordered_map<ID_t, ULONG> m_umapClientIDtoIP{}; // For validation
	std::unordered_map<std::string, ID_t> m_umapClientStringIDtoID{};
	std::vector<SClientDatum> m_vClientData{};
	std::vector<SOCKADDR_IN> m_vClients{};
	std::vector<SStringID> m_vClientStringIDs{};
};
