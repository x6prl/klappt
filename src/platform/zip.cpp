#include "zip.h"

#include <cstddef>
#include <filesystem>
#include <string>

#include "SDL3/SDL_log.h"
#include "app/worker.h"
#include "base/measure.h"
#include "base/str_view.h"
#include "platform/fs.h"

#define _LARGEFILE64_SOURCE 1
#include "vendor/miniz/miniz.h"

namespace {
bool unpack(std::string in_fname, std::string out_dir) {
	Measure m{__FUNCTION__};
	// SDL_Log("unpacking %s -> %s", in_fname.c_str(), out_dir.c_str());

	mz_zip_archive zip{};
	auto print_error = [](auto *zip) {
		mz_zip_error err = mz_zip_get_last_error(zip);
		SDL_Log("miniz error: %d (%s)", err, mz_zip_get_error_string(err));
	};

	if (!mz_zip_reader_init_file(&zip, in_fname.c_str(), 0)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "could not initialize archive %s",
		             in_fname.c_str());
		print_error(&zip);
		return false;
	};

	size_t n = mz_zip_reader_get_num_files(&zip);
	if (!n) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "archive is empty %s",
		             in_fname.c_str());
		print_error(&zip);
		return false;
	}

	for (size_t i = 0; i < n; ++i) {
		mz_zip_archive_file_stat st{};
		if (!mz_zip_reader_file_stat(&zip, i, &st)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "cannot stat %s",
			             st.m_filename);
			print_error(&zip);
			return false;
		}
		if (mz_zip_reader_is_file_a_directory(&zip, i)) {
			// SDL_Log("creating a dir %s", st.m_filename);
			std::filesystem::create_directories(out_dir + st.m_filename);
			continue;
		}
		if (!mz_zip_reader_extract_to_file(
				  &zip, i, (out_dir + st.m_filename).c_str(), 0)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "cannot extract %s",
			             st.m_filename);
			print_error(&zip);
			return false;
		} else {
			// SDL_Log("extracted %s", st.m_filename);
		}
	}

	mz_zip_reader_end(&zip);
	m.lap().print();
	return true;
}
} // namespace

bool unpack_asset(StrView zip_path) {
	Measure m{__FUNCTION__};
	auto g = tctx()->a.guard();
	auto out_path = get_writable_path();
	SDL_Log("unzipping " StrView_Fmt " to " StrView_Fmt, StrView_Arg(zip_path),
	        StrView_Arg(out_path));
	if (unpack({zip_path.data, (size_t)zip_path.size},
	           {out_path.data, (size_t)out_path.size})) {
		SDL_Log("unzipped to " StrView_Fmt, StrView_Arg(out_path));
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "unzipping " StrView_Fmt " failed",
		             StrView_Arg(zip_path));
		return false;
	}
	m.lap().print();
	return true;
}
