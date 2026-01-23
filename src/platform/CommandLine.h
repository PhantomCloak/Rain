#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>

class CommandLine
{
 public:
  static void InitializeParams(const std::vector<std::string>& args);

  template <typename T>
  static T Get(std::string arg);
  static bool Param(const std::string& arg);
  static void AddParam(const std::string& arg, const std::string& value);

 private:
  static std::unique_ptr<CommandLine> Instance;
  std::map<std::string, std::string> m_Args;
};
