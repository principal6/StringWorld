#include "Server.h"
#include <thread>

int main()
{
	CServer Server{};
	Server.DisplayInfo();
	Server.Open();

	std::thread thr_listen{
		[&]()
		{
			while (true)
			{
				if (Server.IsClosing()) break;
				Server.Listen(true);
			}
		}
	};

	char CmdBuffer[2048]{};
	std::thread thr_work{
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

	thr_listen.join();
	thr_work.join();

	return 0;
}