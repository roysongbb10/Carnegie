#pragma once

#include <map>

class CommandLineParams
{
public:
	CommandLineParams(int argc, const char *argv[]);
	~CommandLineParams();

	const char* operator[](const std::string& param);

	bool ContainsKey(const char *param);

private:
	std::map<std::string, std::string> _params;
};

