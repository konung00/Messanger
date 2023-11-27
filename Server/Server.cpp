#include "Networking.h"

enum class MsgType : uint32_t {
	ServerAccept,
	ServerDeny,
	ServerPing,
	MessageAll,
	ServerMessage
};

class Server : public networking::IServer<MsgType> {
public:
	Server(uint64_t numPort)
		: networking::IServer<MsgType>(numPort)
	{}


protected:
	virtual bool onClientConnect(std::shared_ptr<networking::Connection<MsgType>> client) {
		networking::Message<MsgType> msg;
		msg.header.id = MsgType::ServerAccept;
		client->send(msg);
		return true;
	}


	virtual void onClientDisconnect(std::shared_ptr<networking::Connection<MsgType>> client) {
		std::cout << "Removing client [" << client->getID() << "]" << std::endl;
	}


	virtual void onMessage(std::shared_ptr<networking::Connection<MsgType>> client, networking::Message<MsgType>& msg) {
		switch (msg.header.id)
		{
		case MsgType::ServerPing:
		{
			std::cout << "[" << client->getID() << "]: Server ping" << std::endl;
			client->send(msg);
		}
		break;

		case MsgType::MessageAll:
		{
			std::cout << "[" << client->getID() << "]: Message all" << std::endl;
			networking::Message<MsgType> msg;
			msg.header.id = MsgType::ServerMessage;
			msg << (client->getID());
			messageAllClients(msg, client);
		}
		break;
		}
	}
};

int main() {
	Server myServer(6000);
	myServer.start();

	while (true) {
		myServer.update(-1, true);
	}

	return 0;
}