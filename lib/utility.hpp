#pragma once

#include <memory>
#include <stddef.h>
#include <type_traits>

template <typename...> struct Index;

template <typename T, typename... R>
struct Index<T, T, R...> : std::integral_constant<size_t, 0> {
};

template <typename T, typename F, typename... R>
struct Index<T, F, R...>
    : std::integral_constant<size_t, 1 + Index<T, R...>::value> {
};

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename F> struct OnUnwind {
    OnUnwind(F&& proc) : proc_(std::forward<F>(proc))
    {
    }
    ~OnUnwind()
    {
        proc_();
    }

private:
    F proc_;
};

template <typename Body, typename After>
auto dynamicWind(Body&& body, After&& after) -> decltype(body())
{
    OnUnwind<After> _(std::forward<After>(after));
    return body();
}
