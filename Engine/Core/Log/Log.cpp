#include "Log.h"


void Log(LogLevel level, const std::string& message)
{
    const char* prefix = "";

    switch (level)
    {
    case LogLevel::Info:    prefix = "[Info] "; break;
    case LogLevel::Warning: prefix = "[Warn] "; break;
    case LogLevel::Error:   prefix = "[Error] "; break;
    case LogLevel::Fatal:   prefix = "[Fatal] "; break;
    }

    std::string output = prefix + message + "\n";

    OutputDebugStringA(output.c_str()); // Visual Studio出力
}


void Fatal(const std::string& message, const char* file, int line)
{
    std::string msg = "[FATAL] ";
    msg += file;
    msg += ":";
    msg += std::to_string(line);
    msg += " ";
    msg += message;
    msg += "\n";

    OutputDebugStringA(msg.c_str());

    // ブレーク（デバッガ停止）
    __debugbreak();

    // 念のためクラッシュ
    std::abort();
}