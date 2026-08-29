#pragma once

#include "base/str_view.h"
#include <SDL3/SDL_log.h>
#include <simdjson/simdjson.h>

struct WordAudioItem {
	StrView url{};
	DynArr<StrView> tags{};
};

struct WordSenseItem {
	StrView valency{}; // NOTE: optional
	DynArr<StrView> translations{};
};

struct WordExampleItem {
	StrView text{};
	StrView translation{}; // NOTE: optional (for de)
};

struct WordPayload {
	StrView pos{};
	StrView etymology{};

	DynArr<StrView> tags{};
	DynArr<StrView> ipa{};
	DynArr<WordAudioItem> audios{};
	DynArr<StrView> synonyms{};
	DynArr<StrView> antonyms{};
	DynArr<StrView> hypernyms{};
	DynArr<StrView> de_glosses{};
	DynArr<WordExampleItem> examples{};
	DynArr<WordSenseItem> senses{};
	DynArr<StrView> words{}; // NOTE: related to a phrase
};

inline StrView copy_to_arena(Arena &a, std::string_view sv) {
	return StrView{sv.data(), static_cast<Size>(sv.size())}.copy(a);
}

inline bool parse_word_json(Arena &scratch, Arena &a, StrView json_sv,
                            WordPayload &out, simdjson::dom::parser &parser) {
	out = {};

	if (!json_sv) {
		return false;
	}

	char *padded_buf =
		  scratch.pushN<char>(json_sv.size + simdjson::SIMDJSON_PADDING);
	memcpy(padded_buf, json_sv.data, json_sv.size);
	memset(padded_buf + json_sv.size, 0, simdjson::SIMDJSON_PADDING);

	simdjson::dom::element doc;
	auto error = parser.parse(padded_buf, json_sv.size).get(doc);
	if (error) {
		SDL_Log("simdjson parse error: %s", simdjson::error_message(error));
		return false;
	}

	// 1. pos
	if (auto val = doc["pos"].get_string(); !val.error()) {
		out.pos = copy_to_arena(a, val.value());
	}

	// 2. etymology
	if (auto val = doc["etymology"].get_string(); !val.error()) {
		out.etymology = copy_to_arena(a, val.value());
	}

	// 3. tags
	if (auto arr = doc["tags"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			if (auto str = item.get_string(); !str.error()) {
				out.tags.push(a, copy_to_arena(a, str.value()));
			}
		}
	}

	// 4. ipa
	if (auto arr = doc["ipa"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			if (auto str = item.get_string(); !str.error()) {
				out.ipa.push(a, copy_to_arena(a, str.value()));
			}
		}
	}

	// 5. audios
	if (auto arr = doc["audios"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			WordAudioItem audio{};
			if (auto url = item["url"].get_string(); !url.error()) {
				audio.url = copy_to_arena(a, url.value());
			}
			if (auto tags_arr = item["tags"].get_array(); !tags_arr.error()) {
				for (auto tag : tags_arr.value()) {
					if (auto tag_str = tag.get_string(); !tag_str.error()) {
						audio.tags.push(a, copy_to_arena(a, tag_str.value()));
					}
				}
			}
			out.audios.push(a, audio);
		}
	}

	// 6. synonyms
	if (auto arr = doc["synonyms"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			if (auto str = item.get_string(); !str.error()) {
				out.synonyms.push(a, copy_to_arena(a, str.value()));
			}
		}
	}

	// 7. antonyms
	if (auto arr = doc["antonyms"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			if (auto str = item.get_string(); !str.error()) {
				out.antonyms.push(a, copy_to_arena(a, str.value()));
			}
		}
	}

	// 8. hypernyms
	if (auto arr = doc["hypernyms"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			if (auto str = item.get_string(); !str.error()) {
				out.hypernyms.push(a, copy_to_arena(a, str.value()));
			}
		}
	}

	// 9. de_glosses
	if (auto arr = doc["de_glosses"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			if (auto str = item.get_string(); !str.error()) {
				out.de_glosses.push(a, copy_to_arena(a, str.value()));
			}
		}
	}

	// 10. examples
	if (auto arr = doc["examples"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			WordExampleItem ex{};
			if (auto text = item["text"].get_string(); !text.error()) {
				ex.text = copy_to_arena(a, text.value());
			}
			if (auto trans = item["translation"].get_string(); !trans.error()) {
				ex.translation = copy_to_arena(a, trans.value());
			}
			out.examples.push(a, ex);
		}
	}

	// 11. senses
	if (auto arr = doc["senses"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			WordSenseItem sense{};
			if (auto val = item["valency"].get_string(); !val.error()) {
				sense.valency = copy_to_arena(a, val.value());
			}
			if (auto tr_arr = item["translations"].get_array();
			    !tr_arr.error()) {
				for (auto tr : tr_arr.value()) {
					if (auto tr_str = tr.get_string(); !tr_str.error()) {
						sense.translations.push(
							  a, copy_to_arena(a, tr_str.value()));
					}
				}
			}
			out.senses.push(a, sense);
		}
	}

	// 12. words for phrases
	if (auto arr = doc["words"].get_array(); !arr.error()) {
		for (auto item : arr.value()) {
			if (auto str = item.get_string(); !str.error()) {
				out.words.push(a, copy_to_arena(a, str.value()));
			}
		}
	}

	return true;
}

