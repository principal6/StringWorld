#pragma once

#include <vector>

using ID_t = short;
static constexpr int KClientStringIDMaxSize{ 31 };

enum class EClientPacketType
{
	Enter,
	Input,
	Talk,
	Leave,
};

enum class EServerPacketType
{
	Echo,
	EnterPermission,
	ClientsData,
	Talk,
	Denial,
};

class CByteData
{
public:
	CByteData() {}
	CByteData(const char* Input, int InputSize) { Append(Input, InputSize); }
	~CByteData() {}

public:
	void Append(char Input)
	{
		size_t OldSize{ m_vBytes.size() };
		m_vBytes.resize(OldSize + sizeof(Input));
		memcpy(&m_vBytes[OldSize], &Input, sizeof(Input));
	}

	void Append(short Input)
	{
		size_t OldSize{ m_vBytes.size() };
		m_vBytes.resize(OldSize + sizeof(Input));
		memcpy(&m_vBytes[OldSize], &Input, sizeof(Input));
	}

	void Append(const char* Input, int InputSize)
	{
		if (!Input) return;
		if (InputSize == 0) return;
		size_t OldSize{ m_vBytes.size() };
		m_vBytes.resize(OldSize + InputSize);
		memcpy(&m_vBytes[OldSize], Input, InputSize);
	}

	const char* Get() const { return &m_vBytes[0]; }
	const char* Get(size_t Offset) const { return &m_vBytes[Offset]; }
	int Size() const { return (int)m_vBytes.size(); }
	int Size(size_t Offset) const { return (int)(m_vBytes.size() - Offset); }

private:
	std::vector<char> m_vBytes{};
};