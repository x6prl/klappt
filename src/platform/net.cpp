#include "net.h"
#include "base/dyn_arr.h"
#include "base/str_view.h"
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_log.h>
#include <cstring>
#include <curl/curl.h>

namespace {

static size_t get_write_callback(void *data, size_t size, size_t nmemb,
                                 void *arr_ptr) {
	size_t realsize = size * nmemb;
	auto &arr = *static_cast<DynArr<uint8_t> *>(arr_ptr);

	if (arr.size + realsize
	    //+ 1
	    > static_cast<size_t>(arr.reserved)) {
		return 0;
	}

	memcpy(arr.data + arr.size, data, realsize);

	arr.size += realsize;
	// arr.data[arr.size] = '\0';

	return realsize;
}

static size_t get_and_write_to_file_write_callback(void *data, size_t size,
                                                   size_t nmemb,
                                                   void *file_ptr) {
	SDL_Log("%s", __PRETTY_FUNCTION__);
	size_t realsize = size * nmemb;
	SDL_IOStream *file = static_cast<SDL_IOStream *>(file_ptr);

	// SDL_Log("writing %zu bytes", realsize);
	if (SDL_WriteIO(file, data, realsize) != realsize) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Write failed: %s",
		             SDL_GetError());
		exit(1);
	}
	return realsize;
}
} // namespace

namespace net {

DynArr<uint8_t> get(Arena &a, StrView url) {
	CURLcode glob = curl_global_init(CURL_GLOBAL_ALL);
	if (glob != 0) {
		SDL_Log("Failed global init ...\n");
		exit(1);
	}
	CURL *curl = curl_easy_init();
	if (!curl) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "curl_easy_init() error");
	}

	char url_cstr[url.size + 1];
	memcpy(url_cstr, url.data, url.size);
	url_cstr[url.size] = '\0';

	auto response = DynArr<uint8_t>::filled_zero_or_default(a, 4096);

	char curl_err_buf[CURL_ERROR_SIZE];
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_err_buf);
	curl_easy_setopt(curl, CURLOPT_URL, url_cstr);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	SDL_Log("net: Performing GET to " StrView_Fmt, StrView_Arg(url));
	CURLcode res = curl_easy_perform(curl);

	if (res == CURLE_OK) {
		StrView s{(const char *)response.data, response.size};
		SDL_Log(StrView_Fmt, StrView_Arg(s));
	}

	curl_easy_cleanup(curl);
	// curl_global_cleanup();

	if (res != CURLE_OK) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GET failed: %s", curl_err_buf);
	}
	return response;
}

bool get_and_write(std::string url, std::string file_path) {
	SDL_Log("%s from %s to %s", __PRETTY_FUNCTION__, url.c_str(),
	        file_path.c_str());
	SDL_IOStream *file = SDL_IOFromFile(file_path.c_str(), "wb");
	SDL_Log("file %s opened", file_path.c_str());
	if (!file) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to open file: %s",
		             SDL_GetError());
		return false;
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "curl_easy_init() error");
	}

	char curl_err_buf[CURL_ERROR_SIZE];
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_err_buf);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
	                 get_and_write_to_file_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

	SDL_Log("net: Performing GET to %s", url.c_str());
	CURLcode res = curl_easy_perform(curl);

	SDL_Log("net: result %d", res);

	if (res == CURLE_OK) {
		SDL_Log("net: success");
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	if (res != CURLE_OK) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GET failed: %s", curl_err_buf);
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "curl: %s\n",
		             curl_easy_strerror(res));

		long os_errno = 0;
		curl_easy_getinfo(curl, CURLINFO_OS_ERRNO, &os_errno);

		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "errno=%ld (%s)\n", os_errno,
		             strerror(os_errno));
	}

	SDL_CloseIO(file);
	return true;
}

} // namespace net
