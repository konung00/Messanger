#include "Networking.h"

enum class MsgType : uint32_t {
	ServerAccept,
	ServerDeny,
	ServerPing,
	MessageAll,
	ServerMessage,
};

class Client : public networking::IClient<MsgType> {
public:
	void pingServer() {
		networking::Message<MsgType> msg;
		msg.header.id = MsgType::ServerPing;
		std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
		msg << timeNow;
		send(msg);
	}


	void MessageAll() {
		networking::Message<MsgType> msg;
		msg.header.id = MsgType::MessageAll;
		send(msg);
	}
};


int main() {
	Client myClient;
	myClient.connect("127.0.0.1", 6000);

	bool key[3] = { false, false, false };
	bool old_key[3] = { false, false, false };

	bool bQuit = false;
	while (!bQuit) {
		if (GetForegroundWindow() == GetConsoleWindow()) {
			key[0] = GetAsyncKeyState('1') & 0x8000;
			key[1] = GetAsyncKeyState('2') & 0x8000;
			key[2] = GetAsyncKeyState('3') & 0x8000;
		}

		if (key[0] && !old_key[0]) {
			myClient.pingServer();
		}

		if (key[1] && !old_key[1]) {
			myClient.MessageAll();
		}

		if (key[2] && !old_key[2]) {
			bQuit = true;
		}

		for (int i = 0; i < 3; ++i)
			old_key[i] = key[i];

		if (myClient.isConnected()) {
			if (!myClient.incoming().is_empty()) {
				auto msg = myClient.incoming().pop_front().msg;

				switch (msg.header.id)
				{
					case MsgType::ServerAccept:
					{
						std::cout << "Server accepted connection" << std::endl;
					}
					break;

					case MsgType::ServerPing:
					{
						std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
						std::chrono::system_clock::time_point timeThen;
						msg >> timeThen;
						std::cout
							<< "Ping: "
							<< std::chrono::duration<double>(timeNow - timeThen).count()
							<< std::endl;
					}
					break;

					case MsgType::ServerMessage :
					{
						uint32_t clientID;
						msg >> clientID;
						std::cout << "Hello from [" << clientID << "]" << std::endl;
					}
					break;
				}
			}
		}
		else {
			std::cout << "Server is down" << std::endl;
			bQuit = true;
		}
	}

	return 0;
}