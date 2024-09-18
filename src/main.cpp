#include <Windows.h>

void init_log() {
	std::optional<std::filesystem::path> logpath = logger::log_directory();

	const char* plugin_name = Version::PROJECT.data();
	*logpath /= fmt::format(FMT_STRING("{}.log"), plugin_name);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logpath->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::trace);
	log->flush_on(spdlog::level::trace);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%T.%e] [%=5t] [%L] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);
	}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info) {

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	//const auto ver = a_f4se->RuntimeVersion();
	//if (ver < F4SE::RUNTIME_1_10_162 && ver != F4SE::RUNTIME_VR_1_2_72) {
	//	logger::critical("unsupported runtime v{}", ver.string());
	//	return false;
	//}

	return true;
}

// Functions to save and restore game settings
// Mantella temporarily modifies some settings to mute some NPC commentary
// During player conversation

RE::GameSettingCollection *GameSettings;

std::map<std::string, float> SettingsFloat;
std::map<std::string, int> SettingsInt;

bool saveFloat(std::monostate, std::string key) {
	RE::Setting *floatsetting = GameSettings->GetSetting(key);
	bool ret = false;
	if (floatsetting != NULL && floatsetting->GetType() == RE::Setting::SETTING_TYPE::kFloat) {
		SettingsFloat.insert_or_assign(key, floatsetting->GetFloat());
		ret = true;
		}
	return ret;
	}

bool saveInt(std::monostate, std::string key) {
	RE::Setting* intsetting = GameSettings->GetSetting(key);
	bool ret = false;
	if (intsetting != NULL && intsetting->GetType() == RE::Setting::SETTING_TYPE::kInt) {
		SettingsInt.insert_or_assign(key, intsetting->GetInt());
		ret = true;
		}
	return ret;
	}

bool restoreFloat(std::monostate, std::string key) {
	RE::Setting* floatsetting = GameSettings->GetSetting(key);
	bool ret = false;
	if (SettingsFloat.contains(key) && floatsetting != NULL) {
		floatsetting->SetFloat(SettingsFloat.at(key));
		ret = true;
		}
	return ret;
	}

bool restoreInt(std::monostate, std::string key) {
	RE::Setting* intsetting = GameSettings->GetSetting(key);
	bool ret = false;
	if (SettingsInt.contains(key) && intsetting != NULL) {
		intsetting->SetInt(SettingsInt.at(key));
		ret = true;
		}
	return ret;
	}


void MessageHandler(F4SE::MessagingInterface::Message* a_msg) {
	if (!a_msg) {
		return;
	}

	switch (a_msg->type) {

	case F4SE::MessagingInterface::kGameDataReady: 	{
		logger::info("GameDataReady");
		GameSettings = RE::GameSettingCollection::GetSingleton();
		}
		break;
	case F4SE::MessagingInterface::kPostLoadGame:
		break;
	}
}

// Substitute for SUP_F4SE's function
bool isMenuModeActive(std::monostate) {
	RE::UI* pUI = RE::UI::GetSingleton();
	return pUI->GetMenuOpen("DialogueMenu")
		|| pUI->GetMenuOpen("PipboyMenu")
		|| pUI->GetMenuOpen("VATSMenu")
		|| pUI->GetMenuOpen("LooksMenu")
		|| pUI->GetMenuOpen("BarterMenu")
		|| pUI->GetMenuOpen("WorkshopMenu")
		|| pUI->GetMenuOpen("ConsoleMenu")
		|| pUI->GetMenuOpen("SimpleTextField");
	}

// This is the placeholder string we expect to find in memory before overwriting
static char orgText[] = "Mantella01Mantella02Mantella03Mantella04Mantella05Mantella06Mantella07Mantella08Mantella09Mantella10Mantella11Mantella12Mantella13Mantella14Mantella15";
// Store last text written
static char lastText[256];

void CopyFuz(std::string fname) {							// Copy the generated FUZ file to the requested name
	char exe_path[_MAX_PATH];

	GetModuleFileNameA(NULL, exe_path, _MAX_PATH);			// Fallout exe
	std::filesystem::path app_path(exe_path);
	std::filesystem::path base_path(app_path.remove_filename());
	base_path += "Data\\Sound\\Voice\\Mantella.esp\\MantellaVoice00";		// Path to Mantella voices directory
	std::filesystem::path src_path(base_path / "00001ED2_1.fuz");
	std::filesystem::path target_file(base_path / std::format("{}_1.fuz", fname));
	CopyFile(src_path.c_str(), target_file.c_str(), false);
	}

