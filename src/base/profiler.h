#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>

#define KLAPPT_PROFILE_SCOPE() ZoneScoped
#define KLAPPT_PROFILE_SCOPE_N(name) ZoneScopedN(name)
#define KLAPPT_PROFILE_FRAME() FrameMark
#define KLAPPT_PROFILE_FRAME_N(name) FrameMarkNamed(name)
#define KLAPPT_PROFILE_NAME(text, size) ZoneName(text, size)
#define KLAPPT_PROFILE_NAME_F(...) ZoneNameF(__VA_ARGS__)
#define KLAPPT_PROFILE_THREAD(name) tracy::SetThreadName(name)
#define KLAPPT_PROFILE_CONNECTED() TracyIsConnected
#define KLAPPT_PROFILE_ZONE_TEXT(text, size) ZoneText(text, size)
#define KLAPPT_PROFILE_TEXT(text, size) TracyMessage(text, size)
#else
#define KLAPPT_PROFILE_SCOPE() ((void)0)
#define KLAPPT_PROFILE_SCOPE_N(name) ((void)0)
#define KLAPPT_PROFILE_FRAME() ((void)0)
#define KLAPPT_PROFILE_FRAME_N(name) ((void)0)
#define KLAPPT_PROFILE_NAME(text, size) ((void)0)
#define KLAPPT_PROFILE_NAME_F(...) ((void)0)
#define KLAPPT_PROFILE_THREAD(name) ((void)0)
#define KLAPPT_PROFILE_CONNECTED() false
#define KLAPPT_PROFILE_ZONE_TEXT(text, size) ((void)0)
#define KLAPPT_PROFILE_TEXT(text, size) ((void)0)
#endif
