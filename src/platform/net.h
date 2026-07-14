#pragma once

#include <string>

#include "base/dyn_arr.h"
#include "base/str_view.h"

namespace net {
DynArr<uint8_t> get(Arena &a, StrView url);
bool get_and_write(std::string url, std::string file_path);
}
