#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace Rain {

  class Log {
   public:
    enum class Type : uint8_t {
      Core = 0,
      Client = 1
    };
    enum class Level : uint8_t {
      Trace = 0,
      Info,
      Warn,
      Error,
      Fatal
    };

   public:
    static void Init();

    static std::shared_ptr<spdlog::logger> GetCoreLogger() { return s_CoreLogger; }
    static std::shared_ptr<spdlog::logger> GetClientLogger() { return s_ClientLogger; }

    template <typename... Args>
    static void PrintMessage(Log::Type type, std::string_view tag, Args&&... args) {
      auto logger = (type == Log::Type::Core) ? GetCoreLogger() : GetClientLogger();
      logger->info("{0}: {1}", tag, fmt::format(std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void PrintMessageError(Log::Type type, std::string_view tag, Args&&... args) {
      auto logger = (type == Log::Type::Core) ? GetCoreLogger() : GetClientLogger();
      logger->error("{0}: {1}", tag, fmt::format(std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void PrintAssertMessage(Log::Type type, std::string_view prefix, Args&&... args);

   private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;
  };

  template <typename... Args>
  void Log::PrintAssertMessage(Log::Type type, std::string_view prefix, Args&&... args) {
    auto logger = (type == Log::Type::Core) ? GetCoreLogger() : GetClientLogger();
    logger->error("{0}: {1}", prefix, fmt::format(std::forward<Args>(args)...));
  }

  template <>
  inline void Log::PrintAssertMessage(Type type, std::string_view prefix) {
    auto logger = (type == Log::Type::Core) ? GetCoreLogger() : GetClientLogger();
    logger->error("{0}", prefix);
  }

#define RN_LOG(...) ::Rain::Log::PrintMessage(::Rain::Log::Type::Core, "STATUS: ", __VA_ARGS__)
#define RN_LOG_ERR(...) ::Rain::Log::PrintMessageError(::Rain::Log::Type::Core, "ERROR: ", __VA_ARGS__)

}  // namespace Rain
