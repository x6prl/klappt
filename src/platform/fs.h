#pragma once

#include "base/str_view.h"

StrView get_app_base_path();
StrView get_writable_path();
Size get_regular_file_size(const char *path);

bool fs_exists(StrView path);
bool fs_is_regular_file(StrView path);
bool fs_is_directory(StrView path);
bool fs_remove(StrView path);
bool fs_remove_all(StrView path);
bool fs_rename(StrView from, StrView to);
bool fs_touch(StrView path);
Size fs_file_size(StrView path);
