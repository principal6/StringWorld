#pragma once

#include <Windows.h>
#include <iostream>
#include <conio.h>

enum class EArrowKeys
{
	None,
	Up,
	Left,
	Right,
	Down
};

enum class EBackgroundColor
{
	Black = 0,
	DarkGray = BACKGROUND_INTENSITY,
	LightGray = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,
	White = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,

	Red = BACKGROUND_RED,
	Green = BACKGROUND_GREEN,
	Blue = BACKGROUND_BLUE,
	Cyan = BACKGROUND_GREEN | BACKGROUND_BLUE,
	Magenta = BACKGROUND_RED | BACKGROUND_BLUE,
	Yellow = BACKGROUND_RED | BACKGROUND_GREEN,

	LightRed = BACKGROUND_INTENSITY | BACKGROUND_RED,
	LightGreen = BACKGROUND_INTENSITY | BACKGROUND_GREEN,
	LightBlue = BACKGROUND_INTENSITY | BACKGROUND_BLUE,
	LightCyan = BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_BLUE,
	LightMagenta = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE,
	LightYellow = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN,
};

enum class EForegroundColor
{
	Black = 0,
	DarkGray = FOREGROUND_INTENSITY,
	LightGray = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
	White = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,

	Red = FOREGROUND_RED,
	Green = FOREGROUND_GREEN,
	Blue = FOREGROUND_BLUE,
	Cyan = FOREGROUND_GREEN | FOREGROUND_BLUE,
	Magenta = FOREGROUND_RED | FOREGROUND_BLUE,
	Yellow = FOREGROUND_RED | FOREGROUND_GREEN,

	LightRed = FOREGROUND_INTENSITY | FOREGROUND_RED,
	LightGreen = FOREGROUND_INTENSITY | FOREGROUND_GREEN,
	LightBlue = FOREGROUND_INTENSITY | FOREGROUND_BLUE,
	LightCyan = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
	LightMagenta = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
	LightYellow = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
};

class CDoubleBufferedConsole
{
public:
	CDoubleBufferedConsole(short Width, short Height, const char* Title) : m_Width{ Width }, m_Height{ Height } { StartUp(Title); }
	~CDoubleBufferedConsole() { CleanUp(); }

public:
	void Clear()
	{
		COORD Coord{ 0, 0 };
		DWORD Written{};
		WORD Attribute{ (WORD)((WORD)m_eClearBackground | (WORD)m_eDefaultForeground) };
		for (short Y = 0; Y < m_Height; ++Y)
		{
			Coord.Y = Y;
			FillConsoleOutputCharacterA(m_BackBuffer, ' ', m_Width, Coord, &Written);
			FillConsoleOutputAttribute(m_BackBuffer, Attribute, m_Width, Coord, &Written);
		}
	}

	void PrintChar(short X, short Y, char Char, EBackgroundColor eBackground, EForegroundColor eForeground)
	{
		DWORD Written{};
		COORD Coord{ X, Y };
		WORD Attribute{ (WORD)((WORD)eBackground | (WORD)eForeground) };
		FillConsoleOutputCharacterA(m_BackBuffer, Char, 1, Coord, &Written);
		FillConsoleOutputAttribute(m_BackBuffer, Attribute, 1, Coord, &Written);
	}

	void PrintChar(short X, short Y, char Char, EForegroundColor eForeground)
	{
		PrintChar(X, Y, Char, m_eClearBackground, eForeground);
	}

	void PrintChar(short X, short Y, char Char)
	{
		PrintChar(X, Y, Char, m_eClearBackground, m_eDefaultForeground);
	}

	void PrintHString(short X, short Y, const char* String, EBackgroundColor eBackground, EForegroundColor eForeground, int StringLength = -1)
	{
		DWORD Written{};
		COORD Coord{ X, Y };
		WORD Attribute{ (WORD)((WORD)eBackground | (WORD)eForeground) };
		if (StringLength == -1) StringLength = (int)strlen(String);
		WriteConsoleOutputCharacterA(m_BackBuffer, String, StringLength, Coord, &Written);
		FillConsoleOutputAttribute(m_BackBuffer, Attribute, StringLength, Coord, &Written);
	}

	void PrintHString(short X, short Y, const char* String, EForegroundColor eForeground, int StringLength = -1)
	{
		PrintHString(X, Y, String, m_eClearBackground, eForeground, StringLength);
	}

	void PrintHString(short X, short Y, const char* String, int StringLength = -1)
	{
		PrintHString(X, Y, String, m_eDefaultForeground, StringLength);
	}

	void PrintVString(short X, short Y, const char* String, EBackgroundColor eBackground, EForegroundColor eForeground, int StringLength = -1)
	{
		DWORD Written{};
		COORD Coord{ X, Y };
		WORD Attribute{ (WORD)((WORD)eBackground | (WORD)eForeground) };
		if (StringLength == -1) StringLength = (int)strlen(String);
		for (int i = 0; i < StringLength; ++i)
		{
			Coord.Y = Y + i;
			WriteConsoleOutputCharacterA(m_BackBuffer, &String[i], 1, Coord, &Written);
			FillConsoleOutputAttribute(m_BackBuffer, Attribute, 1, Coord, &Written);
		}
	}

