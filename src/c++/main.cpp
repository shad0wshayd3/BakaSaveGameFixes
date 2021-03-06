namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path)
		{
			stl::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format(FMT_STRING("{}.log"), Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level =
			*Settings::EnableDebugLogging ? spdlog::level::trace : spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%m/%d/%Y - %T] [%^%l%$] %v"s);
	}
}

namespace Patch
{
	void AddNoteVarArgs([[maybe_unused]] void* a_this, const char* a_message, va_list a_list)
	{
		std::array<char, 2048> buffer{ '\0' };
		vsnprintf_s(buffer.data(), buffer.size(), 2048, a_message, a_list);

		std::string formatted{ buffer.data(), buffer.size() };
		logger::info(formatted.c_str());
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_F4SE, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	const auto rtv = a_F4SE->RuntimeVersion();
	if (rtv < F4SE::RUNTIME_LATEST)
	{
		stl::report_and_fail(
			fmt::format(FMT_STRING("{} does not support runtime v{}."sv), Plugin::NAME, rtv.string()));
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_F4SE)
{
	Settings::Load();

	InitializeLog();
	logger::info(FMT_STRING("{} v{} log opened."sv), Plugin::NAME, Plugin::VERSION.string());
	logger::debug("Debug logging enabled."sv);

	F4SE::Init(a_F4SE);
	F4SE::AllocTrampoline(1 << 4);

	REL::Relocation<std::uintptr_t> target{ REL::ID(1255812) };
	
	auto& trampoline = F4SE::GetTrampoline();
	trampoline.write_branch<5>(target.address(), Patch::AddNoteVarArgs);

	logger::info("Plugin loaded successfully."sv);

	return true;
}
