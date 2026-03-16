#include "SysInfo.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

#include <fstream>
#include <sstream>
#include <string>

const std::string SysInfo::OSName() {
#if defined(__APPLE__) || defined(__MACH__)
  return "macOS";
#elif defined(_WIN32)
  return "Windows";
#elif defined(__linux__)
  std::ifstream file("/etc/os-release");
  std::string line;
  std::string distroName = "Unknown Linux Distribution";

  if (file.is_open()) {
    while (std::getline(file, line)) {
      if (line.find("PRETTY_NAME=") == 0) {
        distroName = line.substr(13, line.length() - 14);  // Removes 'PRETTY_NAME="' and '"'
        break;
      }
    }
    file.close();
  }

  return distroName;
#else
  return "Unknown OS";
#endif
}

const std::string SysInfo::CPUName() {
  std::string cpuName;
#if defined(__APPLE__) || defined(__MACH__)
  char buffer[128];
  size_t bufferSize = sizeof(buffer);
  sysctlbyname("machdep.cpu.brand_string", &buffer, &bufferSize, NULL, 0);
  cpuName = buffer;
#elif defined(_WIN32)
  HKEY hKey;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    char value[256];
    DWORD size = sizeof(value);
    if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
                         (LPBYTE)value, &size) == ERROR_SUCCESS) {
      cpuName = value;
    }
    RegCloseKey(hKey);
  }
#elif defined(__linux__)
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  while (std::getline(cpuinfo, line)) {
    if (line.find("model name") != std::string::npos) {
      cpuName = line.substr(line.find(":") + 2);
      break;
    }
  }
#endif
  return cpuName;
}

const int SysInfo::CoreCount() {
  int coreCount = 0;
#if defined(__APPLE__) || defined(__MACH__)
  int mib[2];
  mib[0] = CTL_HW;
  mib[1] = HW_AVAILCPU;
  size_t len = sizeof(coreCount);
  sysctl(mib, 2, &coreCount, &len, NULL, 0);
  if (coreCount < 1) {
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &coreCount, &len, NULL, 0);
    if (coreCount < 1) {
      coreCount = 1;
    }
  }
#elif defined(_WIN32)
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  coreCount = sysInfo.dwNumberOfProcessors;
#elif defined(__linux__)
  coreCount = sysconf(_SC_NPROCESSORS_ONLN);
#endif
  return coreCount;
}

const int SysInfo::TotalMemory() {
  int64_t totalMemory = 0;
#if defined(__APPLE__) || defined(__MACH__)
  int mib[2];
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  size_t len = sizeof(totalMemory);
  sysctl(mib, 2, &totalMemory, &len, NULL, 0);
#elif defined(_WIN32)
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  totalMemory = memInfo.ullTotalPhys;
#elif defined(__linux__)
  struct sysinfo sysInfo;
  sysinfo(&sysInfo);
  totalMemory = sysInfo.totalram;
  totalMemory *= sysInfo.mem_unit;
#endif
  return totalMemory / (1024 * 1024);  // Return memory in MB
}
