#pragma once
#include <set>
#include <string>

namespace WebEngine
{
  class WGSLPreprocessor
  {
   public:
    static void Define(const std::string& symbol);
    static void Undefine(const std::string& symbol);
    static bool IsDefined(const std::string& symbol);

    static std::string Process(const std::string& source);

   private:
    static std::set<std::string> s_Defines;
  };
}  // namespace WebEngine
