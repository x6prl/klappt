#include "neuro.h"

#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_log.h"
#include "app/app_context.h"
#include "app/audio_context.h"
#include "app/worker.h"

#include "app/neuro_context.h"
#include "base/measure.h"
#include "platform/fs.h"

#include <cassert>
#include <filesystem>

namespace {

constexpr auto ASSET_NAME_PIPER_ONNX =
	  "Thorsten-Voice-Piper/de_DE-thorsten-medium.onnx";
constexpr auto ASSET_NAME_PIPER_TOKENS = "Thorsten-Voice-Piper/tokens.txt";
// eSpeak data - all files under this directory are needed
// ASR (Whisper)
constexpr auto ASSET_NAME_WHISPER_ENCODER =
	  "sherpa-onnx-whisper-base/base-encoder.int8.onnx";
constexpr auto ASSET_NAME_WHISPER_DECODER =
	  "sherpa-onnx-whisper-base/base-decoder.int8.onnx";
constexpr auto ASSET_NAME_WHISPER_TOKENS =
	  "sherpa-onnx-whisper-base/base-tokens.txt";
// eSpeak-ng data (all subdirs/files recursively)
constexpr auto ASSET_NAME_ESPEAK_NG_DATA = "espeak-ng-data/";

const std::vector<std::string> espeak_ng_data_files = {
	  // --- Root Level Files ---
	  "espeak-ng-data/af_dict",
	  "espeak-ng-data/am_dict",
	  "espeak-ng-data/an_dict",
	  "espeak-ng-data/ar_dict",
	  "espeak-ng-data/as_dict",
	  "espeak-ng-data/az_dict",
	  "espeak-ng-data/ba_dict",
	  "espeak-ng-data/be_dict",
	  "espeak-ng-data/bg_dict",
	  "espeak-ng-data/bn_dict",
	  "espeak-ng-data/bpy_dict",
	  "espeak-ng-data/bs_dict",
	  "espeak-ng-data/ca_dict",
	  "espeak-ng-data/chr_dict",
	  "espeak-ng-data/cmn_dict",
	  "espeak-ng-data/cs_dict",
	  "espeak-ng-data/cv_dict",
	  "espeak-ng-data/cy_dict",
	  "espeak-ng-data/da_dict",
	  "espeak-ng-data/de_dict",
	  "espeak-ng-data/el_dict",
	  "espeak-ng-data/en_dict",
	  "espeak-ng-data/eo_dict",
	  "espeak-ng-data/es_dict",
	  "espeak-ng-data/et_dict",
	  "espeak-ng-data/eu_dict",
	  "espeak-ng-data/fa_dict",
	  "espeak-ng-data/fi_dict",
	  "espeak-ng-data/fr_dict",
	  "espeak-ng-data/ga_dict",
	  "espeak-ng-data/gd_dict",
	  "espeak-ng-data/gn_dict",
	  "espeak-ng-data/grc_dict",
	  "espeak-ng-data/gu_dict",
	  "espeak-ng-data/hak_dict",
	  "espeak-ng-data/haw_dict",
	  "espeak-ng-data/he_dict",
	  "espeak-ng-data/hi_dict",
	  "espeak-ng-data/hr_dict",
	  "espeak-ng-data/ht_dict",
	  "espeak-ng-data/hu_dict",
	  "espeak-ng-data/hy_dict",
	  "espeak-ng-data/ia_dict",
	  "espeak-ng-data/id_dict",
	  "espeak-ng-data/intonations",
	  "espeak-ng-data/io_dict",
	  "espeak-ng-data/is_dict",
	  "espeak-ng-data/it_dict",
	  "espeak-ng-data/ja_dict",
	  "espeak-ng-data/jbo_dict",
	  "espeak-ng-data/ka_dict",
	  "espeak-ng-data/kk_dict",
	  "espeak-ng-data/kl_dict",
	  "espeak-ng-data/kn_dict",
	  "espeak-ng-data/ko_dict",
	  "espeak-ng-data/kok_dict",
	  "espeak-ng-data/ku_dict",
	  "espeak-ng-data/ky_dict",
	  "espeak-ng-data/la_dict",
	  "espeak-ng-data/lb_dict",
	  "espeak-ng-data/lfn_dict",
	  "espeak-ng-data/lt_dict",
	  "espeak-ng-data/lv_dict",
	  "espeak-ng-data/mi_dict",
	  "espeak-ng-data/mk_dict",
	  "espeak-ng-data/ml_dict",
	  "espeak-ng-data/mr_dict",
	  "espeak-ng-data/ms_dict",
	  "espeak-ng-data/mt_dict",
	  "espeak-ng-data/mto_dict",
	  "espeak-ng-data/my_dict",
	  "espeak-ng-data/nci_dict",
	  "espeak-ng-data/ne_dict",
	  "espeak-ng-data/nl_dict",
	  "espeak-ng-data/no_dict",
	  "espeak-ng-data/nog_dict",
	  "espeak-ng-data/om_dict",
	  "espeak-ng-data/or_dict",
	  "espeak-ng-data/pa_dict",
	  "espeak-ng-data/pap_dict",
	  "espeak-ng-data/phondata",
	  "espeak-ng-data/phondata-manifest",
	  "espeak-ng-data/phonindex",
	  "espeak-ng-data/phontab",
	  "espeak-ng-data/piqd_dict",
	  "espeak-ng-data/pl_dict",
	  "espeak-ng-data/pt_dict",
	  "espeak-ng-data/py_dict",
	  "espeak-ng-data/qdb_dict",
	  "espeak-ng-data/qu_dict",
	  "espeak-ng-data/quc_dict",
	  "espeak-ng-data/qya_dict",
	  "espeak-ng-data/ro_dict",
	  "espeak-ng-data/ru_dict",
	  "espeak-ng-data/sd_dict",
	  "espeak-ng-data/shn_dict",
	  "espeak-ng-data/si_dict",
	  "espeak-ng-data/sjn_dict",
	  "espeak-ng-data/sk_dict",
	  "espeak-ng-data/sl_dict",
	  "espeak-ng-data/smj_dict",
	  "espeak-ng-data/sq_dict",
	  "espeak-ng-data/sr_dict",
	  "espeak-ng-data/sv_dict",
	  "espeak-ng-data/sw_dict",
	  "espeak-ng-data/ta_dict",
	  "espeak-ng-data/te_dict",
	  "espeak-ng-data/th_dict",
	  "espeak-ng-data/ti_dict",
	  "espeak-ng-data/tk_dict",
	  "espeak-ng-data/tn_dict",
	  "espeak-ng-data/tr_dict",
	  "espeak-ng-data/tt_dict",
	  "espeak-ng-data/ug_dict",
	  "espeak-ng-data/uk_dict",
	  "espeak-ng-data/ur_dict",
	  "espeak-ng-data/uz_dict",
	  "espeak-ng-data/vi_dict",
	  "espeak-ng-data/yue_dict",

	  // --- Lang Directory Files ---
	  "espeak-ng-data/lang/aav/vi",
	  "espeak-ng-data/lang/aav/vi-VN-x-central",
	  "espeak-ng-data/lang/aav/vi-VN-x-south",
	  "espeak-ng-data/lang/art/eo",
	  "espeak-ng-data/lang/art/ia",
	  "espeak-ng-data/lang/art/io",
	  "espeak-ng-data/lang/art/jbo",
	  "espeak-ng-data/lang/art/lfn",
	  "espeak-ng-data/lang/art/piqd",
	  "espeak-ng-data/lang/art/py",
	  "espeak-ng-data/lang/art/qdb",
	  "espeak-ng-data/lang/art/qya",
	  "espeak-ng-data/lang/art/sjn",
	  "espeak-ng-data/lang/art/xex",
	  "espeak-ng-data/lang/azc/nci",
	  "espeak-ng-data/lang/bat/lt",
	  "espeak-ng-data/lang/bat/ltg",
	  "espeak-ng-data/lang/bat/lv",
	  "espeak-ng-data/lang/bnt/sw",
	  "espeak-ng-data/lang/bnt/tn",
	  "espeak-ng-data/lang/ccs/ka",
	  "espeak-ng-data/lang/cel/cy",
	  "espeak-ng-data/lang/cel/ga",
	  "espeak-ng-data/lang/cel/gd",
	  "espeak-ng-data/lang/cus/om",
	  "espeak-ng-data/lang/dra/kn",
	  "espeak-ng-data/lang/dra/ml",
	  "espeak-ng-data/lang/dra/ta",
	  "espeak-ng-data/lang/dra/te",
	  "espeak-ng-data/lang/esx/kl",
	  "espeak-ng-data/lang/eu",
	  "espeak-ng-data/lang/gmq/da",
	  "espeak-ng-data/lang/gmq/fo",
	  "espeak-ng-data/lang/gmq/is",
	  "espeak-ng-data/lang/gmq/nb",
	  "espeak-ng-data/lang/gmq/sv",
	  "espeak-ng-data/lang/gmw/af",
	  "espeak-ng-data/lang/gmw/de",
	  "espeak-ng-data/lang/gmw/en",
	  "espeak-ng-data/lang/gmw/en-029",
	  "espeak-ng-data/lang/gmw/en-GB-scotland",
	  "espeak-ng-data/lang/gmw/en-GB-x-gbclan",
	  "espeak-ng-data/lang/gmw/en-GB-x-gbcwmd",
	  "espeak-ng-data/lang/gmw/en-GB-x-rp",
	  "espeak-ng-data/lang/gmw/en-Shaw",
	  "espeak-ng-data/lang/gmw/en-US",
	  "espeak-ng-data/lang/gmw/en-US-nyc",
	  "espeak-ng-data/lang/gmw/lb",
	  "espeak-ng-data/lang/gmw/nl",
	  "espeak-ng-data/lang/grk/el",
	  "espeak-ng-data/lang/grk/grc",
	  "espeak-ng-data/lang/inc/as",
	  "espeak-ng-data/lang/inc/bn",
	  "espeak-ng-data/lang/inc/bpy",
	  "espeak-ng-data/lang/inc/gu",
	  "espeak-ng-data/lang/inc/hi",
	  "espeak-ng-data/lang/inc/kok",
	  "espeak-ng-data/lang/inc/mr",
	  "espeak-ng-data/lang/inc/ne",
	  "espeak-ng-data/lang/inc/or",
	  "espeak-ng-data/lang/inc/pa",
	  "espeak-ng-data/lang/inc/sd",
	  "espeak-ng-data/lang/inc/si",
	  "espeak-ng-data/lang/inc/ur",
	  "espeak-ng-data/lang/ine/hy",
	  "espeak-ng-data/lang/ine/hyw",
	  "espeak-ng-data/lang/ine/sq",
	  "espeak-ng-data/lang/ira/fa",
	  "espeak-ng-data/lang/ira/fa-Latn",
	  "espeak-ng-data/lang/ira/ku",
	  "espeak-ng-data/lang/ira/ps",
	  "espeak-ng-data/lang/iro/chr",
	  "espeak-ng-data/lang/itc/la",
	  "espeak-ng-data/lang/jpx/ja",
	  "espeak-ng-data/lang/ko",
	  "espeak-ng-data/lang/map/haw",
	  "espeak-ng-data/lang/miz/mto",
	  "espeak-ng-data/lang/myn/quc",
	  "espeak-ng-data/lang/poz/id",
	  "espeak-ng-data/lang/poz/mi",
	  "espeak-ng-data/lang/poz/ms",
	  "espeak-ng-data/lang/qu",
	  "espeak-ng-data/lang/roa/an",
	  "espeak-ng-data/lang/roa/ca",
	  "espeak-ng-data/lang/roa/ca-ba",
	  "espeak-ng-data/lang/roa/ca-nw",
	  "espeak-ng-data/lang/roa/ca-va",
	  "espeak-ng-data/lang/roa/es",
	  "espeak-ng-data/lang/roa/es-419",
	  "espeak-ng-data/lang/roa/fr",
	  "espeak-ng-data/lang/roa/fr-BE",
	  "espeak-ng-data/lang/roa/fr-CH",
	  "espeak-ng-data/lang/roa/ht",
	  "espeak-ng-data/lang/roa/it",
	  "espeak-ng-data/lang/roa/pap",
	  "espeak-ng-data/lang/roa/pt",
	  "espeak-ng-data/lang/roa/pt-BR",
	  "espeak-ng-data/lang/roa/ro",
	  "espeak-ng-data/lang/sai/gn",
	  "espeak-ng-data/lang/sem/am",
	  "espeak-ng-data/lang/sem/ar",
	  "espeak-ng-data/lang/sem/he",
	  "espeak-ng-data/lang/sem/mt",
	  "espeak-ng-data/lang/sem/ti",
	  "espeak-ng-data/lang/sit/cmn",
	  "espeak-ng-data/lang/sit/cmn-Latn-pinyin",
	  "espeak-ng-data/lang/sit/hak",
	  "espeak-ng-data/lang/sit/my",
	  "espeak-ng-data/lang/sit/yue",
	  "espeak-ng-data/lang/sit/yue-Latn-jyutping",
	  "espeak-ng-data/lang/tai/shn",
	  "espeak-ng-data/lang/tai/th",
	  "espeak-ng-data/lang/trk/az",
	  "espeak-ng-data/lang/trk/ba",
	  "espeak-ng-data/lang/trk/cv",
	  "espeak-ng-data/lang/trk/kaa",
	  "espeak-ng-data/lang/trk/kk",
	  "espeak-ng-data/lang/trk/ky",
	  "espeak-ng-data/lang/trk/nog",
	  "espeak-ng-data/lang/trk/tk",
	  "espeak-ng-data/lang/trk/tr",
	  "espeak-ng-data/lang/trk/tt",
	  "espeak-ng-data/lang/trk/ug",
	  "espeak-ng-data/lang/trk/uz",
	  "espeak-ng-data/lang/urj/et",
	  "espeak-ng-data/lang/urj/fi",
	  "espeak-ng-data/lang/urj/hu",
	  "espeak-ng-data/lang/urj/smj",
	  "espeak-ng-data/lang/zle/be",
	  "espeak-ng-data/lang/zle/ru",
	  "espeak-ng-data/lang/zle/ru-cl",
	  "espeak-ng-data/lang/zle/ru-LV",
	  "espeak-ng-data/lang/zle/uk",
	  "espeak-ng-data/lang/zls/bg",
	  "espeak-ng-data/lang/zls/bs",
	  "espeak-ng-data/lang/zls/hr",
	  "espeak-ng-data/lang/zls/mk",
	  "espeak-ng-data/lang/zls/sl",
	  "espeak-ng-data/lang/zls/sr",
	  "espeak-ng-data/lang/zlw/cs",
	  "espeak-ng-data/lang/zlw/pl",
	  "espeak-ng-data/lang/zlw/sk",
};

static std::string get_storage_base_path() {

#if __ANDROID__
	return SDL_GetAndroidExternalStoragePath();
#else
	return SDL_GetBasePath();
#endif
}

/**
 * Create all parent directories in the given path.
 */
static bool mkdir_all(std::string dir) {
#ifdef __ANDROID__
	Size pos = 0;
	while (true) {
		pos = dir.find('/', pos + 1);
		if (pos == std::string::npos)
			break;
		std::filesystem::path p(dir.substr(0, pos));
		std::error_code ec;
		SDL_Log("CREATE :::::  %s", p.c_str());
		std::filesystem::create_directories(p, ec);
	}
#endif
	(void)dir; // avoid unused warning on non-Android for now
	return true;
}

static std::filesystem::path sherpa_check_and_load_assets() {
	auto _base_path = get_writable_path();
	auto base_path = std::filesystem::path{
		  std::string{_base_path.data, (size_t)_base_path.size}};
	std::vector<std::string> files = {
		  ASSET_NAME_PIPER_ONNX,      ASSET_NAME_PIPER_TOKENS,
		  ASSET_NAME_WHISPER_TOKENS,  ASSET_NAME_WHISPER_DECODER,
		  ASSET_NAME_WHISPER_ENCODER, // ASSET_NAME_ESPEAK_NG_DATA

	};
	files.insert(files.begin(), espeak_ng_data_files.begin(),
	             espeak_ng_data_files.end());
	for (auto f : files) {
		auto full = base_path / f;

		// data = SDL_LoadFile(full.c_str(), &sz);
		if (std::filesystem::is_regular_file(full) &&
		    std::filesystem::file_size(full) > 0
		    // ||
		    // info.type == SDL_PATHTYPE_DIRECTORY)
		) {
			SDL_Log("Found asset on disk: %s", full.c_str());
		} else {
			SDL_Log("Asset not found on disk: %s", full.c_str());
			return {};
			{
				SDL_Log("GETTING %s to %s", f.c_str(), full.c_str());
				// https://github.com/espeak-ng/espeak-ng/raw/refs/heads/master/espeak-ng-data/lang/art/ia
				std::string host = "http://10.42.0.1:8000/";
				// if (f.contains("espeak-ng")) host =
				// "https://github.com/espeak-ng/espeak-ng/raw/refs/heads/master/";
				auto url = host + f;
				mkdir_all(full);
				assert(false);
				// bool ok = net::get_and_write(url, full);
				// if (!ok) {
				// 	SDL_LogError(SDL_LOG_CATEGORY_ERROR,
				// 	             "Error getting from %s to %s", url.c_str(),
				// 	             full.c_str());
				// }
			}
		}
	}
	// #endif
	return base_path;
}

bool sherpa_init_tts(NeuroContext *nnctx, std::filesystem::path base_path) {
	// initialize TTS (Piper is a VITS model)
	SherpaOnnxOfflineTtsConfig tts_config;
	memset(&tts_config, 0, sizeof(tts_config));

	// std::filesystem::path base_path = check_and_load_assets();
	auto path_po = (base_path / ASSET_NAME_PIPER_ONNX);
	auto path_pt = (base_path / ASSET_NAME_PIPER_TOKENS);
	auto path_es = (base_path / ASSET_NAME_ESPEAK_NG_DATA);
	tts_config.model.vits.model = path_po.c_str();
	tts_config.model.vits.tokens = path_pt.c_str();
	tts_config.model.vits.data_dir = path_es.c_str();

	{
		for (auto p :
		     {tts_config.model.vits.model, tts_config.model.vits.tokens,
		      tts_config.model.vits.data_dir}) {
			SDL_Log("FILE: %s", p);

			// std::ifstream f(p, std::ios::binary);
			// char buf[64]{};
			// f.read(buf, sizeof(buf) - 1);
			//
			// SDL_Log("HEAD: %s", buf);
		}
	}
	tts_config.model.vits.noise_scale = 0.667f;
	tts_config.model.vits.noise_scale_w = 0.8f;
	tts_config.model.vits.length_scale = 1.0f;

	tts_config.model.num_threads = 2;
	tts_config.model.provider = "cpu";
	tts_config.max_num_sentences = 2;

	nnctx->sherpa_ctx.offline_tts = SherpaOnnxCreateOfflineTts(&tts_config);
	if (!nnctx->sherpa_ctx.offline_tts) {
		SDL_LogError(
			  SDL_LOG_CATEGORY_ERROR,
			  "[Sherpa] Failed to initialize TTS engine. Check paths.\n");
		return false;
	}
	SDL_Log("[Sherpa] TTS Engine initialized.");
	return true;
}

bool sherpa_init_asr(NeuroContext *nnctx, std::filesystem::path base_path) {
	// initialize ASR (Whisper)
	SherpaOnnxOfflineRecognizerConfig asr_config;
	memset(&asr_config, 0, sizeof(asr_config));

	asr_config.feat_config.sample_rate = AudioContext::FREQUENCY;
	asr_config.feat_config.feature_dim = 80;

	auto path_we = (base_path / ASSET_NAME_WHISPER_ENCODER);
	auto path_wd = (base_path / ASSET_NAME_WHISPER_DECODER);
	auto path_wt = (base_path / ASSET_NAME_WHISPER_TOKENS);
	asr_config.model_config.whisper.encoder = path_we.c_str();
	asr_config.model_config.whisper.decoder = path_wd.c_str();
	asr_config.model_config.tokens = path_wt.c_str();
	{
		SDL_Log("<<<<<<<<<<>>>>>>>>>>>>>>");
		for (auto p : {asr_config.model_config.whisper.encoder,
		               asr_config.model_config.whisper.decoder,
		               asr_config.model_config.tokens}) {
			SDL_Log("FILE: %s", p);

			// std::ifstream f(p, std::ios::binary);
			// char buf[64]{};
			// f.read(buf, sizeof(buf) - 1);
			//
			// SDL_Log("HEAD: %s", buf);
		}
	}
	asr_config.model_config.whisper.language = "de";
	asr_config.model_config.whisper.task = "transcribe";
	asr_config.model_config.whisper.tail_paddings = -1;

	asr_config.model_config.num_threads = 2;
	asr_config.model_config.provider = "cpu";

	asr_config.decoding_method = "greedy_search";
	asr_config.max_active_paths = 4;

	nnctx->sherpa_ctx.offline_recognizer =
		  SherpaOnnxCreateOfflineRecognizer(&asr_config);
	if (!nnctx->sherpa_ctx.offline_recognizer) {
		SDL_LogError(
			  SDL_LOG_CATEGORY_ERROR,
			  "[Sherpa] Failed to initialize ASR engine. Check paths.\n");
		return false;
	}
	SDL_Log("[Sherpa] ASR Engine initialized.\n");
	return true;
}

// TODO: just wipe it...........
void cleanup_sherpa_engines(NeuroContext *nnctx) {
	auto &s = nnctx->sherpa_ctx;
	if (s.offline_tts) {
		SherpaOnnxDestroyOfflineTts(s.offline_tts);
		s.offline_tts = nullptr;
	}
	if (s.offline_recognizer) {
		SherpaOnnxDestroyOfflineRecognizer(s.offline_recognizer);
		s.offline_recognizer = nullptr;
	}
	if (s.offline_stream) {
		SherpaOnnxDestroyOfflineStream(s.offline_stream);
		s.offline_stream = nullptr;
	}
}
} // namespace

