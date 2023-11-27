#pragma once
#include "Common.h"
#include "Message.h"
#include "TSQueue.h"


namespace networking {

	template <typename T>
	class IServer;

	template <typename T>
	class Connection : public std::enable_shared_from_this<Connection<T>> {
	public:
		enum class owner {
			server,
			client
		};


		Connection(owner parent, asio::io_context& asioContextIn, asio::ip::tcp::socket socketIn, TSQueue<Owned_message<T>>& queueMsgIn)
			: asioContext_(asioContextIn)
			, socket_(std::move(socketIn))
			, qMessagesIn_(queueMsgIn)
			, ownerType_(parent)
		{
			if (ownerType_ == owner::server) {
				handShakeIn_ = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
				handShakeCheck_ = scramble(handShakeOut_);
			}
			else {
				handShakeIn_ = 0;
				handShakeOut_ = 0;
			}
		}


		virtual ~Connection() {}


	public:
		uint32_t getID() const {
			return id_;
		}


		void disconnect() {
			if (isConnected())
				asio::post(asioContext_, [this]() { socket_.close(); });
		}


		bool isConnected() {
			return socket_.is_open();
		}


		void send(const Message<T>& msg) {
			asio::post(asioContext_,
				[this, msg]()
				{
					bool bWrittenMessage = !qMessagesOut_.is_empty();
					qMessagesOut_.push_back(msg);
					if (!bWrittenMessage)
						writeHeader();
				});
		}


		void connectToClient(networking::IServer<T>* server, uint32_t uID = 0) {
			if (ownerType_ == owner::server) {
				if (isConnected()) {
					id_ = uID;
					writeValidation();
					readValidation(server);
				}
			}
		}


		void connectToServer(const asio::ip::tcp::resolver::results_type& endpoints) {
			if (ownerType_ == owner::client) {
				asio::async_connect(socket_, endpoints,
					[this](std::error_code ec, asio::ip::tcp::endpoint)
					{
						if (!ec)
							readValidation();
					});
			}
		}

		
	private:
		// ASYNC - Prime context to write a message header
		void writeHeader() {
			asio::async_write(socket_, asio::buffer(&qMessagesOut_.front().header, sizeof(Message_header<T>)),
				[this](std::error_code ec, size_t length) 
				{
					if (!ec) {
						if (qMessagesOut_.front().body.size() > 0) {
							writeBody();
						}
						else {
							qMessagesOut_.pop_front();
							if (!qMessagesOut_.is_empty())
								writeHeader();
						}
					}
					else {
						std::cerr << "[" << id_ << "] Write header fail!" << std::endl;
						socket_.close();
					}
				});
		}


		// ASYNC - Prime context to write a message body
		void writeBody() {
			asio::async_write(socket_, asio::buffer(qMessagesOut_.front().body.data(), qMessagesOut_.front().body.size()),
				[this](std::error_code ec, size_t length) 
				{
					if (!ec) {
						qMessagesOut_.pop_front();
						if (!qMessagesOut_.is_empty())
							writeHeader();
					}
					else {
						std::cerr << "[" << id_ << "] Write body fail!" << std::endl;
						socket_.close();
					}
				});
		}


		// ASYNC - Prime context ready to read a message header
		void readHeader() {
			asio::async_read(socket_, asio::buffer(&msgTempIn_.header, sizeof(Message_header<T>)),
				[this](std::error_code ec, size_t length) 
				{
					if (!ec) {
						if (msgTempIn_.header.size > 0) {
							msgTempIn_.body.resize(msgTempIn_.header.size);
							readBody();
						}
						else {
							addToIncomingMessageQueue();
						}
					}
					else {
						std::cerr << "[" << id_ << "] Read header fail!" << std::endl;
						socket_.close();
					}
				});
		}


		// ASYNC - Prime context ready to read a message body
		void readBody() {
			asio::async_read(socket_, asio::buffer(msgTempIn_.body.data(), msgTempIn_.body.size()),
				[this](std::error_code ec, size_t length)  
				{
					if (!ec) {
						addToIncomingMessageQueue();
					}
					else {
						std::cerr << "[" << id_ << "] Read body fail!" << std::endl;
						socket_.close();
					}
				});
		}


		// Once a full message is received, add it to the incoming queue
		void addToIncomingMessageQueue() {
			if (ownerType_ == owner::server)
				// Convert it to an "owned message", by initialising with the a shared pointer from this connection object
				qMessagesIn_.push_back({ this->shared_from_this(), msgTempIn_ });
			else
				qMessagesIn_.push_back({ nullptr, msgTempIn_ });

			readHeader();
		}


		// "Encrypt" data
		uint64_t scramble(uint64_t nInput) {
			uint64_t out = nInput ^ 0xDEADBEEFC0DECAFE;
			out = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;
			return out ^ 0xC0DEFACE12345678;
		}


		void writeValidation() {
			asio::async_write(socket_, asio::buffer(&handShakeOut_, sizeof(uint64_t)),
				[this](std::error_code ec, size_t length) 
				{
					if (!ec) {
						if (ownerType_ == owner::client)
							readHeader();
					}
					else {
						socket_.close();
					}
				});
		}


		void readValidation(networking::IServer<T>* server = nullptr) {
			asio::async_read(socket_, asio::buffer(&handShakeIn_, sizeof(uint64_t)),
				[this, server](std::error_code ec, size_t length) 
				{
					if (!ec) {
						if (ownerType_ == owner::server) {
							if (handShakeCheck_ == handShakeIn_) {
								std::cout << "Client validated" << std::endl;
								server->onClientValidated(this->shared_from_this());
								readHeader();
							}
							else {
								std::cout << "Client disconnected (fail validation)" << std::endl;
								socket_.close();
							}
						}
						else {
							handShakeOut_ = scramble(handShakeIn_);
							writeValidation();
						}
					}
					else {
						std::cout << "Client disconnected (readValidation)" << std::endl;
					}
				});
		}


	protected:
		// Descriptor of connection
		asio::ip::tcp::socket socket_;

		// Shared context for asio instance
		asio::io_context& asioContext_;

		// This queue holds all messages to be sent to the remote side of this connection
		TSQueue<Message<T>> qMessagesOut_;

		// Ref to the incoming queue of the parent object
		TSQueue<Owned_message<T>>& qMessagesIn_;

		// Temorary variable for assembling asynchronous messages
		Message<T> msgTempIn_;

		// Pointer to type of handling the connection
		owner ownerType_ = owner::server;

		uint32_t id_ = 0;

		// <Handshake validation>
		uint64_t handShakeOut_ = 0; 
		uint64_t handShakeIn_ = 0;
		uint64_t handShakeCheck_ = 0;
		// </Handshake validation>
	};
}