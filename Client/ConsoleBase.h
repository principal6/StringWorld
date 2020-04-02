#pragma once

#include <Windows.h>
#include <conio.h>
#include <iostream>

enum class EArrowKeys
{
	None,
	Up,
	Left,
	Right,
	Down
};

enum class EFonts
{
	Fixedsys,
	System,
	Terminal,
	±¼¸²Ã¼,
	µ¸¿òÃ¼
};

static const wchar_t KFontNames[5][30]
{
	L"Fixedsys",
	L"System",
	L"Terminal",
	L"±¼¸²Ã¼",
	L"µ¸¿òÃ¼"
};

class CConsoleBase
{
public:
	CConsoleBase(int Width, int Height, const char* Title) : m_Width{ Width }, m_Height{ Height } 
	{
		m_Width = max(m_Width, 5);
		m_Height = max(m_Height, 5);
		Initialize(Title);
	}
	~CConsoleBase() 
	{
		Destroy(); 
	}

public:
	void Clear(char Char = 0x20)
	{
		memset(m_Buffer, Char, m_BufferSize - 1);
		for (int y = 0; y < m_Height; ++y)
		{
			int line_head{ y * (m_Width + 1) };
			m_Buffer[line_head + m_Width] = '\n';
		}
		m_Buffer[m_BufferSize - 1] = 0;
	}

	void PrintString(int X, int Y, const char* String)
	{
		X = max(min(X, m_Width), 0);
		Y = max(min(Y, m_Height), 0);
		
		int length{ (int)strlen(String) };
		if (X + length > m_Width) length = m_Width - X;
		memcpy(m_Buffer + Y * (m_Width + 1) + X, String, length);
	}

	void PrintString(int X, int Y, const char* String, int MaxLength)
	{
		X = max(min(X, m_Width), 0);
		Y = max(min(Y, m_Height), 0);

		int length{ min((int)strlen(String), MaxLength) };
		if (X + length > m_Width) length = m_Width - X;
		memcpy(m_Buffer + Y * (m_Width + 1) + X, String, length);
	}

	void PrintString(int X, int Y, char Char, int Count)
	{
		X = max(min(X, m_Width), 0);
		Y = max(min(Y, m_Height), 0);

		int length{ Count };
		if (X + length > m_Width) length = m_Width - X;
		memset(m_Buffer + Y * (m_Width + 1) + X, Char, length);
	}

	void PrintVString(int X, int Y, const char* String)
	{
		X = max(min(X, m_Width), 0);
		Y = max(min(Y, m_Height), 0);
		int length{ (int)strlen(String) };
		for (int i = 0; i < length; ++i)
		{
			if (i + Y >= m_Height) return;
			m_Buffer[(i + Y) * (m_Width + 1) + X] = String[i];
		}
	}

	void PrintVString(int X, int Y, char Char, int Count)
	{
		X = max(min(X, m_Width), 0);
		Y = max(min(Y, m_Height), 0);
		int length{ Count };
		for (int i = 0; i < length; ++i)
		{
			if (i + Y >= m_Height) return;
			m_Buffer[(i + Y) * (m_Width + 1) + X] = Char;
		}
	}

	// Width and Height imply inner box size.
	void PrintBox(int X, int Y, int Width, int Height, char CharHorz, char CharVert)
	{
		PrintString(X + 1, Y, CharHorz, Width);
		PrintString(X + 1, Y + Height + 1, CharHorz, Width);
		PrintVString(X, Y + 1, CharVert, Height);
		PrintVString(X + Width + 1, Y + 1, CharVert, Height);
	}

	void PrintChar(int X, int Y, char Char)
	{
		if (X < 0 || X >= m_Width || Y < 0 || Y >= m_Height) return;
		m_Buffer[Y * (m_Width + 1) + X] = Char;
	}

	// prints log bottom up
	void PrintLog(short X, short Y, int Width, int Height)
	{
		int height{ min(min(KLogCount, m_Height), Height) };
		for (int i = 0; i < height; ++i)
		{
			int line_index{ m_LogIndex - 1 - i };
			if (line_index < 0) line_index += KLogCount;
			int _Y{ max(Y + height - i - 1, Y) };

			PrintString(X, _Y, m_Log[line_index], Width);
		}
	}

	void Render()
	{
		SetConsoleCursorPosition(m_hConsoleOutput, { 0, 0 });
		printf(m_Buffer);

		Clear();
	}

	void DrawPoint(int X, int Y, COLORREF Color)
	{
		HDC hDC{ GetDC(m_hWnd) };

		SetPixelV(hDC, X, Y, Color);

		ReleaseDC(m_hWnd, hDC);
	}