inline void log_word_payload(const WordPayload &p) {
	SDL_Log("=== WORD JSON PAYLOAD ===");

	// 1. pos
	if (p.pos) {
		SDL_Log("  pos: " StrView_Fmt, StrView_Arg(p.pos));
	}

	// 2. tags
	for (Size i = 0; i < p.tags.size; ++i) {
		SDL_Log("  tag[%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.tags[i]));
	}

	// 3. ipa
	for (Size i = 0; i < p.ipa.size; ++i) {
		SDL_Log("  ipa[%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.ipa[i]));
	}

	// 4. audios
	for (Size i = 0; i < p.audios.size; ++i) {
		SDL_Log("  audio[%d].url: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.audios[i].url));
		for (Size j = 0; j < p.audios[i].tags.size; ++j) {
			SDL_Log("    tag[%d]: " StrView_Fmt, static_cast<int>(j),
			        StrView_Arg(p.audios[i].tags[j]));
		}
	}

	// 5. senses
	for (Size i = 0; i < p.senses.size; ++i) {
		const auto &sense = p.senses[i];
		if (sense.valency) {
			SDL_Log("  sense[%d] valency: [" StrView_Fmt "]",
			        static_cast<int>(i), StrView_Arg(sense.valency));
		} else {
			SDL_Log("  sense[%d]:", static_cast<int>(i));
		}
		for (Size j = 0; j < sense.translations.size; ++j) {
			SDL_Log("    translation[%d]: " StrView_Fmt, static_cast<int>(j),
			        StrView_Arg(sense.translations[j]));
		}
	}

	// 6. synonyms
	for (Size i = 0; i < p.synonyms.size; ++i) {
		SDL_Log("  synonym[%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.synonyms[i]));
	}

	// 7. antonyms
	for (Size i = 0; i < p.antonyms.size; ++i) {
		SDL_Log("  antonym[%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.antonyms[i]));
	}

	// 8. hypernyms
	for (Size i = 0; i < p.hypernyms.size; ++i) {
		SDL_Log("  hypernym[%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.hypernyms[i]));
	}

	// 9. etymology
	if (p.etymology) {
		SDL_Log("  etymology: " StrView_Fmt, StrView_Arg(p.etymology));
	}

	// 10. de_glosses
	for (Size i = 0; i < p.de_glosses.size; ++i) {
		SDL_Log("  de_gloss[%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.de_glosses[i]));
	}

	// 11. examples
	for (Size i = 0; i < p.examples.size; ++i) {
		SDL_Log("  example[%d].text: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.examples[i].text));
		if (p.examples[i].translation) {
			SDL_Log("  example[%d].translation: " StrView_Fmt,
			        static_cast<int>(i),
			        StrView_Arg(p.examples[i].translation));
		}
	}

	// 12. words
	for (Size i = 0; i < p.words.size; ++i) {
		SDL_Log("  word[%d]: " StrView_Fmt, static_cast<int>(i),
		        StrView_Arg(p.words[i]));
	}
}
