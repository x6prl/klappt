#pragma once

#include <cstddef>

#include "SDL3/SDL_log.h"
#include "app/app_status.h"
#include "base/measure.h"
#include "base/stats.h"
#include "base/str_view.h"
#include "domain/word.h"

// TODO: rewrite

template <class AddWord>
inline bool wparse_entries(Arena &a, const char *data, size_t size,
                           AddWord add_word, AppStatus *app_status) {
	SDL_Log("%s %ld bytes", __PRETTY_FUNCTION__, size);
	StrView file{data, static_cast<Size>(size)};
	Measure perf;
	Stats wstats;

	Size linecount{0};

	auto next_line = [&]() {
		linecount++;
		return file.mut_split_by('\n').trim();
	};
	auto error = [&](const char msg[]) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "line %d: %s", linecount, msg);
		app_status->set_exit_with_error(StrView::lit(msg));
	};

	for (; file;) {
		auto line = next_line();
		while (file && !line) {
			line = next_line();
		}
		if (!line) {
			break;
		}
		if (line.size < 2) {
			// SDL_Log("\n=> %d parsing \n|" StrView_Fmt "|", linecount,
			//         StrView_Arg(line));
			error("line is too small");
			return false;
		}
		wstats.push(line.size);
		auto &word = add_word();
		auto tail = line.slice(line.size - 5);
		switch (line.first()) {
		case 'd':
		case '(':
			if (line.size > 5 && ' ' == line[3] &&
			    str_to_gender(line.data) != Gender::unknown &&
			    line.slice(5).is_contains('-')) {
				const char *dash = line.slice(5).find('-');
				char prev = dash[-1];
				if (' ' == prev || '"' == prev) {
					word.type = WordType::Noun;
				} else {
					word.type = WordType::Phrase;
				}
				break;
			}
			if (tail == "(sg.)" || tail == "(pl.)") {
				word.type = WordType::Noun;
				break;
			}
			// SDL_Log("Suggesting \"" StrView_Fmt "\" is a phrase.",
			//         StrView_Arg(line));
			word.type = WordType::Phrase;
			break;
		case 'v':
			if (line[1] == ' ') {
				word.type = WordType::Verb;
			} else {
				word.type = WordType::Phrase;
			}
			break;
		case 'a':
			if (line[1] == ' ') {
				word.type = WordType::Adj;
			} else {
				word.type = WordType::Phrase;
			}
			break;
		default:
			word.type = WordType::Phrase;
		}
		switch (word.type) {
		case WordType::Noun: {
			auto c = line;
			word.n.gender = str_to_gender(line.mut_split().data);
			if (Gender::unknown == word.n.gender) {
				error("cannot get gender of that line");
				return false;
			}
			if (line) {
				word.n.lemma = line.mut_split().copy(a);
			} else {
				error("lemma and plural suffix expected");
				return false;
			}
			if (line) {
				word.n.plural_suffix = line.copy(a);
			} else {
				SDL_Log("E \"" StrView_Fmt "\"", StrView_Arg(c));
				error("plural suffix expected");
				return false;
			}
		} break;
		case WordType::Verb:
			// SDL_Log("Verb");
			line = line.slice(2);
			{ // inf and 3rd person exception
				auto present_tense = line.mut_split_by('/');
				auto [inf, exception] = present_tense.split_by('-');

				constexpr char STRESS_CHAR = '\'';
				if (STRESS_CHAR == inf.mut_trimr().first()) {
					inf.mut_chopl();
					word.v.is_separable_prefix = true;
					word.v.infinitive = inf.copy(a);
				} else {
					if (inf.is_contains(STRESS_CHAR)) {
						auto [h, t] = inf.split_by(STRESS_CHAR);
						word.v.infinitive = StrView::concat(a, h, t);
					} else {
						word.v.infinitive = inf.copy(a);
					}
				}

				word.v.third_person = exception.triml().copy(a);
			}
			if (line.mut_trim()) {
				auto pt = line.mut_split_by('/').trim();
				if (pt && '-' != pt.first()) {
					word.v.praeteritum = pt.copy(a);
				}
				word.v.auxv_and_past_participle = line.mut_trim().copy(a);
			} else {
				break;
			}
			break;
		case WordType::Adj: {
			// SDL_Log("Adj");
			line = line.slice(2);
			auto [lemma, tail] = line.split();

			bool is_valid_adj = (tail && lemma.first() == tail.first()) ||
			                    tail.is_contains_substr("am "_v) ||
			                    tail == "(indecl.)"_v;

			if (!tail || is_valid_adj) {
				word.a.lemma = lemma.copy(a);
				if (!tail)
					break;

				word.a.is_indeclinable = (tail == "(indecl.)");
				if (word.a.is_indeclinable)
					break;

				word.a.comparative = tail.mut_split().copy(a);
				word.a.superlative = tail.copy(a);
			} else {
				word.type = WordType::Phrase;
				word.p.text = line.copy(a);
			}
		} break;
		case WordType::Phrase:
			// SDL_Log("Phrase");
			word.type = WordType::Phrase;
			word.p.text = line.copy(a);
			break;
		default:
			SDL_Log("Unknown word type" StrView_Fmt, StrView_Arg(line));
			break;
		}

		line = next_line();
		if (!line) {
			error("expected translations line or json after lemma");
			return false;
		}
		auto is_json = [](StrView str) {
			return '{' == str.first() && '}' == str.last();
		};
		if (is_json(line)) {
			word.json_payload = line.copy(a);
			line = next_line();
		} else {
			word.translations_raw = line.copy(a);
			line = next_line();
			if (line && is_json(line)) {
				word.json_payload = line.copy(a);
				line = next_line();
			}
		}

		if (line) {
			error("extra entry line");
			return false;
		}
		// print_word(word);
	}
	perf.lap().print();
	SDL_Log("words");
	wstats.print();
	return true;
}

// inline void wparse(Arena &a, const char *data, size_t size, Words &words) {
// 	wparse_entries(a, data, size, [&]() -> Word & {
// 		auto wref = words.add();
// 		return words[wref];
// 	});
// }

inline bool wparse(Arena &a, const char *data, size_t size, DynArr<Word> &words,
                   AppStatus *app_status) {
	return wparse_entries(
		  a, data, size,
		  [&]() -> Word & {
			  words.push(a, Word{});
			  return words.last();
		  },
		  app_status);
}

// bool wparse_file(Arena &a, const char *filename, Words &words);
bool wparse_file(Arena &a, const char *filename, DynArr<Word> &words,
                 AppStatus *app_status);
