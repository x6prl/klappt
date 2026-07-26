#pragma once

#include "base/str_view.h"

StrView get_app_base_path();
StrView get_writable_path();
Size get_regular_file_size(const char *path);
