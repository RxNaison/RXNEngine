#pragma once

#include "Base.h"
#include "Log.h"
#include <filesystem>

#ifdef RXN_ENABLE_ASSERTS

#define RXN_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { RXN##type##ERROR(msg, __VA_ARGS__); RXN_DEBUGBREAK(); } }
#define RXN_INTERNAL_ASSERT_WITH_MSG(type, check, ...) RXN_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
#define RXN_INTERNAL_ASSERT_NO_MSG(type, check) RXN_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", RXN_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

#define RXN_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define RXN_INTERNAL_ASSERT_GET_MACRO(...) RXN_EXPAND_MACRO( RXN_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, RXN_INTERNAL_ASSERT_WITH_MSG, RXN_INTERNAL_ASSERT_NO_MSG) )

#define RXN_ASSERT(...) RXN_EXPAND_MACRO( RXN_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
#define RXN_CORE_ASSERT(...) RXN_EXPAND_MACRO( RXN_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
#define RXN_ASSERT(...)
#define RXN_CORE_ASSERT(...)
#endif