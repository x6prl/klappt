#pragma once

#include <cstddef>

#include "base/measure.h"
#include "base/stats.h"
#include "base/str_view.h"
#include "domain/word.h"

template <class AddWord>
inline void wparse_entries(Arena &a, const char *data, size_t size,
                           AddWord add_word) {
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
		exit(6);
	};

	for (; file;) {
		auto line = next_line();
		while (file && !line) {
			line = next_line();
		}
		if (!line) {
			break;
		}
		// SDL_Log("\n=> %d parsing \n|" StrView_Fmt "|", linecount,
		//         StrView_Arg(line));
		if (line.size < 2) {
			error("line is too small");
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
				word.type = WordType::Noun;
				break;
			}
			if (tail == "(sg.)" || tail == "(pl.)") {
				word.type = WordType::Noun;
				break;
			}
			SDL_Log("Suggesting \"" StrView_Fmt "\" is a phrase.",
			        StrView_Arg(line));
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
		case WordType::Noun:
			word.n.gender = str_to_gender(line.mut_split().data);
			if (Gender::unknown == word.n.gender) {
				error("cannot get gender of that line");
			}
			if (line) {
				word.n.lemma = line.mut_split().copy(a);
			} else {
				error("lemma and plural suffix expected");
			}
			if (line) {
				word.n.plural_suffix = line.copy(a);
			} else {
				error("plural suffix expected");
			}
			break;
		case WordType::Verb:
			// SDL_Log("Verb");
			line = line.slice(2);
			{
				auto present = line.mut_split_by('/').trim();
				auto exception = present;
				word.v.infinitive = exception.mut_split_by('-').trim().copy(a);
				if (exception) {
					word.v.third_person = exception.trim().copy(a);
				}
			}
			if (line.mut_trim()) {
				auto pt = line.mut_split_by('/').trim();
				if (pt && '-' != pt.first()) {
					word.v.praeteritum = pt.copy(a);
				}
			} else {
				break;
			}
			if (line.mut_trim()) {
				word.v.auxv_and_past_participle = line.copy(a);
			}
			break;
		case WordType::Adj:
			// SDL_Log("Adj");
			line = line.slice(2);
			word.a.lemma = line.mut_split().copy(a);
			if (!line) {
				break;
			}
			word.a.is_indeclinable = (line == "(indecl.)");
			if (word.a.is_indeclinable) {
				break;
			}
			word.a.comparative = line.mut_split().copy(a);
			if (line) {
				word.a.superlative = line.copy(a);
			}
			break;
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
		if (line) {
			word.translations_raw = line.copy(a);
		} else {
			error("translations expected");
		}
		line = next_line();
		if (line) {
			if ('[' == line.first() && ']' == line.last()) {
				word.grammar = line.copy(a);
				line = next_line();
			}
			if (line) {
				error("extra entry line is not allowed; store translatable "
				      "examples as separate phrase entries");
			}
		}
		// print_word(word);
	}
	perf.lap().print();
	SDL_Log("words");
	wstats.print();
}

// inline void wparse(Arena &a, const char *data, size_t size, Words &words) {
// 	wparse_entries(a, data, size, [&]() -> Word & {
// 		auto wref = words.add();
// 		return words[wref];
// 	});
// }

inline void wparse(Arena &a, const char *data, size_t size,
                   DynArr<Word> &words) {
	wparse_entries(a, data, size, [&]() -> Word & {
		words.push(a, Word{});
		return words.last();
	});
}

// bool wparse_file(Arena &a, const char *filename, Words &words);
bool wparse_file(Arena &a, const char *filename, DynArr<Word> &words);
