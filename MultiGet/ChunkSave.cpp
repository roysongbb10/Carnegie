#include "ChunkSave.h"
#include <iostream>


ChunkSave::ChunkSave(const char *file): terminate(false), stopped(false)
{
	fs.open(file, std::ios::out | std::ios::binary);
	thread = std::thread(&ChunkSave::StoreChunk, this);
}


ChunkSave::~ChunkSave()
{
	if (!stopped) {
		ShutDown();
	}
}

void ChunkSave::ShutDown()
{
	{
		std::unique_lock<std::mutex> lock(taskMutex);
		terminate = true;
	}

	// Wake up all threads
	condition.notify_all();

	// Wait for thread end
	thread.join();
	stopped = true;
}

void ChunkSave::SaveChunk(int offset, std::shared_ptr<std::vector<uint8_t>> content)
{
	std::unique_lock<std::mutex> lock(taskMutex);
	tasks.push(Chunk(offset, content));
	condition.notify_one();
}

void ChunkSave::StoreChunk()
{
	while (true)
	{
		Chunk chunk;

		// Scope based locking
		{
			std::unique_lock<std::mutex> lock(taskMutex);

			// Wait until queue is not empty or termination signal is sent
			condition.wait(lock, [this] { return !tasks.empty() || terminate; });

			// If termination signal received and queue is empty then eixt
			if (terminate && tasks.empty()) {
				return;
			}

			chunk = tasks.front();

			// Remove top
			tasks.pop();
		}

		fs.seekp(chunk.first);
		fs.write(reinterpret_cast<const char*>(&chunk.second->data()[0]), chunk.second->size());
	}
}
