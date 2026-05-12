#include "wparser.h"

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_iostream.h"

// bool wparse_file(Arena &a, const char *filename, Words &words) {
// 	size_t file_size = 0;
// 	void *data = SDL_LoadFile(filename, &file_size);
// 	if (!data) {
// 		SDL_Log("wparsing %s error: %s", filename, SDL_GetError());
// 		return false;
// 	}
// 	wparse(a, static_cast<char *>(data), file_size, words);
// 	SDL_free(data);
// 	return true;
// }

bool wparse_file(Arena &a, const char *filename, DynArr<Word> &words, AppStatus *app_status) {
	size_t file_size = 0;
	void *data = SDL_LoadFile(filename, &file_size);
	if (!data) {
		SDL_Log("wparsing %s error: %s", filename, SDL_GetError());
		return false;
	}
	auto parsed = wparse(a, static_cast<char *>(data), file_size, words, app_status);
	SDL_free(data);
	return parsed;
}
