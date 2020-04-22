#pragma once

#include "SharedDefinitions.h"
#include "UDPServerBase.h"

#include <unordered_map>
#include <mutex>

struct SInternalClientData
{
	SOCKADDR_IN Address{};
	SStringID StringID{};
	short TimeOutCounter{};
};

class CServer : public CUDPServerBase
{
public:
	CServer(USHORT Port, timeval TimeOut) : CUDPServerBase(Port, TimeOut) 
	{
		char IP[16]{};
		inet_ntop(AF_INET, &m_HostAddr.sin_addr, IP, 16);
		m_HostIPString = IP;

		m_vUpdateData.emplace_back();
		m_vUpdateData[0].ID = ((char)EServerPacketType::Update);
	}
	virtual ~CServer() {}

public:
	bool Receive(bool bShouldEcho = false)
	{
		int ReceivedByteCount{};
		SOCKADDR_IN ClientAddr{};
		if (_Receive(&ReceivedByteCount, &ClientAddr))
		{
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
							++m_ClientCount;

							if (NewClientID >= GetClientVectorSize())
							{
								m_vUpdateData.emplace_back();
								m_vClientInetrnalData.emplace_back();
							}
							GetClientData(NewClientID).ID = NewClientID;
							m_vClientInetrnalData[NewClientID].Address = ClientAddr;
							memcpy(&m_vClientInetrnalData[NewClientID].StringID, StringID, KStringIDSize); // Save StringID

							m_umapClientStringIDtoID[StringID] = NewClientID;
							m_umapClientIDtoIP[NewClientID] = ClientAddr.sin_addr.S_un.S_addr;
						}
						m_Mutex.unlock();

						CByteData IDBytes{};
						IDBytes.Append(NewClientID);
						SendTo(EServerPacketType::EnterPermission, IDBytes.Get(), IDBytes.Size(), &ClientAddr);
					}
					else
					{
						SendTo(EServerPacketType::Denial, "Nickname collision occured.", 28, &ClientAddr);
					}
				}
				else
				{
					if (m_ClientCount == 0) return false;

					ID_t ClientID{};
					memcpy(&ClientID, ByteData.Get(1), sizeof(ID_t));
					if (m_umapClientIDtoIP.find(ClientID) == m_umapClientIDtoIP.end()) return false; // Non-existing ID
					if (m_umapClientIDtoIP.at(ClientID) != ClientAddr.sin_addr.S_un.S_addr) return false; // ID-IP validation

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
								++GetClientData(ClientID).X;
							}
							else if (WithoutHeader[0] == (char)EInput::Left)
							{
								--GetClientData(ClientID).X;
							}
							else if (WithoutHeader[0] == (char)EInput::Up)
							{
								--GetClientData(ClientID).Y;
							}
							else if (WithoutHeader[0] == (char)EInput::Down)
							{
								++GetClientData(ClientID).Y;
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
							SendTo(EServerPacketType::Chat, Chat.Get(), Chat.Size(), &ClientInternal.Address);
						}
						break;
					}
					default:
						// echo
						if (bShouldEcho) SendTo(EServerPacketType::Echo, WithoutHeader, WithoutHeaderSize, &ClientAddr);
						break;
					}
				}
			}
			return true;
		}
		return false;
	}

	// Update once per tick
	bool Update()
	{
		if (m_ClientCount <= 0) return false;

		m_vUpdateData[0].X = m_ClientCount;
		m_vUpdateData[0].Y = (short)GetClientVectorSize();

		// Broadcast updated data to all the clients
		size_t SentCount{};
		for (size_t i = 0; i < m_vClientInetrnalData.size(); ++i)
		{
			auto& Client{ GetClientData((short)i) };
			if (Client.ID == KInvalidID) continue;
			_SendTo(&m_vClientInetrnalData[i].Address, (const char*)&m_vUpdateData[0], GetUpdateDataByteCount());
			++SentCount;
		}

		if (SentCount)
		{
			++m_UpdateCounter;
			return true;
		}
		return false;
	}

public:
	void IncreaseTimeOutCounter()
	{
		++m_ServerRunningTime;
		for (size_t i = 0; i < m_vClientInetrnalData.size(); ++i)
		{
			auto& Client{ GetClientData((short)i) };
			if (Client.ID == KInvalidID) continue;

			auto& ClientInternal{ m_vClientInetrnalData[i] };
			++ClientInternal.TimeOutCounter;
			if (ClientInternal.TimeOutCounter >= KClientTimeOutTick) KickOut(Client.ID);
		}
	}

	void KickOut(ID_t ID)
	{
		if (GetClientData(ID).ID == KInvalidID) return;

		m_Mutex.lock();
		{
			m_umapClientStringIDtoID.erase(m_vClientInetrnalData[ID].StringID.String);
			m_umapClientIDtoIP.erase(ID);
			GetClientData(ID).ID = KInvalidID;
			--m_ClientCount;
		}
		m_Mutex.unlock();
	}

protected:
	bool SendTo(EServerPacketType eType, const char* Buffer, int BufferSize, SOCKADDR_IN* ClientAddr)
	{
		CByteData ByteData{};
		ByteData.Append((char)eType);
		ByteData.Append(Buffer, BufferSize);
		return _SendTo(ClientAddr, ByteData.Get(), ByteData.Size());
	}

private:
	const SClientDatum& GetClientData(ID_t ClientID) const
	{
		return m_vUpdateData[ClientID + 1];
	}

	SClientDatum& GetClientData(ID_t ClientID)
	{
		return m_vUpdateData[ClientID + 1];
	}

	int GetClientVectorSize() const
	{
		return (int)(m_vUpdateData.size() - 1);
	}

	int GetUpdateDataByteCount() const
	{
		return (int)(m_vUpdateData.size() * sizeof(SClientDatum));
	}

	short _GetNextClientID() const
	{
		short NextID{ KInvalidID };
		for (short i = 0; i < (short)GetClientVectorSize(); ++i)
		{
			if (GetClientData(i).ID == KInvalidID)
			{
				NextID = i;
				break;
			}
		}
		if (NextID == KInvalidID) NextID = (short)GetClientVectorSize();
		return NextID;
	}

public:	
	const std::string& GetHostIPString() const { return m_HostIPString; }
	u_short GetServicePort() const { return ntohs(m_HostAddr.sin_port); }
	short GetCurrentClientCount() const { return m_ClientCount; }
	size_t GetUpdateCounter() const { return m_UpdateCounter; }
	size_t GetServerRunningTime() const { return m_ServerRunningTime; }

protected:
	static constexpr short KClientTimeOutTick{ 600 };

protected:
	std::string m_HostIPString{};

protected:
	size_t m_UpdateCounter{};
	size_t m_ServerRunningTime{};

// Critical section
private:
	std::mutex m_Mutex{};
	std::unordered_map<ID_t, ULONG> m_umapClientIDtoIP{}; // For ID-IP validation
	std::unordered_map<std::string, ID_t> m_umapClientStringIDtoID{}; // For StringID collision test
	std::vector<SInternalClientData> m_vClientInetrnalData{};
	short m_ClientCount{};
	std::vector<SClientDatum> m_vUpdateData{};
};
