#include "WGSLPreprocessor.h"
#include <sstream>
#include <vector>
#include "core/Log.h"

namespace WebEngine
{
  std::set<std::string> WGSLPreprocessor::s_Defines;

  void WGSLPreprocessor::Define(const std::string& symbol) { s_Defines.insert(symbol); }

  void WGSLPreprocessor::Undefine(const std::string& symbol) { s_Defines.erase(symbol); }

  bool WGSLPreprocessor::IsDefined(const std::string& symbol) { return s_Defines.count(symbol) > 0; }

  // Trim leading/trailing whitespace
  static std::string Trim(const std::string& s)
  {
    size_t start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
  }

  std::string WGSLPreprocessor::Process(const std::string& source)
  {
    std::istringstream stream(source);
    std::string result;
    std::string line;

    // Stack tracks nested #if blocks: each entry is {branchActive, anyBranchTaken}
    struct IfState
    {
      bool active;      // Is the current branch being emitted?
      bool anyTaken;    // Has any branch in this #if/#elif/#else chain been taken?
      bool parentActive; // Was the parent scope active?
    };
    std::vector<IfState> stack;

    auto isActive = [&]() -> bool
    {
      if (stack.empty()) return true;
      return stack.back().active && stack.back().parentActive;
    };

    while (std::getline(stream, line))
    {
      std::string trimmed = Trim(line);

      // #define SYMBOL
      if (trimmed.rfind("#define ", 0) == 0)
      {
        if (isActive())
        {
          std::string symbol = Trim(trimmed.substr(8));
          if (!symbol.empty()) Define(symbol);
        }
        continue;
      }

      // #undef SYMBOL
      if (trimmed.rfind("#undef ", 0) == 0)
      {
        if (isActive())
        {
          std::string symbol = Trim(trimmed.substr(7));
          if (!symbol.empty()) Undefine(symbol);
        }
        continue;
      }

      // #if SYMBOL (same as #ifdef)
      if (trimmed.rfind("#if ", 0) == 0)
      {
        std::string symbol = Trim(trimmed.substr(4));
        bool parentActive = isActive();
        bool condition = IsDefined(symbol);
        stack.push_back({parentActive && condition, parentActive && condition, parentActive});
        continue;
      }

      // #ifdef SYMBOL
      if (trimmed.rfind("#ifdef ", 0) == 0)
      {
        std::string symbol = Trim(trimmed.substr(7));
        bool parentActive = isActive();
        bool condition = IsDefined(symbol);
        stack.push_back({parentActive && condition, parentActive && condition, parentActive});
        continue;
      }

      // #ifndef SYMBOL
      if (trimmed.rfind("#ifndef ", 0) == 0)
      {
        std::string symbol = Trim(trimmed.substr(8));
        bool parentActive = isActive();
        bool condition = !IsDefined(symbol);
        stack.push_back({parentActive && condition, parentActive && condition, parentActive});
        continue;
      }

      // #elif SYMBOL
      if (trimmed.rfind("#elif ", 0) == 0)
      {
        if (stack.empty())
        {
          RN_LOG_ERR("WGSLPreprocessor: #elif without matching #if");
          continue;
        }
        std::string symbol = Trim(trimmed.substr(6));
        auto& top = stack.back();
        if (!top.anyTaken && top.parentActive && IsDefined(symbol))
        {
          top.active = true;
          top.anyTaken = true;
        }
        else
        {
          top.active = false;
        }
        continue;
      }

      // #else
      if (trimmed == "#else")
      {
        if (stack.empty())
        {
          RN_LOG_ERR("WGSLPreprocessor: #else without matching #if");
          continue;
        }
        auto& top = stack.back();
        top.active = !top.anyTaken && top.parentActive;
        continue;
      }

      // #endif
      if (trimmed == "#endif")
      {
        if (stack.empty())
        {
          RN_LOG_ERR("WGSLPreprocessor: #endif without matching #if");
          continue;
        }
        stack.pop_back();
        continue;
      }

      // Regular line: emit if active
      if (isActive())
      {
        result += line;
        result += '\n';
      }
    }

    if (!stack.empty())
    {
      RN_LOG_ERR("WGSLPreprocessor: unterminated #if ({0} level(s) deep)", stack.size());
    }

    return result;
  }
}  // namespace WebEngine
