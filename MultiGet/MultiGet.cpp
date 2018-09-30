// Carnegie Coding Check: Multi-GET

/*
Target: Build an application that downloads part of a file from a web server, in chunks.
Requirements
Your app should meet all of the following requirements:
• Source URL should be specified with a required command-line option
• File is downloaded in 4 chunks (4 requests made to the server)
• Only the first 4 MiB of the file should be downloaded from the server
• Output file may be specified with a command-line option (but may have a default if not set)
• No corruption in file - correct order, correct size, correct bytes (you don’t need to verify correctness, but it should not introduce errors)
• File is retrieved with GET requests
Optional features
If you finish quickly, you may improve your app by adding features like:
• File is downloaded in parallel rather than serial
• Support files smaller than 4 MiB (less chunks/adjust chunk size)
• Configurable number of chunks/chunk size/total download size

 */

#include <iostream>
#include "CommandLineParams.h"
#include <string>
#include <memory>
#include <vector>
#include <curl/curl.h>
#include "ThreadPool.h"
#include <fstream>
#include <atomic>
#include <cassert>

static void usage()
{
	std::cout << "Usage: MultiGet -u url [-f file] [-c chunksize] [-n chunknumber]" << std::endl;
	std::cout << "    url:         The url of file to be downloaded" << std::endl;
	std::cout << "    file:        File name for saving the downloaded file" << std::endl;
	std::cout << "                 Default filename 384MB.jar" << std::endl;
	std::cout << "    chunksize:   The size of chunk in byte. Default: 1MB" << std::endl;
	std::cout << "    chunknumber: The number of chunk to download" << std::endl;
	std::cout << "                 Default: 4. 0 means all chunks" << std::endl;
};

static const char* DEFAULT_FILENAME = "384MB.jar";
static const int CHUNK_SIZE = 1 * 1024 * 1024; // 1MB
static const int CHUNK_NUM = 4;		// Default chunk number
static const int THREAD_NUM = 4;		// Default number of thread in thread pool

static const int RETRY_NUM = 3;		// Retry time of downloading a chunk

// Libcurl callback function
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	assert(contents != nullptr);
	assert(userp != nullptr);

	size_t realsize = size * nmemb;

	auto vtr = static_cast<std::vector<uint8_t>*>(userp);
	for (size_t i = 0; i < realsize; i++)
	{
		vtr->emplace_back(((uint8_t*)contents)[i]);
	}

	return realsize;
}

// Get the content without header of the URL in the given range
// headerOnly = true returns content of header only
// Returns a vector containing the downloaded content
static std::shared_ptr<std::vector<uint8_t>> getContent(const char* url, int offset, int length, bool headerOnly = false)
{
	assert(url != nullptr);

	CURLcode res;
	auto deleter = [](CURL* curl) {curl_easy_cleanup(curl); };
	std::unique_ptr<CURL, decltype(deleter)> curl(curl_easy_init(), deleter);
	std::shared_ptr<std::vector<uint8_t>> chunk;
	
	if (curl) {
		curl_easy_setopt(curl.get(), CURLOPT_URL, url);
		// Set headers
		if (headerOnly)
		{
			curl_easy_setopt(curl.get(), CURLOPT_HEADER, 1L);
			curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1L);
		}
		else
		{
			std::string range = std::to_string(offset);
			range.append("-").append(std::to_string(offset + length - 1));

			curl_easy_setopt(curl.get(), CURLOPT_RANGE, range.c_str());
		}

		/* send all data to this function  */
		curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

		chunk = std::make_shared <std::vector<uint8_t>>();
		curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, chunk.get());

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl.get());
		/* Check for errors */
		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));

			chunk = nullptr;
		}
	}

	return chunk;
}

// Get file size of the given url
// Returning -1 means error happens
int getUrlFileSize(const char* url)
{
	int length = -1;

	// Get header of http response
	auto chunk = getContent(url, 0, 0, true);
	if (chunk != nullptr)
	{
		std::string content(chunk->begin(), chunk->end());
		std::string contentLength("Content-Length: ");
		auto index = content.find(contentLength);
		if (index != std::string::npos)
		{
			index += contentLength.length();
			auto end = content.find('\r', index);
			std::string size = content.substr(index, end - index);
			length = std::stoi(size);
		}
	}

	return length;
}

int main(int argc, const char *argv[])
{
	// Parse input paramters
	CommandLineParams params(argc, argv);
	if (!params.ContainsKey("url"))
	{
		usage();
		return -1;
	}

	int chunk_size = params.ContainsKey("c") ? atoi(params["c"]) : CHUNK_SIZE;
	int chunk_num = params.ContainsKey("n") ? atoi(params["n"]) : CHUNK_NUM;
	int thread_num = THREAD_NUM;	// Could be extened to paramter passed in
	const char* url = params["url"];
	const char* file = params.ContainsKey("f") ? params.operator[]("f") : DEFAULT_FILENAME;

	int file_size = getUrlFileSize(url);
	if (file_size == -1)
	{
		std::cout << "Failed to get the size of downloaded file." << std::endl;
		return -1;
	}

	//std::cout << "file size:" << file_size << std::endl;
	// Calculate the actual number of chunks.
	int num = (file_size + chunk_size - 1) / chunk_size;
	chunk_num = (chunk_num <= 0) 
				? num : min(num, chunk_num);

	// Create a filestream to store the downloaded chunks
	std::ofstream fs(file, std::ios::out | std::ios::binary);

	ThreadPool pool(thread_num);
	std::mutex writeChunkMutex;
	std::atomic<bool> downloadSucceeded = true;

	// Create tasks to download file in chunks
	for (int i=0; i<chunk_num; i++)
	{
		pool.Enqueue([&fs, url, &writeChunkMutex, i, chunk_size, chunk_num, file_size, &downloadSucceeded]()
		{
			int retry = RETRY_NUM;

			int offset = i * chunk_size;
			int length = chunk_size;
			if (i == chunk_num - 1)
			{
				// Last chunk
				if (chunk_size * chunk_num > file_size)
				{
					length = file_size - chunk_size * i;
				}
			}

			// Retry if there is failure downloading
			while (retry > 0)
			{
				auto chunk = getContent(url, offset, length);
				if (chunk != nullptr)
				{
					std::string content(chunk->begin(), chunk->end());

					{
						// Store content to file
						std::unique_lock<std::mutex> lock(writeChunkMutex);
						fs.seekp(offset);
						fs << content;
					}

					std::cout << "Download succeeded. Chunk start from " << offset << ", length " << length << std::endl;
					return;
				}
				retry--;
				std::cout << "Download failed. Chunk start from " << offset << ", length " << length << std::endl;
			}

			// Here means downloading fails
			downloadSucceeded = false;				
		});
	}

	std::cout << "Downloading......." << std::endl;

	// Wait until all tasks finish
	pool.ShutDownWhenNoTask();

	if (downloadSucceeded)
	{
		std::cout << "Downloaded succeeded" << std::endl;
	}
	else
	{
		std::cout << "Downloaded failed" << std::endl;
	}

    return 0;
}

