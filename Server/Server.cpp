#include "Server.h"
#include <thread>
#include <chrono>

int main()
{
	CServer Server{};
	Server.DisplayInfo();
	Server.Open();

	std::chrono::steady_clock Clock{};
	std::thread thr_net{
		[&]()
		{
			long long PrevTimePoint{Clock.now().time_since_epoch().count()};
			while (true)
			{
				if (Server.IsClosing()) break;
				Server.Listen(true);

				long long CurrTimePoint{ Clock.now().time_since_epoch().count() };
				if (CurrTimePoint - PrevTimePoint > 30'000'000)
				{
					if (Server.Update())
					{
						//printf("Update - tick: %lld\n", CurrTimePoint / 1'000'000);
					}
					PrevTimePoint = CurrTimePoint;
				}
			}
		}
	};

	char CmdBuffer[2048]{};
	std::thread thr_command{
		[&]()
		{
			while (true)
			{
				fgets(CmdBuffer, 2048, stdin);
				if (strncmp(CmdBuffer, "QUIT", 4) == 0)
				{
					Server.Close();
					break;
				}
			}
		}
	};

	thr_net.join();
	thr_command.join();

	return 0;
}