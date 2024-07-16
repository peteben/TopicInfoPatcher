

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info) {
#define NDEBUG


#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();

	if (!path) {
		return false;
		}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::trace);
	//log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

void MessageHandler(F4SE::MessagingInterface::Message* a_msg) {
	if (!a_msg) {
		return;
	}

	switch (a_msg->type) {

	case F4SE::MessagingInterface::kGameDataReady: 	{
		logger::info("GameDataReady");
		}
		break;
	case F4SE::MessagingInterface::kPostLoadGame:
		break;
	}
}

struct Response {
	void* unkptr1;		// Unknown pointer
	uint64_t unku64;	// Flags?
	uint64_t len;		// String length
	char str[151];	
	};

struct resplist {
	Response* cur;		// Ptr to actual response
	uint8_t unkt77[8];
	};

// Placeholder text in memory, to be overwritten
static char orgText[] = "Mantella01Mantella02Mantella03Mantella04Mantella05Mantella06Mantella07Mantella08Mantella09Mantella10Mantella11Mantella12Mantella13Mantella14Mantella15";
// Store last text written
static char lastText[256];

int PatchTopicInfo(std::monostate, RE::TESTopic *topic , std::string text) {
	int ret = 0;
	int maxcnt = 4;

	if (topic->numTopicInfos == 1) {						// Sanity check, to avoid overwriting the wrong location
		RE::TESTopicInfo* tpInfo = topic->topicInfos[0];	// Get the TopicInfo from Topic arg

		void* headptr = tpInfo->responses.head;					// Look for actual response textr
		resplist** rlist1 = (resplist**)tpInfo->responses.head;
		resplist* rlistptr = *rlist1;
		Response* rp;

		logger::info("Topic {:X} TopicInfo {:X} Text: {}", topic->formID, tpInfo->formID, text);
		logger::info("headptr {:p} rlistptr {:p}", headptr, (void*)rlistptr);

		// Look for actual text. It should be the second one.
		for (rp = rlistptr->cur;
			strcmp(rp->str, orgText) != 0 && (lastText[0] == '\0' || strcmp(rp->str, lastText) != 0)  && maxcnt > 0;
			rp = rlistptr->cur) {
			rlistptr++;
			maxcnt--;
			}

		logger::info("Reponse {:p} len {:d} Text: {}", (void*) rp, rp->len, rp->str);

		if (text.length() > 150) {
			ret = -2;
			logger::warn("Replacement too long! ({:d}", text.length());
			}
		else if (maxcnt <= 0) {
			logger::warn("TopicInfo string not found!");
			ret = -1;
			}
		else if (strcmp(rp->str, orgText) != 0   && (lastText[0] == '\0' || strcmp(rp->str, lastText) != 0) ) {
			ret = -3;
			logger::warn("TopicInfo string in memeory does not match last usage!");
			}
		else {
			memset(rp->str, 0, 151);			// Fill w/0
			strcpy_s(rp->str, text.data());		// Copy new text for subtittles
			//rp->len = strlen(rp->str);
			logger::info("Modified TopicInfo");
			strcpy_s(lastText, rp->str);		// Save the last string
			}

		if (ret != 0) {
			logger::info("Error");
			}
		}
	return ret;
	}

bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_VM) {
	a_VM->BindNativeMethod("TopicInfoPatcher", "PatchTopicInfo", PatchTopicInfo, true);
	return true;
	}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se) {
	F4SE::Init(a_f4se);

	F4SE::GetMessagingInterface()->RegisterListener(MessageHandler);

	const auto papyrus = F4SE::GetPapyrusInterface();
	if (!papyrus || !papyrus->Register(RegisterFunctions)) {
		logger::critical("Failed to register Papyrus functions!");
	} else {
		logger::info("Registered Papyrus functions.");
	}

	return true;
	}
