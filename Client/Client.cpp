#include "Client.h"
#include "DoubleBufferedConsole.h"
#include <string>
#include <thread>

int main()
{
	CClient Client{ 9999, timeval{ 5, 0 } };
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

	static constexpr int KWidth{ 130 };
	static constexpr int KHeight{ 30 };
	CDoubleBufferedConsole Console{ KWidth, KHeight, "StringWorld" };
	Console.SetClearBackground(EBackgroundColor::Black);
	Console.SetDefaultForeground(EForegroundColor::LightYellow);

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

	while (true)
	{
		if (Console.HitKey())
		{
			EArrowKeys ArrowKey{ Console.GetHitArrowKey() };
			if (ArrowKey == EArrowKeys::Left)
			{
				Client.Input(EInput::Left);
			}
			else if (ArrowKey == EArrowKeys::Right)
			{
				Client.Input(EInput::Right);
			}
			else if (ArrowKey == EArrowKeys::Up)
			{
				Client.Input(EInput::Up);
			}
			else if (ArrowKey == EArrowKeys::Down)
			{
				Client.Input(EInput::Down);
			}

			int Key{ Console.GetHitKey() };
			if (Key == VK_ESCAPE)
			{
				break;
			}
			else if (Key == VK_RETURN)
			{
				if (Console.GetCommand(0, KHeight - 1))
				{
					Client.Chat(Console.GetLastCommand());
				}
			}
		}

		Console.Clear();

		//Console.FillBox(5, 5, 7, 10, '~', EBackgroundColor::Cyan, EForegroundColor::White);
		Console.PrintBox(0, 0, 70, KHeight - 1, ' ', EBackgroundColor::DarkGray, EForegroundColor::Black);

		Console.PrintBox(70, 0, 40, KHeight - 1, ' ', EBackgroundColor::DarkGray, EForegroundColor::Black);
		//Console.PrintCommandLog(70, 0, 40, 29);		
		if (Client.HasChatLog())
		{
			static constexpr int KMaxLines{ KHeight - 1 - 2 };

			auto& vChatLog{ Client.GetChatLog() };
			int ChatCount{ (int)vChatLog.size() };
			int Offset{ (ChatCount > KMaxLines) ? ChatCount - KMaxLines : 0 };
			for (int i = Offset; i < ChatCount; ++i)
			{
				Console.PrintHString(70 + 1, 1 + i - Offset, vChatLog[i].String, 40 - 2);
			}
		}

		auto& vClientData{ Client.GetClientData() };
		auto& MyData{ Client.GetMyDatum() };
		for (auto& Client : vClientData)
		{
			Console.PrintChar(Client.X, Client.Y, '@', EForegroundColor::Yellow);
		}
		Console.PrintChar(MyData.X, MyData.Y, '@', EForegroundColor::LightYellow);

		Console.PrintChar(112, 1, 'X');
		Console.PrintChar(112, 2, 'Y');
		Console.PrintHString(114, 1, std::to_string(MyData.X).c_str());
		Console.PrintHString(114, 2, std::to_string(MyData.Y).c_str());

		Console.Render();
	}

	thr_listen.join();

	return 0;
}
