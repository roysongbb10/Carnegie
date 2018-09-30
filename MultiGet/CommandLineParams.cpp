#include "CommandLineParams.h"


CommandLineParams::CommandLineParams(int argc, const char *argv[])
{
	if (argc < 2 || argv == NULL)
		return;

	const char *param = NULL;
	//bool first = true;	// For retrieving command name;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' || argv[i][0] == '/') {
			if (param != NULL) {
				_params[std::string(param)] = std::string("true");
			}

			param = argv[i] + 1;
		}
		else {
			if (param != NULL) {
				_params[std::string(param)] = std::string(argv[i]);
				param = NULL;
			}
		}
	}

	// If the last param is still waiting, set to true
	if (param != NULL) {
		_params[std::string(param)] = std::string("true");
	}
}


CommandLineParams::~CommandLineParams()
{
}

const char* CommandLineParams::operator[](const std::string& param) {
	return ContainsKey(param.c_str()) ? _params[param].c_str() : NULL;
}

bool CommandLineParams::ContainsKey(const char *param) {
	auto it = _params.find(std::string(param));
	return it != _params.end();
}