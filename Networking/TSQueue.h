#pragma once
#include "Common.h"

namespace networking {
	
	template <typename T>
	class TSQueue {
		// Thread save queue
	public:
		TSQueue() = default;
		TSQueue(const TSQueue<T>&) = delete;


		virtual ~TSQueue() { clear(); }

	
	public:
		const T& front() {
			std::scoped_lock lock(muxQueue);
			return deqQueue.front();
		}


		const T& back() {
			std::scoped_lock(muxQueue);
			return deqQueue.back();
		}


		void push_back(const T& item) {
			std::scoped_lock lock(muxQueue);
			deqQueue.emplace_back(std::move(item));
			std::unique_lock<std::mutex> uLock(muxBlocking);
			cvBlocking.notify_one();
		}


		void push_front(const T& item) {
			std::scoped_lock lock(muxQueue);
			deqQueue.emplace_front(std::move(item));
			std::unique_lock<std::mutex> uLock(muxBlocking);
			cvBlocking.notify_one();
		}


		bool is_empty() {
			std::scoped_lock lock(muxQueue);
			return deqQueue.empty();
		}


		size_t count() {
			std::scoped_lock lock(muxQueue);
			return deqQueue.size();
		}


		void clear() {
			std::scoped_lock lock(muxQueue);
			deqQueue.clear();
		}


		T pop_front() {
			std::scoped_lock lock(muxQueue);
			auto value = std::move(deqQueue.front());
			deqQueue.pop_front();
			return value;
		}


		T pop_back() {
			std::scoped_lock lock(muxQueue);
			auto value = std::move(deqQueue.back());
			deqQueue.pop_back();
			return value;
		}


		void wait() {
			while (is_empty()) {
				std::unique_lock<std::mutex> uLock(muxBlocking);
				cvBlocking.wait(uLock);
			}
		}


	protected:
		std::mutex muxQueue;
		std::deque<T> deqQueue;
		std::condition_variable cvBlocking;
		std::mutex muxBlocking;
	};
}