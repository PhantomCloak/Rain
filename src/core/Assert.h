#pragma once

#define RN_DEBUG

#ifdef RN_DEBUG
	#if defined(HZ_PLATFORM_WINDOWS)
		#define RN_DEBUGBREAK() __debugbreak()
	#elif defined(HZ_PLATFORM_LINUX)
		#include <signal.h>
		#define RN_DEBUGBREAK() raise(SIGTRAP)
	#elif defined(__APPLE__) // Adding this line for macOS
		#define RN_DEBUGBREAK() __builtin_trap()
	#elif defined(__EMSCRIPTEN__)
		#include <emscripten.h>
		#define RN_DEBUGBREAK() EM_ASM({ debugger; });
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define RN_ENABLE_ASSERTS
#else
	#define RN_DEBUGBREAK()
#endif

#define RN_EXPAND_MACRO(x) x

#ifdef RN_ENABLE_ASSERTS
	// Alteratively we could use the same "default" message for both "WITH_MSG" and "NO_MSG" and
	// provide support for custom formatting by concatenating the formatting string instead of having the format inside the default message
	#define RN_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { RN##type##ERROR(msg, __VA_ARGS__); RN_DEBUGBREAK(); } }
	#define RN_INTERNAL_ASSERT_WITH_MSG(type, check, ...) RN_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
	#define RN_INTERNAL_ASSERT_NO_MSG(type, check) RN_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", RN_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

	#define RN_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
	#define RN_INTERNAL_ASSERT_GET_MACRO(...) RN_EXPAND_MACRO( RN_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, RN_INTERNAL_ASSERT_WITH_MSG, RN_INTERNAL_ASSERT_NO_MSG) )

	// Currently accepts at least the condition and one additional parameter (the message) being optional
	#define RN_ASSERT(...) RN_EXPAND_MACRO( RN_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
	#define RN_CORE_ASSERT(...) RN_EXPAND_MACRO( RN_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
//#else
//	#define RN_ASSERT(...)
//	#define RN_CORE_ASSERT(...)
#endif
