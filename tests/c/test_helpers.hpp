#pragma once

// Shared containment helpers for the host tests.

#include <cstring>
#include <string_view>

inline bool contains(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

inline bool contains(const char* text, const char* needle)
{
    return text && needle && std::strstr(text, needle) != nullptr;
}
