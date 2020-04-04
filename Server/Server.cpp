#include "Server.h"
#include "../Client/DoubleBufferedConsole.h"
#include <thread>
#include <chrono>

static CServer Server{};

static BOOL ConsoleEventHandler(DWORD event) {

	switch (event) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		Server.CleanUp();
		break;
	}
	return TRUE;
}

int main()
{
	SetConsoleCtrlHandler(ConsoleEventHandler, TRUE);
	Server.Open();

	CDoubleBufferedConsole Console{ 120, 36, "StringWorld SERVER" };
	Console.SetDefaultForeground(EForegroundColor::LightCyan);

	std::thread thr_net{
		[&]()
		{
			std::chrono::steady_clock Clock{};
			long long PrevUpdateTP{ Clock.now().time_since_epoch().count() };
			long long PrevSecondTP{ Clock.now().time_since_epoch().count() };
			while (true)
			{
				if (Server.IsClosing()) break;

				Server.Listen();

				long long Now{ Clock.now().time_since_epoch().count() };
				if (Now - PrevUpdateTP > 30'000'000) // 30 ms
				{
					Server.Update();
					PrevUpdateTP = Now;
				}
				if (Now - PrevSecondTP > 1'000'000'000) // 1 sec
				{
					Server.IncreaseTimeOutCounter();
					PrevSecondTP = Now;
				}
			}
		}
	};

	std::thread thr_input{
		[&]()
		{
			while (true)
			{
				if (Server.IsClosing()) break;

				if (Console.HitKey())
				{
					int Key{ Console.GetHitKey() };
					if (Key == VK_RETURN)
					{
						if (Console.GetCommand())
						{
							auto Command{ Console.GetLastCommand() };
							if (strncmp(Command, "QUIT", 4) == 0)
							{
								Server.Close();
								break;
							}
							else if (strncmp(Command, "CLIENTS", 7) == 0)
							{

							}
						}
					}
				}
			}
		}
	};

	while (true)
	{
		if (Server.IsClosing()) break;

		Console.Clear();

		Console.PrintBox(0, 0, 36, 4, ' ', EBackgroundColor::LightGray, EForegroundColor::LightGray);
		Console.PrintHString(2, 1, "   SERVER IP:");
		Console.PrintHString(2, 2, "SERVICE PORT:");
		Console.PrintHString(16, 1, Server.GetHostIPString());
		Console.PrintHString(16, 2, Server.GetServicePort());

		Console.PrintHString(2, 4, "Number of clients:");
		Console.PrintHString(21, 4, Server.GetCurrentClientCount());

		Console.PrintCommand(0, 35);

		Console.Render();
	}

	thr_net.join();
	thr_input.join();

	return 0;
}