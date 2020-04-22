#include "Server.h"
#include "../Client/SingleBufferedConsole.h"
#include <thread>
#include <chrono>

static CServer Server{ 9999, timeval{ 1, 0 } };

static BOOL ConsoleEventHandler(DWORD event) {

	switch (event) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		Server.Terminate();
		break;
	}
	return TRUE;
}

int main()
{
	SetConsoleCtrlHandler(ConsoleEventHandler, TRUE);

	CSingleBufferedConsole Console{ 120, 36, "StringWorld SERVER", ECommandLinePosition::Bottom };
	Console.SetDefaultForeground(EForegroundColor::LightCyan);

	std::thread ThrUpdate
	{
		[&]()
		{
			std::chrono::steady_clock Clock{};
			long long PrevUpdateTP{ Clock.now().time_since_epoch().count() };
			long long PrevSecondTP{ Clock.now().time_since_epoch().count() };
			while (true)
			{
				if (Server.IsTerminating()) break;

				Server.Receive();

				long long Now{ Clock.now().time_since_epoch().count() };
				if (Now - PrevUpdateTP > 16'000'000) // 16 ms
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

	std::thread ThrInput
	{
		[&]()
		{
			while (true)
			{
				if (Server.IsTerminating()) break;

				if (Console.HitKey())
				{
					if (Console.IsHitKey(VK_RETURN))
					{
						if (Console.ReadCommand())
						{
							auto Command{ Console.GetLastCommand() };
							if (Console.IsLastCommand("/quit"))
							{
								Server.Terminate();
								break;
							}
							else if (Console.IsLastCommand("/clients"))
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
		if (Server.IsTerminating()) break;

		Console.Clear();

		Console.PrintBox(0, 0, 36, 4, ' ', EBackgroundColor::LightGray, EForegroundColor::LightGray);
		Console.PrintHString(2, 1, "   SERVER IP:");
		Console.PrintHString(2, 2, "SERVICE PORT:");
		Console.PrintHString(16, 1, Server.GetHostIPString());
		Console.PrintHString(16, 2, Server.GetServicePort());

		Console.PrintHString(2, 4, "     Running time:");
		Console.PrintHString(2, 5, "Number of clients:");
		Console.PrintHString(2, 6, "     Update count:");
		Console.PrintHString(21, 4, Server.GetServerRunningTime());
		Console.PrintHString(21, 5, Server.GetCurrentClientCount());
		Console.PrintHString(21, 6, Server.GetUpdateCounter());

		Console.Render();
	}

	ThrUpdate.join();
	ThrInput.join();
	return 0;
}
