#pragma once
\
#include "../Server/SharedDefinitions.h"
#include "UDPClientBase.h"

class CClient : public CUDPClientBase
{
public:
	CClient(const char* ServerIP, u_short Port, timeval TimeOut) : CUDPClientBase(ServerIP, Port, TimeOut) {}
	virtual ~CClient() {}

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
			if (_Receive(&ReceivedByteCount))
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

	bool Leave()
	{
		if (Send(EClientPacketType::Leave, '!'))
		{
			Terminate();
			return true;
		}
		return false;
	}

	void Input(EInput eInput)
	{
		Send(EClientPacketType::Input, (char)eInput);
	}

	void Chat(const char* Content)
	{
		int ContentLength{ (int)strlen(Content) };
		if (ContentLength == 0) return;

		_LogChat(Content, true);

		Send(EClientPacketType::Chat, m_vChatLog.back().String, KChatTextSize + 1);
	}

protected:
	bool Send(EClientPacketType eType, const char* Buffer, int BufferSize)
	{
		CByteData ByteData{};
		ByteData.Append((char)eType);
		// Append ID given by ther server when entered.
		if (eType != EClientPacketType::Enter) ByteData.Append(m_MyDatum.ID);
		ByteData.Append(Buffer, BufferSize);

		int SentByteCount{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)&m_ServerAddr, sizeof(m_ServerAddr)) };
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

		int SentByteCount{ sendto(m_Socket, ByteData.Get(), ByteData.Size(), 0, (sockaddr*)&m_ServerAddr, sizeof(m_ServerAddr)) };
		if (SentByteCount > 0) return true;
		return false;
	}

public:
	bool Receive()
	{
		int ReceivedByteCount{};
		if (_Receive(&ReceivedByteCount))
		{
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
				short ClientCount{};
				memcpy(&ClientCount, &ReceivedData[2], sizeof(ClientCount));

				short ClientVectorSize{};
				memcpy(&ClientVectorSize, &ReceivedData[4], sizeof(ClientVectorSize));

				m_vClientData.resize(ClientVectorSize);
				memcpy(&m_vClientData[0], &ReceivedData[sizeof(SClientDatum)], ClientVectorSize * sizeof(SClientDatum));

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
		return false;
	}

protected:
	void _LogChat(const char* Content, bool bIsMyChat = false)
	{
		std::string _Content{ (bIsMyChat) ? m_MyStringID.String : Content };
		SChatText Chat{};
		if (bIsMyChat)
		{
			Chat.bIsLocal = true;

			_Content += ": ";
			_Content += Content;
		}
		memcpy(Chat.String, &_Content[0], min((int)_Content.size(), KChatTextSize));
		m_vChatLog.emplace_back(Chat);
	}

public:
	const SClientDatum& GetMyDatum() const { return m_MyDatum; }
	const SStringID& GetMyStringID() const { return m_MyStringID; }
	const std::vector<SClientDatum>& GetClientData() const { return m_vClientData; }
	bool HasChatLog() const { return m_vChatLog.size(); }
	const std::vector<SChatText>& GetChatLog() const { return m_vChatLog; }

private:
	SClientDatum m_MyDatum{};
	SStringID m_MyStringID{};
	std::vector<SClientDatum> m_vClientData{};

private:
	std::vector<SChatText> m_vChatLog{};
};
