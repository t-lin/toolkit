#pragma once
#ifndef TYPE_INSPECTION_H
#define TYPE_INSPECTION_H

#include <type_traits>
#include <experimental/type_traits>

/******************************************************************************
 * Is T a fundamental type or a pointer type?
 *****************************************************************************/
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

/******************************************************************************
 * Does T support certain operators?
 *****************************************************************************/
// Credit: Walter E. Brown
//  - https://open-std.org/JTC1/SC22/WG21/docs/papers/2015/n4502.pdf
//
// Leverage is_detected() from <experimental/type_traits>, though we could
// easily roll our own custom methods using the paper above.

// Check support for subtraction operator
template <typename T>
using subtracted_t = decltype(std::declval<T>() - std::declval<T>());

template <typename T>
constexpr bool hasSubtractionOp = std::experimental::is_detected_v<subtracted_t, T>;

// Check support for addition operator
template <typename T>
using added_t = decltype(std::declval<T>() + std::declval<T>());

template <typename T>
constexpr bool hasAdditionOp = std::experimental::is_detected_v<added_t, T>;

// Check support for multiplication operator
template <typename T>
using multiplied_t = decltype(std::declval<T>() * std::declval<T>());

template <typename T>
constexpr bool hasMultiplicationOp = std::experimental::is_detected_v<multiplied_t, T>;

// Check support for division operator
template <typename T>
using divided_t = decltype(std::declval<T>() / std::declval<T>());

template <typename T>
constexpr bool hasDivisionOp = std::experimental::is_detected_v<divided_t, T>;

// Check support for less-than operator
template <typename T>
using lessThan_t = decltype(std::declval<T>() < std::declval<T>());

template <typename T>
constexpr bool hasLessThanOp = std::experimental::is_detected_v<lessThan_t, T>;

// Check support for greater-than operator
template <typename T>
using greaterThan_t = decltype(std::declval<T>() > std::declval<T>());

template <typename T>
constexpr bool hasGreaterThanOp = std::experimental::is_detected_v<greaterThan_t, T>;

// TODO: Add checks for >=, <=, and some other ops (e.g. pre & post increment)

#endif
