#pragma once
#include "Log.h"

#define LOG_INFO(msg)    Log(LogLevel::Info, msg)
#define LOG_WARN(msg)    Log(LogLevel::Warning, msg)
#define LOG_ERROR(msg)   Log(LogLevel::Error, msg)
#define LOG_FATAL(msg)   Fatal(msg, __FILE__, __LINE__)