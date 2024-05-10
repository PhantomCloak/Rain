#include "filesystem.h"
#include <filesystem>
#include <sstream>
#include <fstream>


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
