#include "platform/CommandLine.h"

void CommandLine::InitializeParams(const std::vector<std::string>& args)
{
  for (const auto& parameter : args)
  {
    int delimeter = parameter.find('=');
    Instance->m_Args.emplace(parameter.substr(0, delimeter), parameter.substr(delimeter, parameter.length() - delimeter));
  }
}

std::unique_ptr<CommandLine> CommandLine::Instance = nullptr;

template <typename T>
T CommandLine::Get(std::string arg)
{
  if (Instance == nullptr)
  {
    return false;
  }

  if (Instance->m_Args.find(arg) == Instance->m_Args.end())
  {
    return false;
  }

  const std::string& value = Instance->m_Args.at(arg);

  if constexpr (std::is_integral_v<T>)
  {
    return std::atoi(value.c_str());
  }

  if constexpr (std::is_same_v<T, bool>)
  {
    return value == "true";
  }

  if constexpr (std::is_same_v<T, std::string>)
  {
    return value;
  }
}

bool CommandLine::Param(const std::string& arg)
{
  if (Instance == nullptr)
  {
    return false;
  }

  for (const auto& [key, value] : Instance->m_Args)
  {
    if (key == arg)
    {
      return true;
    }
  }

  return false;
};

void CommandLine::AddParam(const std::string& arg, const std::string& value)
{
  if (Instance == nullptr)
  {
    return;
  }

  Instance->m_Args.emplace(arg, value);
}
