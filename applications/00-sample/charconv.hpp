#pragma once

#include <string>

namespace blk
{
std::string utf8_encode(const std::wstring& wstr);

std::wstring utf8_decode(const std::string& str);
} // namespace blk