void init_tts(AppContext *ctx) {
	Worker::neuro_job_push(
		  ctx,
		  NeuroJob{.func = [](NeuroContext *nnctx, NeuroJob::Payload *payload) {
			  (void)payload;

			  // TODO: split checker
			  std::filesystem::path base_path = sherpa_check_and_load_assets();
			  if (sherpa_init_tts(nnctx, base_path)) {
				  MT::run([](AppContext *ctx) {
					  ctx->audio_asr_tts_status.is_tts_initialized = true;
				  });
			  } else {
				  SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			                   "Failed initializing TTS");
			  }
		  }});
}

void init_asr(AppContext *ctx) {
	Worker::neuro_job_push(
		  ctx,
		  NeuroJob{.func = [](NeuroContext *nnctx, NeuroJob::Payload *payload) {
			  (void)payload;

			  // TODO: split checker
			  std::filesystem::path base_path = sherpa_check_and_load_assets();
			  if (base_path.empty()) {
				  SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			                   "Failed initializing ASR: assets check and load "
			                   "failed");
				  return;
			  }

			  if (sherpa_init_asr(nnctx, base_path)) {
				  MT::run([](AppContext *ctx) {
					  ctx->audio_asr_tts_status.is_asr_initialized = true;
				  });
			  } else {
				  SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			                   "Failed initializing ASR");
			  }
		  }});
}

