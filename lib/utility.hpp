#pragma once

#include <array>
#include <bitset>
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

using WideChar = std::array<char, 4>;

// TODO: replace this function with an iterator or array adaptor
template <typename F>
void foreachUtf8Glyph(F&& callback, const char* data, size_t len)
{
    size_t index = 0;
    while (index < len) {
        const std::bitset<8> parsed(data[index]);
        if (parsed[7] == 0) {
            callback(WideChar{data[index], 0, 0, 0});
            index += 1;
        } else if (parsed[7] == 1 and parsed[6] == 1 and parsed[5] == 0) {
            callback(WideChar{data[index], data[index + 1], 0, 0});
            index += 2;
        } else if (parsed[7] == 1 and parsed[6] == 1 and parsed[5] == 1 and
                   parsed[4] == 0) {
            callback(WideChar{data[index], data[index + 1],
                              data[index + 2], 0});
            index += 3;
        } else if (parsed[7] == 1 and parsed[6] == 1 and parsed[5] == 1 and
                   parsed[4] == 1 and parsed[3] == 0) {
            callback(WideChar{data[index], data[index + 1],
                              data[index + 2], data[index + 3]});
            index += 4;
        } else {
            throw std::runtime_error("failed to parse unicode string");
        }
    }
}

inline size_t utf8Len(const char* data, size_t len)
{
    size_t ret = 0;
    foreachUtf8Glyph([&ret](const WideChar&) {
                         ++ret;
                     }, data, len);
    return ret;
}
