#pragma once

// C++ library headers
#include <string>
#include <memory>
#include <type_traits>

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
  int size_s = std::snprintf( nullptr, 0, format.c_str(), args... );
  if( size_s <= 0 ) {
    //throw std::runtime_error( "Error during formatting." );
    return "";
  }
  auto size = static_cast<uint64_t>( size_s ) + 1; // +1 for '\0'
  auto buf = std::make_unique<char[]>( size );
  std::snprintf( buf.get(), size, format.c_str(), args ... );
#pragma GCC diagnostic pop
  return std::string( buf.get(), buf.get() + size - 1 ); // Don't want the '\0'
}
