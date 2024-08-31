#pragma once

#include <string>
class SysInfo {
	public:
		static const std::string OSName();
		static const std::string CPUName();
		static const int CoreCount();
		static const int TotalMemory();
};