void run_asr(AppContext *ctx) {
	// worker_job_push(ctx, {.type = Job::Type::ASR});
	Worker::neuro_job_push(
		  ctx,
		  NeuroJob{.func = [](NeuroContext *nnctx, NeuroJob::Payload *payload) {
			  (void)payload;

			  Measure m{"JOB: ASR"};
			  { // notify ui
			    // SDL_SetAtomicInt(&ctx.is_asr_in_progress,
			    // SoundContext::TRUE);
			    // push_event(ASR_NOTIFY_UI_GENERAL_CODE, nullptr);
				  MT::run([](AppContext *ctx) {
					  ctx->audio_asr_tts_status.is_asr_in_progress = true;
				  });
			  }
			  m.lap().printus("ui notified");
			  char *spoken_text{nullptr};
			  const SherpaOnnxOfflineRecognizerResult *sherpa_result{nullptr};
			  // transcribe_audio(
		      // 	  sound_ctx, (float *)ctx.audio.data,
		      // 	  ctx.audio.size_bytes / sizeof(float),
		      // 	  SoundContext::FREQUENCY);
			  {
				  auto &s = nnctx->sherpa_ctx;
				  if (s.offline_recognizer) {
					  // TODO: !!!!!!!!! think harder
					  Measure cofs{"ONNX: CreateOfflineStream"};
					  if (!s.offline_stream)
						  s.offline_stream = SherpaOnnxCreateOfflineStream(
								s.offline_recognizer);
					  if (s.offline_stream) {
						  cofs.lap().printus();

						  // NOTE: !!! Touching another thread data!!!
					      // *****************************************
						  {
							  AudioContext *actx = tctx()->app_ctx->audio;
							  if (0 == actx->rec_audio_buffer.size_bytes) {
								  SDL_LogError(SDL_LOG_CATEGORY_ERROR,
							                   "Cannot transcribe: rec buffer "
							                   "is empty");
								  return;
							  }
							  Measure awo{"ONNX: AcceptWaveformOffline"};
							  auto samples_count =
									actx->rec_audio_buffer.size_bytes /
									sizeof(float);
							  SherpaOnnxAcceptWaveformOffline(
									s.offline_stream, AudioContext::FREQUENCY,
									static_cast<float *>(
										  actx->rec_audio_buffer.data),
									samples_count);
							  awo.lap().printus();
						  }
						  // NOTE: !!! Touching another thread data!!!
					      // *****************************************

						  Measure dos{"ONNX: DecodeOfflineStream"};
						  SherpaOnnxDecodeOfflineStream(s.offline_recognizer,
					                                    s.offline_stream);
						  dos.lap().printus();

						  Measure gr{"ONNX: GetResult"};
						  sherpa_result = SherpaOnnxGetOfflineStreamResult(
								s.offline_stream);
						  gr.lap().printus();

						  Measure onx{"ONNX: finish"};
						  char *text = nullptr;
						  if (sherpa_result && sherpa_result->text) {
							  text = strdup(sherpa_result->text);
						  }
						  onx.lap().printus("strdup");

						  s.offline_stream = nullptr;
						  onx.lap().printus("destroy stream");

						  spoken_text = text;
					  } else {
						  SDL_LogError(SDL_LOG_CATEGORY_ERROR,
					                   "SherpaOnnxCreateOfflineStream failed");
					  }
				  } else {
					  SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No offline stream");
				  }
			  }
			  m.lap().printus("audio transcribed");
			  { // notify ui
				  if (spoken_text) {
					  // // NOTE: we have a result and asr is not in
				      // progress
				      //    // anymore
				      // SDL_Log("Whisper heard: '%s'", spoken_text);
				      // SDL_SetAtomicInt(&ctx.is_asr_in_progress,
				      //                     SoundContext::FALSE);
				      // push_event(ASR_FINISHED_EVENT_CODE, spoken_text);
				      // // NOTE: freed after copying in ui via the other
				      // job
				      //    // free(spoken_text);

					  // NOTE: we have a result and asr is not in progress
				      // anymore
					  MT::run_with_payload(spoken_text, [](AppContext *ctx,
				                                           void *data2) {
						  ctx->audio_asr_tts_status.is_asr_in_progress = false;

						  auto asr_text =
								StrView::lit(static_cast<char *>(data2))
									  .copy(ctx->arena_screen());
						  SDL_Log(" ->> " StrView_Fmt, StrView_Arg(asr_text));
						  // memcpy(ctx->asr_result.data,
					      // asr_text.data,
					      //        std::min(asr_text.size,
					      //        ctx->asr_result.max_size));
					      // ctx->asr_result.size = asr_text.size;
						  ctx->asr_result = asr_text;

						  // free the memory
					      // TODO: refactor
					      // worker_job_push(
					      // 	  ctx,
					      // 	  {.type = Job::Type::ASR_DATA_FREE,
					      //                 .tts_text = {.data =
					      //                 (const char *)event
					      //                                            ->user.data1,
					      //                              .size =
					      //                              0}});
					      // free the memory
						  NeuroJob::Payload cb_payload = {.data_ptr = data2};
						  Worker::neuro_job_push(
								ctx, {.func =
					                        [](NeuroContext *nnctx,
					                           NeuroJob::Payload *payload) {
												(void)nnctx;
												Measure m{"JOB: ASR DATA "
						                                  "FREE"};
												// 	// NOTE: same ptr as
						                        // spoken_text in Type::ASR
						                        // 	// TODO: refactor
												free(payload->data_ptr);
												m.lap().printms();
												SDL_Log("Neuro Thread: "
						                                "Memory "
						                                "freed");
											},
					                  .payload = cb_payload});
						  SDL_Log("Main Thread: ASR_FINISHED");
					  });
				  } else {
					  // NOTE: we have a result and asr is not in progress
				      // anymore
				      //
				      // SDL_Log("Transcription failed or returned empty.");
				      // SDL_SetAtomicInt(&ctx.is_asr_in_progress,
				      //                     SoundContext::FALSE);
				      // push_event(ASR_NOTIFY_UI_GENERAL_CODE, nullptr);
					  MT::run([](AppContext *ctx) {
						  SDL_Log("Transcription failed or returned empty.");
						  ctx->audio_asr_tts_status.is_asr_in_progress = false;
					  });
				  }
			  }
			  m.lap().printus("ui notified");
			  if (sherpa_result) {
				  SherpaOnnxDestroyOfflineRecognizerResult(sherpa_result);
				  m.lap().printus("sherpa result destroyed");
			  }
		  }});
}

