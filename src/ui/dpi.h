#pragma once

#include <cstdint>

#define dpi(val) (ctx->scale * (val))
#define udpi(val) (static_cast<uint16_t>(dpi(val)))
