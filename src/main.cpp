extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

class DialogueHook
{
public:
	static void Hook()
	{
		_ProcessMessage = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_DialogueMenu[0])).write_vfunc(0x4, ProcessMessage);
	}

private:
	class SkipTextFunc : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			RE::GFxValue bAllowProgress;
			a_params.thisPtr->GetMember("bAllowProgress", &bAllowProgress);

			if (!bAllowProgress.IsBool())
				return;

			if (bAllowProgress.GetBool()) {
				_generic_foo_<50614, void(void*)>::eval(nullptr);
			}
		}
	};

	class OnItemSelectFunc : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			RE::GFxValue bAllowProgress;
			a_params.thisPtr->GetMember("bAllowProgress", &bAllowProgress);

			if (!bAllowProgress.IsBool())
				return;

			//a_params.argCount

			if (bAllowProgress.GetBool()) {
			}
		}
	};

	static void Install(RE::GFxMovieView* a_view, const char* a_pathToObj) {
		RE::GFxValue obj;
		a_view->GetVariable(&obj, a_pathToObj);
		if (!obj.IsObject()) {
			return;
		}

		{
			RE::GFxValue SkipText;
			auto SkipTextImpl = RE::make_gptr<SkipTextFunc>();

			a_view->CreateFunction(&SkipText, SkipTextImpl.get());
			obj.SetMember("SkipText", SkipText);
		}

		{
			RE::GFxValue onItemSelect;
			auto onItemSelectImpl = RE::make_gptr<OnItemSelectFunc>();

			a_view->CreateFunction(&onItemSelect, onItemSelectImpl.get());
			obj.SetMember("onItemSelect", onItemSelect);
		}
	}

	static RE::UI_MESSAGE_RESULTS ProcessMessage(void* self, RE::UIMessage& a_message)
	{
		if (a_message.type == RE::UI_MESSAGE_TYPE::kShow) {
			if (auto ui = RE::UI::GetSingleton()) {
				if (auto menu = ui->GetMenu(RE::DialogueMenu::MENU_NAME).get()) {
					if (auto movie = menu->uiMovie.get()) {
						Install(movie, "_root.DialogueMenu_mc");
						//movie->SetVariable("_root.DialogueMenu_mc.bAllowProgress", true, RE::GFxMovie::SetVarType::kNormal);
					}
				}
			}
		}

		return _ProcessMessage(self, a_message);
	}

	static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kInputLoaded:
		DialogueHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
