#include "Client.h"
#include "ConsoleBase.h"
#include <string>
#include <thread>

int main()
{
	CClient Client{ 9999, timeval{ 2, 0 } };
	char Buffer[2048]{};

	std::cout << "Enter Server IP: ";
	std::cin >> Buffer;
	if (!Client.Connect(Buffer))
	{
		std::cout << "Failed to connect to the server.\n";
		return 0;
	}

	while (true)
	{
		std::cout << "Enter your Nickname: ";
		std::cin >> Buffer;

		if (strlen(Buffer) <= 31)
		{
			if (Client.IsTimedOut()) return 0;
			if (Client.Enter(Buffer))
			{
				break;
			}
		}
		else
		{
			std::cout << "[SYSTEM] Nickname cannot be longer than 31 letters.\n";
		}
	}

	std::thread thr_listen{
		[&]()
		{
			while (true)
			{
				if (Client.IsTimedOut()) break;
				Client.Listen();
			}
		}
	};

	std::thread thr_send{
		[&]()
		{
			while (true)
			{
				fgets(Buffer, 2048, stdin);
				for (auto& ch : Buffer)
				{
					if (ch == '\n') ch = 0;
				}
				if (strncmp(Buffer, "QUIT", 4) == 0) break;
				Client.Talk(Buffer);
			}
		}
	};

	thr_listen.join();
	thr_send.join();

	return 0;
}

int fmain()
{
	CConsoleBase Base{ 85, 14, "BASE" };
	Base.SetFont(EFonts::Terminal, 8, 12);
	Base.SetColorTheme(RGB(20, 20, 20), RGB(0, 180, 80));
	int X{}, Y{};
	
	while (true)
	{
		if (GetAsyncKeyState(VK_LMENU) < 0 && GetAsyncKeyState(VK_RETURN) < 0)
		{
			Base.Reset();
		}
		if (Base.HitKey())
		{
			EArrowKeys ArrowKey{ Base.GetHitArrowKey() };
			if (ArrowKey == EArrowKeys::Right) ++X;
			if (ArrowKey == EArrowKeys::Left) --X;
			if (ArrowKey == EArrowKeys::Down) ++Y;
			if (ArrowKey == EArrowKeys::Up) --Y;

			int Key{ Base.GetHitKey() };
			if (Key == VK_ESCAPE)
			{
				break;
			}
			else if (Key == VK_RETURN)
			{
				if (Base.GetCommand(0, 13))
				{
					std::string Command{ Base.GetLastCommand() };
				}
			}
		}

		Base.PrintBox(42, 0, 32, 10, '-', '|');
		Base.PrintLog(43, 1, 32, 10);

		Base.PrintBox(0, 0, 40, 10, '-', '|');
		Base.PrintChar(X, Y, '@');

		Base.PrintString(78, 0, "[C]");
		Base.PrintString(78, 1, "[I]");
		Base.PrintString(78, 2, "[Q]");
		Base.PrintString(78, 5, ("X:" + std::to_string(X)).c_str());
		Base.PrintString(78, 6, ("Y:" + std::to_string(Y)).c_str());

		Base.Render();
	}

	return 0;
}