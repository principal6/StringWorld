#pragma once

#include <vector>

using ID_t = short;
static constexpr ID_t KInvalidID{ -1 };

static constexpr int KStringIDSize{ 31 };
struct SStringID
{
	char String[KStringIDSize + 1]{};
};

static constexpr int KChatTextSize{ 63 };
struct SChatText
{
	bool bIsLocal{ false };
	char String[KChatTextSize + 1]{};
};

enum class EClientPacketType : char
{
	Enter,
	Leave,
	Input,
	Chat,
};

enum class EServerPacketType : char
{
	Echo,
	EnterPermission,
	Denial,
	Update,
	Chat,
};

enum class EInput : char
{
	Left,
	Right,
	Up,
	Down,
};

struct SClientDatum
{
	ID_t ID{ KInvalidID };
	short X{};
	short Y{};
	short Padding{};
};

class CByteData
{
public:
	CByteData() {}
	CByteData(const char* Input, int InputSize) { Append(Input, InputSize); }
	~CByteData() {}

public:
	void Clear()
	{
		m_vBytes.clear();
	}

	void Append(char Input)
	{
		size_t OldSize{ m_vBytes.size() };
		size_t NewSize{ OldSize + sizeof(Input) };
		if (NewSize > m_vBytes.size()) m_vBytes.resize(NewSize);
		memcpy(&m_vBytes[OldSize], &Input, sizeof(Input));
	}

	void Append(short Input)
	{
		size_t OldSize{ m_vBytes.size() };
		size_t NewSize{ OldSize + sizeof(Input) };
		if (NewSize > m_vBytes.size()) m_vBytes.resize(NewSize);
		memcpy(&m_vBytes[OldSize], &Input, sizeof(Input));
	}

	void Append(const void* Input, int InputSize)
	{
		if (!Input) return;
		if (InputSize == 0) return;

		size_t OldSize{ m_vBytes.size() };
		size_t NewSize{ OldSize + InputSize };
		if (NewSize > m_vBytes.size()) m_vBytes.resize(NewSize);
		memcpy(&m_vBytes[OldSize], Input, InputSize);
	}

	const char* Get() const { return &m_vBytes[0]; }
	const char* Get(size_t Offset) const { return &m_vBytes[Offset]; }
	int Size() const { return (int)m_vBytes.size(); }
	int Size(size_t Offset) const { return (int)(m_vBytes.size() - Offset); }

private:
	std::vector<char> m_vBytes{};
};
