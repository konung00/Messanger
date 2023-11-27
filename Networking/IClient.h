#pragma once
#include "Common.h"
#include "Connection.h"
#include "Message.h"
#include "TSQueue.h"


namespace networking {
	template <typename T>
	class IClient {
	public:
		IClient() {}
		

		virtual ~IClient() {
			disconnect();
		}


	public:
		bool connect(const std::string& host, const uint16_t port) {
			try {
				// Resolve hostname/ip-address into tangiable physical address
				asio::ip::tcp::resolver resolver_ (asioContext_);
				asio::ip::tcp::resolver::results_type endpoints = resolver_.resolve(host, std::to_string(port));

				// Create a connection
				connection_ =
					std::make_unique<Connection<T>>(Connection<T>::owner::client, asioContext_, asio::ip::tcp::socket(asioContext_), qMessagesIn_);

				// Fulfill the connection
				connection_->connectToServer(endpoints);

				threadContext_ = std::thread([this]() { asioContext_.run(); });
			}
			catch (std::exception& e) {
				std::cerr << "Client exception: " << e.what() << std::endl;
				return false;
			}

			return true;
		}


		void disconnect() {
			if (isConnected()) {
				connection_->disconnect();
			}

			asioContext_.stop();
			if (threadContext_.joinable())
				threadContext_.join();

			connection_.release();
		}


		bool isConnected() {
			if (connection_)
				return true;
			return false;
		}


	public:
		// Send a message to the server
		void send(const Message<T>& msg) {
			if (isConnected())
				connection_->send(msg);
		}


		// Get queue of messages from sever
		TSQueue<Owned_message<T>>& incoming() {
			return qMessagesIn_;
		}


	protected:
		asio::io_context asioContext_;
		std::thread threadContext_;

		// Unique pointer to the connection to the server
		std::unique_ptr<Connection<T>> connection_;


	private:
		// Thread safe queue of incoming messages from the server
		TSQueue<Owned_message<T>> qMessagesIn_;
	};
}