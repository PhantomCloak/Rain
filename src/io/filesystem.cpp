#include "filesystem.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace Rain {
  std::vector<std::string> FileSys::GetFilesInDirectory(std::string path) {
    std::vector<std::string> fileList;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      if (!entry.is_directory()) {
        fileList.push_back(entry.path().string());
      }
    }
    return fileList;
  }

  std::vector<std::string> FileSys::GetFoldersInDirectory(std::string path) {
    std::vector<std::string> fileList;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
      if (entry.is_directory()) {
        fileList.push_back(entry.path().string());
      }
    }
    return fileList;
  }

  std::string FileSys::GetFileExtension(std::string path) {
    std::size_t found = path.find_last_of(".");
    if (found == std::string::npos) {
      return "";
    }
    return path.substr(found + 1);
  }

  std::string FileSys::GetFileName(std::string path) {
    std::size_t found = path.find_last_of("/\\");
    if (found == std::string::npos) {
      return path;
    }
    return path.substr(found + 1);
  }

  std::string FileSys::GetParentDirectory(std::string path) {
    std::filesystem::path p(path);
    return p.parent_path().string();
  }

  void FileSys::OpenFileOSDefaults(std::string path) {
    std::string command = "open " + path;
    system(command.c_str());
  }

  std::string FileSys::ReadFile(std::string path) {
    std::ifstream file(path);

    // Check if file opened successfully
    if (!file.is_open()) {
      return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
  }

  bool FileSys::IsFileExist(std::string path) {
    std::ifstream file(path);
    return file.good();
  };

  void FileSys::WatchFile(const std::string& path, std::function<void(std::string fileName)> callback) {
    std::thread([path, callback]() {
      if (!std::filesystem::exists(path)) {
        std::cerr << "File does not exist: " << path << std::endl;
        return;
      }

      auto last_modified_time = std::filesystem::last_write_time(path);
      std::atomic<bool> running{true};

      while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));  // Check every second

        try {
          auto current_modified_time = std::filesystem::last_write_time(path);
          if (last_modified_time != current_modified_time) {
            callback(path);
            last_modified_time = current_modified_time;  // Update last modified time
          }
        } catch (const std::filesystem::filesystem_error& e) {
          std::cerr << "Error accessing file: " << e.what() << std::endl;
          running = false;  // Optionally stop on error
        }
      }
    }).detach();
  }
}  // namespace Rain
