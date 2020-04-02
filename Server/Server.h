#pragma once

#pragma comment (lib, "WS2_32.lib")

#include <WinSock2.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <thread>
#include "SharedDefinitions.h"

struct SClientData
{
	char StringID[KClientStringIDMaxSize + 1]{};
	short X{};
	short Y{};
};

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
			if (bind(m_Socket, (sockaddr*)&addr, sizeof(addr)))
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

		int Result{ select(0, &fdSet, 0, 0, &TimeOut) };
		if (Result > 0)
		{
			SOCKADDR_IN Client{};
			int ClientAddrLen{ (int)sizeof(Client) };
			int ReceivedBytes{ recvfrom(m_Socket, m_Buffer, KBufferSize, 0, (sockaddr*)&Client, &ClientAddrLen) };
			if (ReceivedBytes > 0)
			{
				CByteData ByteData{ m_Buffer, ReceivedBytes };
				if (ByteData.Get()[0] == (char)EClientPacketType::Enter)
				{
					short StringIDLength{};
					memcpy(&StringIDLength, &ByteData.Get()[1], sizeof(short));

					char StringID[KClientStringIDMaxSize]{};
					memcpy(StringID, &ByteData.Get()[1 + sizeof(short)], StringIDLength);

					if (m_umapClientStringIDtoIndex.find(StringID) == m_umapClientStringIDtoIndex.end())
					{
						ID_t ID{ KIDOffset + m_IDCounter };
						++m_IDCounter;

						m_vClientData.emplace_back();
						memcpy(m_vClientData.back().StringID, StringID, StringIDLength);
						m_umapClientStringIDtoIndex[StringID] = ID;
						m_umapClientIDtoIndex[ID] = m_vClientData.size() - 1;

						CByteData IDBytes{};
						IDBytes.Append(ID);
						Send(EServerPacketType::EnterPermission, IDBytes.Get(), IDBytes.Size(), &Client);
					}
					else
					{
						Send(EServerPacketType::Denial, "Nickname collision occured.", 28, &Client);
					}
					return;
				}
				else
				{
					const auto& Bytes{ Client.sin_addr.S_un.S_un_b };
					ID_t ID{};
					memcpy(&ID, ByteData.Get(1), sizeof(ID_t));

					size_t Index{ m_umapClientIDtoIndex.at(ID) };
					printf("[ID %d | NN %s | IP %d.%d.%d.%d | BC %d]: %s\n",
						ID, m_vClientData[Index].StringID, Bytes.s_b1, Bytes.s_b2, Bytes.s_b3, Bytes.s_b4, ReceivedBytes, ByteData.Get(3));

					// echo
					if (bShouldEcho) Send(EServerPacketType::Echo, ByteData.Get(3), ByteData.Size(3), &Client);
				}
			}
		}
	}

private:
	bool Send(EServerPacketType eType, const char* Buffer, int BufferSize, SOCKADDR_IN* Client)
	{
		CByteData ByteData{};
		ByteData.Append((char)eType);
		ByteData.Append(Buffer, BufferSize);
		
		int SentBytes{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)Client, sizeof(SOCKADDR_IN)) };
		if (SentBytes > 0) return true;
		return false;
	}

public:
	void DisplayInfo()
	{
		auto& Bytes{ m_HostAddr.sin_addr.S_un.S_un_b };
		printf("========================================\n");
		printf(" Server IP: %d.%d.%d.%d\n", Bytes.s_b1, Bytes.s_b2, Bytes.s_b3, Bytes.s_b4);
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
		if (connect(Socket, (sockaddr*)&loopback, sizeof(loopback)))
		{
			printf("%s connect(): %d\n", KFailureHead, WSAGetLastError());
			return false;
		}

		int host_length{ (int)sizeof(m_HostAddr) };
		if (getsockname(Socket, (sockaddr*)&m_HostAddr, &host_length))
		{
			printf("%s getsockname(): %d\n", KFailureHead, WSAGetLastError());
			return false;
		}

		if (closesocket(Socket))
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
	static constexpr ID_t KIDOffset{ 100 };

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
	std::unordered_map<std::string, ID_t> m_umapClientStringIDtoIndex{};
	std::vector<SClientData> m_vClientData{};
};
