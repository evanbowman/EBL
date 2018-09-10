#pragma once

#include <type_traits>

template <typename... >
struct Index;

// found it
template <typename T, typename... R>
struct Index<T, T, R...>
: std::integral_constant<size_t, 0>
{ };

// still looking
template <typename T, typename F, typename... R>
struct Index<T, F, R...>
: std::integral_constant<size_t, 1 + Index<T,R...>::value>
{ };
