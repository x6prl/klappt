#pragma once

#include "platform/net.h"
#include "base/arena.h"

#include <simdjson/simdjson.h>
#include <string_view>

inline void ot_test(Arena &a) {
	constexpr auto TESTURL = "https://www.openthesaurus.de/synonyme/"
							 "search?q=test&format=application/json"_v;
	auto res = net::get(a, TESTURL);
	if (!res.is_empty()) {
		simdjson::ondemand::parser parser;
		auto json = simdjson::padded_string(
			  std::string_view((const char *)res.data, res.size));
		simdjson::ondemand::document doc = parser.iterate(json);
	}
}
