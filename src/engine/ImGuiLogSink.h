#pragma once

#include <deque>
#include <mutex>
#include <spdlog/sinks/base_sink.h>
#include <string>

namespace Rain
{
  enum class LogLevel : uint8_t
  {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Critical
  };

  struct LogEntry
  {
    std::string message;
    LogLevel level;
  };

  class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex>
  {
   public:
    static ImGuiLogSink& Get()
    {
      static ImGuiLogSink instance;
      return instance;
    }

    static std::shared_ptr<ImGuiLogSink> GetShared()
    {
      static std::shared_ptr<ImGuiLogSink> instance(&Get(), [](ImGuiLogSink*) {});
      return instance;
    }

    const std::deque<LogEntry>& GetLogs()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return m_Logs;
    }

    void Clear()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      m_Logs.clear();
    }

    size_t GetLogCount()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return m_Logs.size();
    }

    // Copy logs to avoid holding the lock during rendering
    std::vector<LogEntry> CopyLogs()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return std::vector<LogEntry>(m_Logs.begin(), m_Logs.end());
    }

   protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
      spdlog::memory_buf_t formatted;
      formatter_->format(msg, formatted);

      LogEntry entry;
      entry.message = fmt::to_string(formatted);
      // Remove trailing newline if present
      if (!entry.message.empty() && entry.message.back() == '\n')
      {
        entry.message.pop_back();
      }

      entry.level = ConvertLevel(msg.level);

      if (m_Logs.size() >= m_MaxLogs)
      {
        m_Logs.pop_front();
      }
      m_Logs.push_back(std::move(entry));
    }

    void flush_() override {}

   private:
    ImGuiLogSink() = default;

    static LogLevel ConvertLevel(spdlog::level::level_enum level)
    {
      switch (level)
      {
        case spdlog::level::trace:
          return LogLevel::Trace;
        case spdlog::level::debug:
          return LogLevel::Debug;
        case spdlog::level::info:
          return LogLevel::Info;
        case spdlog::level::warn:
          return LogLevel::Warn;
        case spdlog::level::err:
          return LogLevel::Error;
        case spdlog::level::critical:
          return LogLevel::Critical;
        default:
          return LogLevel::Info;
      }
    }

    std::deque<LogEntry> m_Logs;
    static constexpr size_t m_MaxLogs = 10000;
  };

}  // namespace Rain
