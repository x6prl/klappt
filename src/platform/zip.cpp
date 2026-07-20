#include "zip.h"

#include <cstddef>
#include <string>

#include "SDL3/SDL_log.h"
#include "base/measure.h"
#include "vendor/miniz/miniz.h"

bool unzip(StrView _in, StrView _out_dir) {
	Measure m{__FUNCTION__};
	std::string in(_in.data, _in.size);
	std::string out(_out_dir.data, _out_dir.size);

	mz_zip_archive zip{};
	if (!mz_zip_reader_init_file(&zip, in.c_str(), 0)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "could not initialize archive %s",
		             in.c_str());
		return false;
	};

	size_t n = mz_zip_reader_get_num_files(&zip);
	if (!n) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "archive is empty %s", in.c_str());
		return false;
	}

	for (size_t i = 0; i < n; ++i) {
		mz_zip_archive_file_stat st{};
		if (!mz_zip_reader_file_stat(&zip, i, &st)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "cannot stat %s",
			             st.m_filename);
			return false;
		}

		if (!mz_zip_reader_extract_to_file(&zip, i,
		                                   (out + st.m_filename).c_str(), 0)) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "cannot extract %s",
			             st.m_filename);
			return false;
		} else {
			SDL_Log("extracted %s", st.m_filename);
		}
	}

	mz_zip_reader_end(&zip);
	m.lap().print();
	return true;
}
