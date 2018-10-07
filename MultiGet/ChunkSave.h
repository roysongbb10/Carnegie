#pragma once
#include <fstream>
#include <queue>
#include <memory>
#include <mutex>

typedef std::pair<int, std::shared_ptr<std::vector<uint8_t>>> Chunk;

class ChunkSave
{
public:
	ChunkSave(const char *file);
	~ChunkSave();
	void ShutDown();
	void SaveChunk(int offset, std::shared_ptr<std::vector<uint8_t>> content);

private:
	void StoreChunk();

private:
	std::thread thread;
	std::ofstream fs;
	std::queue<Chunk> tasks;
	std::mutex taskMutex;
	std::condition_variable condition;

	bool terminate;
	bool stopped;
};

