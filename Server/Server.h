#pragma once

#pragma comment (lib, "WS2_32.lib")

#include <WinSock2.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include "SharedDefinitions.h"

struct SInternalClientData
{
	SOCKADDR_IN Address{};
	SStringID StringID{};
	short TimeOutCounter{};
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
						ID_t NewClientID{ _GetNextClientID() };

						m_Mutex.lock();
						{
							++m_CurrentClientCount;

							if (NewClientID >= m_vClientData.size())
							{
								m_vClientData.emplace_back();
								m_vClientInetrnalData.emplace_back();
							}
							m_vClientData[NewClientID].ID = NewClientID;
							m_vClientInetrnalData[NewClientID].Address = Client;
							memcpy(&m_vClientInetrnalData[NewClientID].StringID, StringID, KStringIDSize); // Save StringID

							m_umapClientStringIDtoID[StringID] = NewClientID;
							m_umapClientIDtoIP[NewClientID] = Client.sin_addr.S_un.S_addr;
						}
						m_Mutex.unlock();

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
					if (m_CurrentClientCount == 0) return;

					ID_t ClientID{};
					memcpy(&ClientID, ByteData.Get(1), sizeof(ID_t));
					if (m_umapClientIDtoIP.find(ClientID) == m_umapClientIDtoIP.end()) return; // Non-existing ID
					if (m_umapClientIDtoIP.at(ClientID) != Client.sin_addr.S_un.S_addr) return; // ID-IP validation

					m_vClientInetrnalData[ClientID].TimeOutCounter = 0; // Initialize time-out counter
					
					auto WithoutHeader{ ByteData.Get(3) };
					auto WithoutHeaderSize{ ByteData.Size(3) };
					switch (eType)
					{
					case EClientPacketType::Leave:
						KickOut(ClientID);
						break;
					case EClientPacketType::Input:
					{
						m_Mutex.lock();
						{
							if (WithoutHeader[0] == (char)EInput::Right)
							{
								++m_vClientData[ClientID].X;
							}
							else if (WithoutHeader[0] == (char)EInput::Left)
							{
								--m_vClientData[ClientID].X;
							}
							else if (WithoutHeader[0] == (char)EInput::Up)
							{
								--m_vClientData[ClientID].Y;
							}
							else if (WithoutHeader[0] == (char)EInput::Down)
							{
								++m_vClientData[ClientID].Y;
							}
						}
						m_Mutex.unlock();
						break;
					}
					case EClientPacketType::Chat:
					{
						CByteData Chat{};
						Chat.Append(ClientID);
						Chat.Append(WithoutHeader, WithoutHeaderSize);
						// Broadcast chat to all the clients
						for (auto& ClientInternal : m_vClientInetrnalData)
						{
							Send(EServerPacketType::Chat, Chat.Get(), Chat.Size(), &ClientInternal.Address);
						}
						break;
					}
					default:
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
		m_UpdateByteData.Clear();
		m_UpdateByteData.Append((char)EServerPacketType::Update);
		m_UpdateByteData.Append((short)m_vClientData.size()); // Client count
		m_UpdateByteData.Append(&m_vClientData[0], (int)(m_vClientData.size() * sizeof(SClientDatum)));
		
		// Broadcast updated data to all the clients
		for (size_t i = 0; i < m_vClientInetrnalData.size(); ++i)
		{
			auto& Client{ m_vClientData[i] };
			if (Client.ID == KInvalidID) continue;

			auto& ClientInternal{ m_vClientInetrnalData[i] };
			sendto(m_Socket, m_UpdateByteData.Get(), m_UpdateByteData.Size(), 0, (sockaddr*)&ClientInternal, sizeof(ClientInternal));
		}

		return true;
	}

	void IncreaseTimeOutCounter()
	{
		for (size_t i = 0; i < m_vClientInetrnalData.size(); ++i)
		{
			auto& Client{ m_vClientData[i] };
			if (Client.ID == KInvalidID) continue;

			auto& ClientInternal{ m_vClientInetrnalData[i] };
			++ClientInternal.TimeOutCounter;
			if (ClientInternal.TimeOutCounter >= KClientTimeOutTick) KickOut(Client.ID);
		}
	}

	void KickOut(ID_t ID)
	{
		if (m_vClientData[ID].ID == KInvalidID) return;
		
		m_Mutex.lock();
		{
			m_umapClientStringIDtoID.erase(m_vClientInetrnalData[ID].StringID.String);
			m_umapClientIDtoIP.erase(ID);
			m_vClientData[ID].ID = KInvalidID;
			--m_CurrentClientCount;
		}
		m_Mutex.unlock();
	}

private:
	short _GetNextClientID() const
	{
		short NextID{ KInvalidID };
		for (short i = 0; i < (short)m_vClientData.size(); ++i)
		{
			if (m_vClientData[i].ID == KInvalidID)
			{
				NextID = i;
				break;
			}
		}
		if (NextID == KInvalidID) NextID = (short)m_vClientData.size();
		return NextID;
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
	void _DisplayInfo()
	{
		auto& IPv4{ m_HostAddr.sin_addr.S_un.S_un_b };
		printf("========================================\n");
		printf(" Server IP: %d.%d.%d.%d\n", IPv4.s_b1, IPv4.s_b2, IPv4.s_b3, IPv4.s_b4);
		printf(" Service Port: %d\n", KPort);
		printf("========================================\n");
	}

	const std::string& GetHostIPString() const { return m_HostIPString; }
	u_short GetServicePort() const { return KPort; }
	short GetCurrentClientCount() const { return m_CurrentClientCount; }

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

		m_bGotHost = _GetHostIP();
	}

	bool _GetHostIP()
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
		
		m_HostIPString.clear();
		m_HostIPString += std::to_string((int)(m_HostAddr.sin_addr.S_un.S_un_b.s_b1)) + ".";
		m_HostIPString += std::to_string((int)(m_HostAddr.sin_addr.S_un.S_un_b.s_b2)) + ".";
		m_HostIPString += std::to_string((int)(m_HostAddr.sin_addr.S_un.S_un_b.s_b3)) + ".";
		m_HostIPString += std::to_string((int)(m_HostAddr.sin_addr.S_un.S_un_b.s_b4));

		return true;
	}

public:
	void CleanUp()
	{
		Close();

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
	static constexpr u_short KPort{ 9999 };
	static constexpr short KClientTimeOutTick{ 600 };

private:
	bool m_bStartUp{};
	bool m_bGotHost{};
	bool m_bOpen{};
	bool m_bClosing{};

private:
	SOCKET m_Socket{};
	bool m_bSocketCreated{};
	SOCKADDR_IN m_HostAddr{};
	std::string m_HostIPString{};

private:
	char m_Buffer[KBufferSize]{};

// Critical section
private:
	short m_CurrentClientCount{};
	std::unordered_map<ID_t, ULONG> m_umapClientIDtoIP{}; // For ID-IP validation
	std::unordered_map<std::string, ID_t> m_umapClientStringIDtoID{}; // For StringID collision test
	std::vector<SInternalClientData> m_vClientInetrnalData{};
	std::vector<SClientDatum> m_vClientData{};
	std::mutex m_Mutex{};

private:
	CByteData m_UpdateByteData{};
};
