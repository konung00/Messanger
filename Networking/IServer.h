#pragma once

#include "Common.h"
#include "Connection.h"
#include "Message.h"
#include "TSQueue.h"

namespace networking {
	template <typename T>
	class IServer {
	public:
		IServer(uint16_t port)
			: acceptor_(asioContext_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
		{}


		virtual ~IServer() {
			stop();
		}


	public:
		bool start() {
			try {
				waitForClientConnection();
				threadContext_ = std::thread([this]() { asioContext_.run(); });
			}
			catch (std::exception& e) {
				std::cerr << "[SERVER] Exception: " << e.what() << std::endl;
				return false;
			}

			std::cout << "[SERVER] Started!" << std::endl;
			return true;
		}


		void stop() {
			asioContext_.stop();
			if (threadContext_.joinable())
				threadContext_.join();

			std::cout << "[SERVER] Stopped!" << std::endl;
		}


		// ASYNC
		void waitForClientConnection() {
			acceptor_.async_accept(
				[this](std::error_code ec, asio::ip::tcp::socket socket) 
				{
					if (!ec) {
						std::cout << "[SERVER] New connection established: " << socket.remote_endpoint() << std::endl;
						std::shared_ptr<Connection<T>> newConnection =
							std::make_shared<Connection<T>>(Connection<T>::owner::server, asioContext_, std::move(socket), qMessageIn_);
						
						if (onClientConnect(newConnection)) {
							connectionTable_.push_back(std::move(newConnection));
							connectionTable_.back()->connectToClient(this, ++idCounter);
							std::cout << "[" << connectionTable_.back()->getID() << "] Connection Approved" << std::endl;
						}
						else {
							std::cout << "[------] Connection denied" << std::endl;
						}
					}
					else {
						std::cout << "[SERVER] New connection error: " << ec.message() << std::endl;
					}
					waitForClientConnection();
				});
		}


		void messageClient(std::shared_ptr<Connection<T>> client, const Message<T>& msg) {
			if (client && client->isConnected()) {
				client->send(msg);
			}
			else {
				onClientDisconnect(client);
				client.reset();
				connectionTable_.erase(
					std::remove(connectionTable_.begin(), connectionTable_.end(), client), connectionTable_.end()
				);
			}
		}


		void messageAllClients(const Message<T>& msg, std::shared_ptr<Connection<T>> ignoreClient = nullptr) {
			bool invalidClientExists = false;

			for (auto& client : connectionTable_) {

				if (client && client->isConnected()) {

					if (client != ignoreClient)
						client->send(msg);
				}
				else {
					onClientDisconnect(client);
					client.reset();
					invalidClientExists = true;
				}
			}

			if (invalidClientExists)
				connectionTable_.erase(
					std::remove(connectionTable_.begin(), connectionTable_.end(), nullptr), connectionTable_.end()
				);
		}


		void update(size_t numMaxMsg = -1, bool isWaiting = false) {
			if (isWaiting)
				qMessageIn_.wait();

			size_t msgCount = 0;
			while (msgCount < numMaxMsg && !qMessageIn_.is_empty()) {
				auto msg = qMessageIn_.pop_front();
				onMessage(msg.remoteConnection, msg.msg);
				++msgCount;
			}
		}


	protected:
		virtual bool onClientConnect(std::shared_ptr<Connection<T>> client) {
			return true;
		}
		

		virtual  void onClientDisconnect(std::shared_ptr<Connection<T>> client) {}


		virtual void onMessage(std::shared_ptr<Connection<T>> client, Message<T>& msg) {}


	public:
		virtual void onClientValidated(std::shared_ptr<Connection<T>> client) {}


	protected:
		// Thread Safe Queue for incoming message packets
		TSQueue<Owned_message<T>> qMessageIn_;

		// Container of active validated connections
		std::deque<std::shared_ptr<Connection<T>>> connectionTable_;

		asio::io_context asioContext_;

		std::thread threadContext_;

		asio::ip::tcp::acceptor acceptor_;

		uint32_t idCounter = 10000;
	};
}