	void PrintVString(short X, short Y, const char* String, EForegroundColor eForeground, int StringLength = -1)
	{
		PrintVString(X, Y, String, m_eClearBackground, eForeground, StringLength);
	}

	void PrintVString(short X, short Y, const char* String, int StringLength = -1)
	{
		PrintVString(X, Y, String, m_eDefaultForeground, StringLength);
	}

	void PrintBox(short X, short Y, short Width, short Height, char Char, EBackgroundColor eBackground, EForegroundColor eForeground)
	{
		FillHChar(X, Y, Char, Width, eBackground, eForeground);
		FillHChar(X, Y + Height - 1, Char, Width, eBackground, eForeground);
		FillVChar(X, Y, Char, Height, eBackground, eForeground);
		FillVChar(X + Width - 1, Y, Char, Height, eBackground, eForeground);
	}

	void FillBox(short X, short Y, short Width, short Height, char Char, EBackgroundColor eBackground, EForegroundColor eForeground)
	{
		for (short i = 0; i < Height; ++i)
		{
			FillHChar(X, Y + i, Char, Width, eBackground, eForeground);
		}
	}

	void FillHChar(short X, short Y, char Char, int Count, EBackgroundColor eBackground, EForegroundColor eForeground)
	{
		DWORD Written{};
		COORD Coord{ X, Y };
		WORD Attribute{ (WORD)((WORD)eBackground | (WORD)eForeground) };
		FillConsoleOutputCharacterA(m_BackBuffer, Char, Count, Coord, &Written);
		FillConsoleOutputAttribute(m_BackBuffer, Attribute, Count, Coord, &Written);
	}

	void FillHChar(short X, short Y, char Char, int Count, EForegroundColor eForeground)
	{
		FillHChar(X, Y, Char, Count, m_eClearBackground, eForeground);
	}

	void FillHChar(short X, short Y, char Char, int Count)
	{
		FillHChar(X, Y, Char, Count, m_eClearBackground, m_eDefaultForeground);
	}

	void FillVChar(short X, short Y, char Char, int Count, EBackgroundColor eBackground, EForegroundColor eForeground)
	{
		DWORD Written{};
		COORD Coord{ X, Y };
		WORD Attribute{ (WORD)((WORD)eBackground | (WORD)eForeground) };
		for (int i = 0; i < Count; ++i)
		{
			Coord.Y = Y + i;
			FillConsoleOutputCharacterA(m_BackBuffer, Char, 1, Coord, &Written);
			FillConsoleOutputAttribute(m_BackBuffer, Attribute, 1, Coord, &Written);
		}
	}

	void FillVChar(short X, short Y, char Char, int Count, EForegroundColor eForeground)
	{
		FillVChar(X, Y, Char, Count, m_eClearBackground, eForeground);
	}

	void FillVChar(short X, short Y, char Char, int Count)
	{
		FillVChar(X, Y, Char, Count, m_eClearBackground, m_eDefaultForeground);
	}

	void Render()
	{
		SetConsoleActiveScreenBuffer(m_BackBuffer);
		std::swap(m_FrontBuffer, m_BackBuffer);
	}

public:
	void SetClearBackground(EBackgroundColor eBackground)
	{
		m_eClearBackground = eBackground;
	}

	void SetDefaultForeground(EForegroundColor eForeground)
	{
		m_eDefaultForeground = eForeground;
	}

public:
	bool HitKey()
	{
		if (GetAsyncKeyState(VK_LMENU) < 0 && GetAsyncKeyState(VK_RETURN) < 0)
		{
			Reset();
			return false;
		}

		if (_kbhit())
		{
			m_HitKey = _getch();
			return true;
		}
		m_HitKey = 0;
		return false;
	}

	int GetHitKey()
	{
		return m_HitKey;
	}

