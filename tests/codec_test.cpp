#include "domain/word.h"
#include "domain/words_codec.h"
#include <cassert>

static void test_codec() {
	Arena a{};
	Verb v{
		  .infinitive = "inf"_v,
		  .third_person = "3p"_v,
		  .praeteritum = "präterit"_v,
		  .auxv_and_past_participle = "aux"_v,
		  .is_separable_prefix = true,
	};
	Word w0{
		  .word_id = {1},
		  .type = WordType::Verb,
		  .in_learning_list = 1,
		  .v = v,
		  .json_payload = {},
	};
	auto encoded = WordsCodec::encode_word(a, w0);

	Word w1{};
	bool ok = WordsCodec::decode_word(a, encoded.data, encoded.size, w1);
	assert(ok);

	bool comp = words_equal_ignoring_id(w0, w1) && w0.word_id == w0.word_id;
	assert(comp);
}

int main() {
	test_codec();
}
