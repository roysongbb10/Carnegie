#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

class ThreadPool
{
public:
	ThreadPool(int threads);
	~ThreadPool();

	// Add a task to a queue
	void Enqueue(std::function<void()> f);

	// shut down the pool
	void ShutDown();

	// Wait for shutting down the pool after queue is empty
	void ShutDownWhenNoTask();

private:
	std::vector<std::thread> threadPool;

	std::queue<std::function<void()>> tasks;

	std::mutex tasksMutex;

	std::condition_variable condition;

	bool terminate;
	bool stopped;

	// Function that will be invoked by our threads
	void Invoke();

};