	void DrawBox(int X, int Y, int Width, int Height, COLORREF Color)
	{
		HDC hDC{ GetDC(m_hWnd) };

		for (int x = 0; x < Width; ++x)
		{
			for (int y = 0; y < Height; ++y)
			{
				SetPixelV(hDC, X + x, Y + y, Color);
			}
		}

		ReleaseDC(m_hWnd, hDC);
	}

public:
	bool HitKey()
	{
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
			int key{ _getch() }; // arrow keys (up 72, left 75, right 77, down 80)
			if (key == 72) return EArrowKeys::Up;
			if (key == 75) return EArrowKeys::Left;
			if (key == 77) return EArrowKeys::Right;
			if (key == 80) return EArrowKeys::Down;
		}
		return EArrowKeys::None;
	}

	// non-blocking input
	// returns 'true' if there has been new input, 'false' if none.
	bool GetCommand(short X, short Y)
	{
		CONSOLE_CURSOR_INFO cci{ sizeof(CONSOLE_CURSOR_INFO), true };
		SetConsoleCursorInfo(m_hConsoleOutput, &cci);
		
		SetConsoleCursorPosition(m_hConsoleOutput, { X, Y });
		printf("> ");

		memset(m_Command, 0, KLineLength);
		int read_bytes{};
		int log_index{ m_LogIndex };
		while (true)
		{
			if (_kbhit())
			{
				bool should_read{ true };
				int ch{ _getch() };
				if (ch == 224)
				{
					int arrow{ _getch() };
					if (arrow == 72)
					{
						memset(m_Command, 0, KLineLength);

						--log_index;
						if (log_index < 0) log_index = KLogCount - 1;

						memcpy(m_Command, m_Log[log_index], strlen(m_Log[log_index]));
						read_bytes = (int)strlen(m_Command);
						should_read = false;
					}
					else if (arrow == 80)
					{
						memset(m_Command, 0, KLineLength);

						++log_index;
						if (log_index >= KLogCount) log_index = 0;

						memcpy(m_Command, m_Log[log_index], strlen(m_Log[log_index]));
						read_bytes = (int)strlen(m_Command);
						should_read = false;
					}
					else
					{
						continue;
					}
				}
				if (ch == VK_ESCAPE)
				{
					memset(m_Command, 0, KLineLength);
					read_bytes = 0;
					break;
				}
				if (ch == VK_RETURN) break;
				if (ch == VK_BACK)
				{
					if (read_bytes)
					{
						--read_bytes;
						m_Command[read_bytes] = 0;
					}
					should_read = false;
				}
				
				if (should_read)
				{
					m_Command[read_bytes] = ch;
					++read_bytes;
				}

				SetConsoleCursorPosition(m_hConsoleOutput, { X, Y });
				printf(">                                      ");
				SetConsoleCursorPosition(m_hConsoleOutput, { X + 2, Y });
				printf(m_Command);
			}
		}

		for (auto& ch : m_Command)
		{
			if (ch == '\r') ch = 0;
			if (ch == '\n') ch = 0;
		}
		if (strlen(m_Command))
		{
			strcpy_s(m_Log[m_LogIndex], m_Command);
			++m_LogIndex;
			if (m_LogIndex >= KLogCount) m_LogIndex = 0;
		}

		cci.bVisible = false;
		SetConsoleCursorInfo(m_hConsoleOutput, &cci);

		return read_bytes;
	}

	const char* GetLastCommand()
	{
		int log_index{ m_LogIndex - 1 };
		if (log_index < 0) log_index += KLogCount;
		return m_Log[log_index];
	}

public:
	void Reset()
	{
		CONSOLE_CURSOR_INFO cci{ sizeof(CONSOLE_CURSOR_INFO), false };
		SetConsoleCursorInfo(m_hConsoleOutput, &cci);

		CONSOLE_SCREEN_BUFFER_INFOEX csbi{};
		csbi.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
		GetConsoleScreenBufferInfoEx(m_hConsoleOutput, &csbi);

		csbi.srWindow.Right = m_Width;
		csbi.srWindow.Bottom = m_Height + 1;
		csbi.dwSize.X = m_Width;
		csbi.dwSize.Y = m_Height;
		csbi.ColorTable[0] = m_ColorBackground; // background
		csbi.ColorTable[7] = m_ColorFont; // font
		SetConsoleScreenBufferInfoEx(m_hConsoleOutput, &csbi);
	}

	void SetFont(EFonts eFont, int SizeX = 0, int SizeY = 16)
	{
		CONSOLE_FONT_INFOEX cfi{};
		cfi.cbSize = sizeof(CONSOLE_FONT_INFOEX);
		cfi.dwFontSize.X = SizeX;
		cfi.dwFontSize.Y = SizeY;
		cfi.FontWeight = FW_NORMAL;
		cfi.FontFamily = FF_DONTCARE;
		wcscpy_s(cfi.FaceName, KFontNames[(int)eFont]);
		SetCurrentConsoleFontEx(m_hConsoleOutput, FALSE, &cfi);
	}

	void SetColorTheme(COLORREF Background, COLORREF Font)
	{
		m_ColorBackground = Background;
		m_ColorFont = Font;

		Reset();
	}

private:
	void Initialize(const char* Title)
	{
		m_hWnd = GetConsoleWindow();

		SetConsoleTitleA(Title);

		m_hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		
		Reset();

		m_BufferSize = m_Height * (m_Width + 1);
		m_Buffer = new char[m_BufferSize];
		
		Clear();
	}

	void Destroy()
	{
		if (m_Buffer) delete[] m_Buffer;
	}

private:
	static constexpr int KLineLength{ 150 };
	static constexpr int KLogCount{ 20 };
	
private:
	HANDLE m_hConsoleOutput{};
	HWND m_hWnd{};
	int m_Width{};
	int m_Height{};
	int m_BufferSize{};
	char* m_Buffer{};

private:
	COLORREF m_ColorBackground{};
	COLORREF m_ColorFont{};

private:
	int m_HitKey{};
	char m_Command[KLineLength]{};
	char m_Log[KLogCount][KLineLength]{};
	int m_LogIndex{};
};