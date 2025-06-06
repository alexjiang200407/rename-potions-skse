#pragma once

#define WIN32_LEAN_AND_MEAN

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;

using namespace std::literals;

#ifdef SKYRIM_AE
#	define OFFSET(se, ae)       ae
#	define OFFSET_3(se, ae, vr) ae
#elif SKYRIMVR
#	define OFFSET(se, ae)       se
#	define OFFSET_3(se, ae, vr) vr
#else
#	define OFFSET(se, ae)       se
#	define OFFSET_3(se, ae, vr) se
#endif

namespace stl
{
	using namespace SKSE::stl;

	template <class T, size_t SZ = 5>
	void write_thunk_call(std::uintptr_t a_src)
	{
		SKSE::AllocTrampoline(14);

		auto& trampoline = SKSE::GetTrampoline();
		T::func          = trampoline.write_call<SZ>(a_src, T::thunk);
	}

	template <class F, std::size_t idx, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[0] };
		logger::info("Written hook to 0x{:x}", F::VTABLE[0].address() + idx * sizeof(void*));
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}
}

#include "Version.h"

#define SPDLOG_LOG_ONCE(level, ...)                                                        \
	do {                                                                                   \
		static std::once_flag _spdlog_once_flag_##__LINE__;                                \
		std::call_once(_spdlog_once_flag_##__LINE__, [&] { spdlog::level(__VA_ARGS__); }); \
	} while (0)
