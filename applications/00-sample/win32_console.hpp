#pragma once

#include <Windows.h>

#include <string>
#include <vector>

namespace blk
{
std::vector<std::string> cmdline_to_args(PWSTR pCmdLine);
} // namespace blk
