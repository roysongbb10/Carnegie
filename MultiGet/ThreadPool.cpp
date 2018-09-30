#include "ThreadPool.h"


ThreadPool::ThreadPool(int threads)	:
			terminate(false),
			stopped(false)
{
	for (int i = 0; i < threads; i++) {
		//threadPool.push_back(std::thread(&ThreadPool::Invoke, this));
		threadPool.emplace_back(&ThreadPool::Invoke, this);
	}
}


ThreadPool::~ThreadPool()
{
	if (!stopped) {
		ShutDown();
	}
}

void ThreadPool::Enqueue(std::function<void()> f) {
	{
		std::unique_lock<std::mutex> lock(tasksMutex);

		tasks.push(f);
	}

	// Wake up one thread
	condition.notify_one();
}

void ThreadPool::Invoke() {
	std::function<void()> task;
	while (true) {
		// Scope based locking
		{
			std::unique_lock<std::mutex> lock(tasksMutex);

			// Wait until queue is not empty or termination signal is sent
			condition.wait(lock, [this]{ return !tasks.empty() || terminate; });

			// If termination signal received and queue is empty then eixt
			if (terminate && tasks.empty()) {
				return;
			}

			task = tasks.front();

			// Remove it from the queue
			tasks.pop();
		}

		// Execute the task
		task();
	}
}

void ThreadPool::ShutDown() {
	{
		std::unique_lock<std::mutex> lock(tasksMutex);

		terminate = true;
	}

	// Wake up all threads
	condition.notify_all();

	for (std::thread& thread : threadPool) {
		thread.join();
	}

	threadPool.empty();

	stopped = true;
}

void ThreadPool::ShutDownWhenNoTask()
{
	// Polling
	// Contional varaible doens't work here because no one emit notify message
	while (!tasks.empty())
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1s);
	}

	ShutDown();
}
