#include "Client.h"
#include "SingleBufferedConsole.h"
#include <thread>

static CClient Client{ "192.168.219.200", 9999, timeval{ 2, 0 } };

static BOOL ConsoleEventHandler(DWORD event) {

	switch (event) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
		Client.Leave();
		break;
	}
	return TRUE;
}

int main()
{
	SetConsoleCtrlHandler(ConsoleEventHandler, TRUE);
	
	char Buffer[2048]{};
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

	static constexpr int KWidth{ 130 };
	static constexpr int KHeight{ 30 };
	CSingleBufferedConsole Console{ KWidth, KHeight, "StringWorld", ECommandLinePosition::Bottom };
	Console.SetClearBackground(EBackgroundColor::Black);
	Console.SetDefaultForeground(EForegroundColor::LightYellow);

	std::thread ThrNetwork
	{
		[&]()
		{
			while (true)
			{
				if (Client.IsTerminating() || Client.IsTimedOut()) break;

				Client.Receive();
			}
		}
	};
	
	std::thread ThrInput
	{
		[&]()
		{
			while (true)
			{
				if (Client.IsTerminating() || Client.IsTimedOut()) break;

				if (Console.HitKey())
				{
					if (Console.IsHitKey(EArrowKeys::Left))
					{
						Client.Input(EInput::Left);
					}
					else if (Console.IsHitKey(EArrowKeys::Right))
					{
						Client.Input(EInput::Right);
					}
					else if (Console.IsHitKey(EArrowKeys::Up))
					{
						Client.Input(EInput::Up);
					}
					else if (Console.IsHitKey(EArrowKeys::Down))
					{
						Client.Input(EInput::Down);
					}
					else if (Console.IsHitKey(VK_RETURN))
					{
						if (Console.ReadCommand())
						{
							if (Console.IsLastCommand("/quit"))
							{
								Client.Leave();
							}
							else
							{
								Client.Chat(Console.GetLastCommand());
							}
						}
					}
				}
			}
		}
	};

	while (true)
	{
		if (Client.IsTerminating() || Client.IsTimedOut()) break;

		Console.Clear();

		Console.PrintBox(0, 0, 70, KHeight - 1, ' ', EBackgroundColor::DarkGray, EForegroundColor::Black);

		Console.PrintBox(70, 0, 40, KHeight - 1, ' ', EBackgroundColor::DarkGray, EForegroundColor::Black);
		if (Client.HasChatLog())
		{
			static constexpr int KMaxLines{ KHeight - 1 - 2 };

			auto& vChatLog{ Client.GetChatLog() };
			int ChatCount{ (int)vChatLog.size() };
			int Offset{ (ChatCount > KMaxLines) ? ChatCount - KMaxLines : 0 };
			for (int i = Offset; i < ChatCount; ++i)
			{
				if (vChatLog[i].bIsLocal)
				{
					Console.PrintHString(70 + 1, 1 + i - Offset, vChatLog[i].String, EForegroundColor::LightYellow, 40 - 2);
				}
				else
				{
					Console.PrintHString(70 + 1, 1 + i - Offset, vChatLog[i].String, EForegroundColor::Yellow, 40 - 2);
				}
			}
		}

		auto& vClientData{ Client.GetClientData() };
		auto& MyData{ Client.GetMyDatum() };
		for (auto& Client : vClientData)
		{
			if (Client.ID == KInvalidID) continue;
			Console.PrintChar(Client.X, Client.Y, '@', EForegroundColor::Yellow);
		}
		Console.PrintChar(MyData.X, MyData.Y, '@', EForegroundColor::LightYellow);
		
		Console.PrintHString(112, 1, "ID");
		Console.PrintHString(115, 1, Client.GetMyStringID().String);

		Console.PrintHString(112, 3, " X");
		Console.PrintHString(112, 4, " Y");
		Console.PrintHString(115, 3, MyData.X);
		Console.PrintHString(115, 4, MyData.Y);

		Console.Render();
	}

	ThrNetwork.join();
	ThrInput.join();
	return 0;
}