void run_tts(AppContext *ctx, StrView tts_text) {
	NeuroJob::Payload payload = {.tts_text = tts_text};
	Worker::neuro_job_push(
		  ctx,
		  {.func = [](NeuroContext *nnctx, NeuroJob::Payload *payload) -> void {
			   Measure m{"JOB: TTS"};
			   SherpaOnnxGenerationConfig gen_config;
			   memset(&gen_config, 0, sizeof(gen_config));

			   { // set speech speed
				   constexpr auto SPEED_NORMAL = 1.f;
				   constexpr auto SPEED_SLOW = .55f;
				   // TODO: move to tctx->neuro
				   static Hash prev_str_hash;
				   Hash this_str_hash = hash_str_view(payload->tts_text);
				   bool is_same_hashes = prev_str_hash == this_str_hash;

				   if (is_same_hashes) {
					   gen_config.speed = SPEED_SLOW;
					   prev_str_hash = {};
				   } else {
					   prev_str_hash = this_str_hash;
					   gen_config.speed = SPEED_NORMAL;
				   }
			   }

			   char text_buf[1024]{};
			   auto &str = payload->tts_text;
			   memcpy(text_buf, str.data, (str.size < 1024 ? str.size : 1023));

			   const SherpaOnnxGeneratedAudio *generated_audio =
					 SherpaOnnxOfflineTtsGenerateWithConfig(
						   nnctx->sherpa_ctx.offline_tts, text_buf, &gen_config,
						   nullptr, nullptr);

			   if (!generated_audio) {
				   SDL_LogError(SDL_LOG_CATEGORY_ERROR,
			                    "[Sherpa TTS] Generation failed.");
				   return;
			   }

			   int num_samples = generated_audio->n;
			   int sample_rate = generated_audio->sample_rate;
			   auto audio_data = generated_audio->samples;

			   SDL_Log("Synthesized %d samples at %d Hz", num_samples,
		               sample_rate);
			   Size pp_index = nnctx->get_unused_payload();
			   auto playback_payload_ptr =
					 &nnctx->playback_payload_pool[pp_index];
			   auto &pp = *playback_payload_ptr;
			   pp.generated_audio = generated_audio;
			   pp.spec.format = SDL_AUDIO_F32;
			   pp.spec.channels = 1;
			   pp.spec.freq = sample_rate;
			   pp.audio_data = audio_data;
			   pp.audio_data_size =
					 num_samples * static_cast<Size>(sizeof(float));
			   auto on_playback_finished = [](AudioContext *actx,
		                                      AudioJob::Payload *payload) {
				   SDL_Log("Playback filished, slot %d",
			               payload->playback_payload_ptr->pp_index);
				   (void)actx;
				   NeuroJob::Payload nn_payload = {
						 .int32 = payload->playback_payload_ptr->pp_index};
				   Worker::neuro_job_push(
						 tctx()->app_ctx,
						 {.func =
			                    [](NeuroContext *nnctx,
			                       NeuroJob::Payload *payload) {
									SDL_Log("FREEING POOL SLOT %d",
				                            payload->int32);

									nnctx->release_payload_and_free_generated_audio_if_needed(
										  payload->int32);
								},
			              .payload = nn_payload});
			   };
			   playback_play_and_run_on_finished(tctx()->app_ctx,
		                                         playback_payload_ptr,
		                                         on_playback_finished);
			   m.lap().printms();

			   // { // play generated audio
		       //  int num_samples = generated_audio->n;
		       //  int sample_rate = generated_audio->sample_rate;
		       //  auto audio_data = generated_audio->samples;
		       //
		       //  SDL_Log("Synthesized %d samples at %d Hz", num_samples,
		       //             sample_rate);
		       //  SDL_AudioSpec spec;
		       //
		       //  spec.channels = 1;
		       //  spec.format = SDL_AUDIO_F32;
		       //  spec.freq = sample_rate;
		       //
		       //  SDL_AudioStream *playback_stream = SDL_OpenAudioDeviceStream(
		       //  SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		       //  if (!playback_stream) {
		       //   SDL_LogError(SDL_LOG_CATEGORY_ERROR,
		       //                   "Couldn't create audio stream: %s",
		       //                   SDL_GetError());
		       //   return;
		       //  }
		       //
		       //  if (!SDL_PutAudioStreamData(playback_stream, audio_data,
		       //                                 num_samples * sizeof(float)))
		       //                                 {
		       //   SDL_Log("Failed to put audio data: %s", SDL_GetError());
		       //  }
		       //
		       //  SDL_ResumeAudioStreamDevice(playback_stream);
		       //
		       //  while (SDL_GetAudioStreamAvailable(playback_stream) > 0) {
		       //   SDL_Delay(10);
		       //  }
		       //
		       //  SDL_Delay(50);
		       //
		       //  SDL_DestroyAudioStream(playback_stream);
		       // }
		       // SherpaOnnxDestroyOfflineTtsGeneratedAudio(generated_audio);
		       // m.lap().printms();
		   },
	       .payload = payload});
}
