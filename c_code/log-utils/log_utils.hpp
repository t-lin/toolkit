#pragma once

//C library headers
#include <chrono>
#include <stdint.h>

// C++ library headers
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>

using namespace std::chrono_literals;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::chrono::seconds;

namespace LogUtils {

inline steady_clock LogClock;

typedef time_point<steady_clock> TimePoint;

// Helpers for cppPrintf to ensure argument types are fundamental or pointer
template <typename T>
struct isFundamentalOrPointer
    : public std::disjunction<std::is_fundamental<std::decay_t<T>>,
                              std::is_pointer<std::decay_t<T>>> {};

template <typename...>
struct areFundamentalOrPointer;

template <>
struct areFundamentalOrPointer<> : public std::true_type {};

template <typename T>
struct areFundamentalOrPointer<T>
    : public isFundamentalOrPointer<T> {};

template <typename T1, typename T2>
struct areFundamentalOrPointer<T1, T2>
    : public std::conjunction<areFundamentalOrPointer<T1>,
                              areFundamentalOrPointer<T2>> {};

template <typename T1, typename... Trest>
struct areFundamentalOrPointer<T1, Trest...>
    : public std::conjunction<areFundamentalOrPointer<T1>,
                              areFundamentalOrPointer<Trest...>> {};

// printf-like function for C++
// Source: https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
template<typename... Targs>
std::string cppPrintf( const std::string& format, Targs... args ) {
  // Compiler-time check to ensure arguments are basic fundamental types
  static_assert(areFundamentalOrPointer<Targs...>(),
                "cppPrintf arguments must be fundamental or pointer types");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... );
  if( size_s <= 0 ) {
    //throw std::runtime_error( "Error during formatting." );
    return "";
  }
  auto size = static_cast<size_t>( size_s ) + 1; // +1 for '\0'
  auto buf = std::make_unique<char[]>( size );
  std::snprintf( buf.get(), size, format.c_str(), args ... );
#pragma GCC diagnostic pop
  return std::string( buf.get(), buf.get() + size - 1 ); // Don't want the '\0'
}

// Metadata for log throttling
class LogMsgMeta {
  public:
    TimePoint muteUntil;
    size_t nSuppress;

    LogMsgMeta() : nSuppress(0) {};
};

// Avoid conflict between 'DEBUG' as an enum and a preprocessor macro
#ifdef DEBUG
#undef DEBUG
#define DEBUG DEBUG
#endif

// Logger class that wraps around a ROS2 node object, with a log
// throttling capability for cases with identical messages.
// When it emits logging messages, it uses the node's logging utilities.
// If the node object expires or dies, this object will do nothing.
class Logger {
  public:
    using LogLvl = uint8_t;
    static constexpr LogLvl DEBUG = 10;
    static constexpr LogLvl INFO = 20;
    static constexpr LogLvl WARN = 30;
    static constexpr LogLvl ERROR = 40;
    static constexpr LogLvl FATAL = 50;
    static constexpr LogLvl NONE = 0;

  private:
    LogLvl currLvl_ = INFO;
    mutable std::mutex mtx_;

    // For throttling
    static constexpr seconds MSGMETA_CLEANUP_PERIOD = 60s;
    using hash_t = std::invoke_result_t<std::hash<std::string>, std::string>;
    std::hash<std::string> strHasher_;
    std::unordered_map<hash_t, LogMsgMeta> msgMeta_;
    TimePoint cleanupTime_ = LogClock.now() + MSGMETA_CLEANUP_PERIOD;

    void log_(const std::string& msg, const LogLvl& lvl) const {
      static const std::string DEBUG_PREAMBLE = "[DEBUG]";
      static const std::string INFO_PREAMBLE = "[INFO]";
      static const std::string WARN_PREAMBLE = "[WARN]";
      static const std::string ERROR_PREAMBLE = "[ERROR]";
      static const std::string FATAL_PREAMBLE = "[FATAL]";
      static const std::string UNKNOWN = "[UNKNOWN_LOG_LVL]";

      if (currLvl_ == NONE || lvl == NONE) {
        return;
      }

      const char* pPreamble = nullptr;
      switch (lvl) {
        case DEBUG:
          pPreamble = DEBUG_PREAMBLE.c_str();
          break;
        case INFO:
          pPreamble = INFO_PREAMBLE.c_str();
          break;
        case WARN:
          pPreamble = WARN_PREAMBLE.c_str();
          break;
        case ERROR:
          pPreamble = ERROR_PREAMBLE.c_str();
          break;
        case FATAL:
          pPreamble = FATAL_PREAMBLE.c_str();
          break;
        default:
          pPreamble = UNKNOWN.c_str();
      }

      if (lvl >= currLvl_) {
        fprintf(stderr, "%s %s\n", pPreamble, msg.c_str());
      }
    }

  public:
    Logger(LogLvl lvl = INFO) {
      setThreshold(lvl);
    }

    // Copy assignment
    Logger& operator=(const Logger& rhs) {
      msgMeta_ = rhs.msgMeta_;
      cleanupTime_ = rhs.cleanupTime_;

      return *this;
    }

    // Set the logging threshold (logs "below" this level won't be printed)
    void setThreshold(const LogLvl lvl) {
      if (lvl != DEBUG && lvl != INFO && lvl != WARN &&
          lvl != ERROR && lvl != FATAL && lvl != NONE) {
        std::cerr << "ERROR: Unable to set logging level threshold to " <<
          lvl << "\n";
        return;
      }

      currLvl_ = lvl;
    }

    // Regular logging (w/o throttling)
    void operator()(const std::string& msg, LogLvl lvl = INFO) const {
      std::lock_guard<std::mutex> lock(mtx_);
      log_(msg, lvl);
    }

    // Logging w/ throttling. Throttles/mutes/snoozes identical messages
    // for a given duration (seconds).
    void operator()(const std::string& msg, seconds muteDur,
                    LogLvl lvl = INFO) {

      std::lock_guard<std::mutex> lock(mtx_);

      /* Cases:
       *  1) Msg hash doesn't exist
       *  2) Msg hash exists && now >= mutUntil
       *  3) Msg hash exists && now < mutUntil
       *
       * Print log on first two cases, increment supression count on third
       */
      hash_t msgHash = strHasher_(msg);
      auto msgEntry = msgMeta_.find(msgHash);
      auto now = LogClock.now();
      if (msgEntry == msgMeta_.end() || (now >= msgEntry->second.muteUntil)) {
        LogMsgMeta& metadata = msgMeta_[msgHash];
        metadata.muteUntil = now + muteDur;
        if (metadata.nSuppress > 0) {
          log_("(" + std::to_string(metadata.nSuppress) + " suppressed) " + \
                msg, lvl);
          metadata.nSuppress = 0;
        } else {
          log_(msg, lvl);
        }
      } else {
          msgEntry->second.nSuppress++;
      }

      // Periodically go through 'msgMeta' and clear expired entries.
      // For now, hard-code this to a minimum period of 1s.
      // TODO: Use separate thread for this task? Or (time permitting), install
      //       and try out libtask
      if (now >= cleanupTime_) {
        auto entryIt = msgMeta_.cbegin();
        while (entryIt != msgMeta_.cend()) {
          // Deleting while iterating, must increment iterator only once
          if (now >= entryIt->second.muteUntil) {
            entryIt = msgMeta_.erase(entryIt);
          } else {
            entryIt++;
          }
        }
        cleanupTime_ = now + MSGMETA_CLEANUP_PERIOD;
      }
    };
};

} // LogUtils namespace