	EArrowKeys GetHitArrowKey()
	{
		if (m_HitKey == 224)
		{
			m_HitKey = 0;
			int Key{ _getch() }; // arrow keys (up 72, left 75, right 77, down 80)
			if (Key == 72) return EArrowKeys::Up;
			if (Key == 75) return EArrowKeys::Left;
			if (Key == 77) return EArrowKeys::Right;
			if (Key == 80) return EArrowKeys::Down;
		}
		return EArrowKeys::None;
	}

public:
	bool GetCommand(short X, short Y)
	{
		COORD Coord{ X, Y };
		DWORD Written{};
		SetConsoleCursorPosition(m_FrontBuffer, Coord);
		WriteConsoleOutputCharacterA(m_FrontBuffer, "> ", 2, Coord, &Written);
		Coord.X += 2;

		SetConsoleCursorPosition(m_FrontBuffer, Coord);
		memset(m_CommandBuffer, 0, KCommandBufferSize);
		int ReadBytes{};
		int CurrentLogIndex{ m_CommandLogIndex };
		while (true)
		{
			if (_kbhit())
			{
				bool ShouldRead{ true };
				int Key{ _getch() };
				if (Key == 224)
				{
					int arrow{ _getch() };
					if (arrow == 72)
					{
						memset(m_CommandBuffer, 0, KCommandBufferSize);

						--CurrentLogIndex;
						if (CurrentLogIndex < 0) CurrentLogIndex = KCommandLogSize - 1;

						memcpy(m_CommandBuffer, m_CommandLog[CurrentLogIndex], strlen(m_CommandLog[CurrentLogIndex]));
						ReadBytes = (int)strlen(m_CommandBuffer);
						ShouldRead = false;
					}
					else if (arrow == 80)
					{
						memset(m_CommandBuffer, 0, KCommandBufferSize);

						++CurrentLogIndex;
						if (CurrentLogIndex >= KCommandLogSize) CurrentLogIndex = 0;

						memcpy(m_CommandBuffer, m_CommandLog[CurrentLogIndex], strlen(m_CommandLog[CurrentLogIndex]));
						ReadBytes = (int)strlen(m_CommandBuffer);
						ShouldRead = false;
					}
					else
					{
						continue;
					}
				}
				if (Key == VK_ESCAPE)
				{
					memset(m_CommandBuffer, 0, KCommandBufferSize);
					ReadBytes = 0;
					break;
				}
				if (Key == VK_RETURN) break;
				if (Key == VK_BACK)
				{
					if (ReadBytes)
					{
						--ReadBytes;
						m_CommandBuffer[ReadBytes] = 0;
					}
					ShouldRead = false;
				}

				if (ShouldRead)
				{
					m_CommandBuffer[ReadBytes] = Key;
					++ReadBytes;
				}
			}

			WriteConsoleOutputCharacterA(m_FrontBuffer, m_CommandBuffer, m_Width, Coord, &Written);
			SetConsoleCursorPosition(m_FrontBuffer, COORD{ (short)(Coord.X + ReadBytes), Y });
		}

		for (auto& ch : m_CommandBuffer)
		{
			if (ch == '\r') ch = 0;
			if (ch == '\n') ch = 0;
		}
		if (strlen(m_CommandBuffer))
		{
			strcpy_s(m_CommandLog[m_CommandLogIndex], m_CommandBuffer);
			++m_CommandLogIndex;
			if (m_CommandLogIndex >= KCommandLogSize) m_CommandLogIndex = 0;
		}

		return ReadBytes;
	}

	const char* GetLastCommand()
	{
		int LastCommandIndex{ m_CommandLogIndex - 1 };
		if (LastCommandIndex < 0) LastCommandIndex += KCommandLogSize;
		return m_CommandLog[LastCommandIndex];
	}

	// Prints log bottom up
	// X, Y, Width, Height are outer-box size
	void PrintCommandLog(short X, short Y, short Width, short Height)
	{
		X += 1;
		Y += 1;
		Width -= 2;
		Height -= 2;

		short _Height{ min(min(KCommandLogSize, m_Height), Height) };
		for (short i = 0; i < _Height; ++i)
		{
			short CurrentLogIndex{ m_CommandLogIndex - 1 - i };
			if (CurrentLogIndex < 0) CurrentLogIndex += KCommandLogSize;
			short _Y{ max(Y + _Height - i - 1, Y) };

			PrintHString(X, _Y, m_CommandLog[CurrentLogIndex], m_eClearBackground, EForegroundColor::LightYellow,
				min(Width, (int)(strlen(m_CommandLog[CurrentLogIndex]))));
		}
	}

private:
	void StartUp(const char* Title)
	{
		m_FrontBuffer = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
		m_BackBuffer = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);

		SetConsoleTitleA(Title);

		Reset();
	}

	void Reset()
	{
		CONSOLE_SCREEN_BUFFER_INFOEX BufferInfo{};
		BufferInfo.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
		GetConsoleScreenBufferInfoEx(m_FrontBuffer, &BufferInfo);
		BufferInfo.srWindow.Right = BufferInfo.dwSize.X = m_Width;
		BufferInfo.srWindow.Bottom = BufferInfo.dwSize.Y = m_Height;
		SetConsoleScreenBufferInfoEx(m_FrontBuffer, &BufferInfo);
		SetConsoleScreenBufferInfoEx(m_BackBuffer, &BufferInfo);
	}

	void CleanUp()
	{
		CloseHandle(m_BackBuffer);
		CloseHandle(m_FrontBuffer);
	}

private:
	short m_Width{};
	short m_Height{};
	HANDLE m_FrontBuffer{};
	HANDLE m_BackBuffer{};

private:
	EBackgroundColor m_eClearBackground{};
	EForegroundColor m_eDefaultForeground{ EForegroundColor::White };

private:
	int m_HitKey{};

private:
	static constexpr short KCommandBufferSize{ 200 };
	static constexpr short KCommandLogSize{ 30 };

private:
	char m_CommandBuffer[KCommandBufferSize]{};
	char m_CommandLog[KCommandLogSize][KCommandBufferSize]{};
	short m_CommandLogIndex{};
};