// Patch Response text in memory for a given Topic
int PatchTopicInfo(std::monostate, RE::TESTopic *topic , std::string new_text) {
	int ret = 0;

	if (topic->numTopicInfos == 1) {							// Sanity check, to avoid overwriting the wrong location
		RE::TESTopicInfo* tpInfo = topic->topicInfos[0];		// Get the TopicInfo from Topic arg
		RE::ResponseListWrapper resp_list = tpInfo->responses;	
		RE::TESResponse* response = resp_list.head;

		char* resp_text = (char *) response->responseText.c_str();	// Existing response text

		logger::info("Topic {:X} TopicInfo {:X} Text: {}", topic->formID, tpInfo->formID, new_text);
		logger::info("Response Text {}", resp_text);

		if (new_text.length() > 150) {
			ret = -2;
			logger::warn("Replacement too long! ({:d}", new_text.length());
			}
		else if (strcmp(resp_text, orgText) != 0   && (lastText[0] == '\0' || strcmp(resp_text, lastText) != 0) ) {
			ret = -3;
			logger::warn("TopicInfo string in memeory does not match last usage!");
			}
		else {
			memset(resp_text, 0, 151);					// Fill w/0
			strcpy_s(resp_text, 150, new_text.data());	// Copy new text for subtittles
			strcpy_s(lastText, resp_text);				// Save the last string
			}

		if (ret != 0) {
			logger::info("Error");
			}
		}
	return ret;
	}

void ClearCacheInternal(RE::BSAudioManager *) {									// Doesn't seem to do anything?
	RE::BSAudioManager* AudioMan = RE::BSAudioManager::GetSingleton();

	using func_t = decltype(ClearCacheInternal);
	static REL::Relocation<func_t> func{ REL::ID(114073) };
	func(AudioMan);
	}

void ClearCache(std::monostate) {
	ClearCacheInternal(nullptr);
	}

// Override the file name used for wav and lip files
// We can use any name instead of the Topic FormID 

void SetOverrideFileName_internal(RE::TESTopicInfo *Info, char * name, uint64_t , uint64_t){
	using func_t = decltype(SetOverrideFileName_internal);
	static REL::Relocation<func_t> func{ REL::VariantOffset(0x0621710, 0, 0x60C060)};	// Pre-NG, NG, VR

	logger::info("Info: {:p} Name; {:p}", (void*)Info, (void*)name);
	func(Info, name, 0, 0);
	}

void SetOverrideFileName(std::monostate, RE::TESTopic* this_p, std::string name) {
	RE::TESTopicInfo* tpInfo = this_p->topicInfos[0];

	if (name != "00001ED2") {
		CopyFuz(name);
		}

	logger::info("SetOverrideFileName Info: {:p} name: {}", (void*) this_p, name.data());
	SetOverrideFileName_internal(tpInfo, name.data(), 0, 0);
	}

std::string StringRemoveWhiteSpace(std::monostate, std::string str) {
	auto start = str.begin();
	while (start != str.end() && std::isspace(*start)) {
		start++;
		}

	auto end = str.end();
	do {
		end--;
		} while (std::distance(start, end) > 0 && std::isspace(*end));

	return std::string(start, end + 1);
	}

void TakeScreenShot_internal(char*filename, int filetype, int sstype) {
	using func_t = decltype(TakeScreenShot_internal);
	static REL::Relocation<func_t> func{ REL::ID(919230) };	

	func(filename, filetype, sstype);

	}

void TakeScreenShot(std::monostate, std::string filename, int filetype, int sstype) {
	TakeScreenShot_internal(filename.data(), filetype, sstype);
	}


bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_VM) {
	a_VM->BindNativeMethod("TopicInfoPatcher", "PatchTopicInfo", PatchTopicInfo, true);
	a_VM->BindNativeMethod("TopicInfoPatcher", "ClearCache", ClearCache , true);
	a_VM->BindNativeMethod("TopicInfoPatcher", "SetOverrideFileName", SetOverrideFileName, true);
	a_VM->BindNativeMethod("TopicInfoPatcher", "StringRemoveWhiteSpace", StringRemoveWhiteSpace, true);
	a_VM->BindNativeMethod("TopicInfoPatcher", "TakeScreenShot", TakeScreenShot, true);
	a_VM->BindNativeMethod("TopicInfoPatcher", "isMenuModeActive", isMenuModeActive, true);
	a_VM->BindNativeMethod("TopicInfoPatcher", "saveFloat", saveFloat);
	a_VM->BindNativeMethod("TopicInfoPatcher", "saveInt", saveInt);
	a_VM->BindNativeMethod("TopicInfoPatcher", "restoreFloat", restoreFloat);
	a_VM->BindNativeMethod("TopicInfoPatcher", "restoreInt", restoreInt);
	return true;
	}




extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se) {
	F4SE::Init(a_f4se,false);

	init_log();

	F4SE::GetMessagingInterface()->RegisterListener(MessageHandler);

	const auto papyrus = F4SE::GetPapyrusInterface();
	if (!papyrus || !papyrus->Register(RegisterFunctions)) {
		logger::critical("Failed to register Papyrus functions!");
	} else {
		logger::info("Registered Papyrus functions.");
	}
	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	return true;
	}

F4SE_EXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};

	data.PluginName(Version::PROJECT);
	data.PluginVersion(REL::Version(0,9,4,0));
	data.AuthorName("peteben");
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(false);
	data.IsLayoutDependent(true);
	data.HasNoStructUse(false);
	data.CompatibleVersions({ F4SE::RUNTIME_1_10_984, F4SE::RUNTIME_1_10_163, F4SE::RUNTIME_LATEST_VR });

	return data;
	}();
