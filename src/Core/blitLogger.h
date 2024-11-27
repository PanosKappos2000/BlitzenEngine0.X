#pragma once 
#include <iostream>

namespace BlitzenCore
{
    #define BLITZEN_LOG_FATAL               1
    #define BLITZEN_LOG_ERROR               1

    #ifndef NDEBUG
        #define BLITZEN_LOG_INFO            1
        #define BLITZEN_LOG_WARNINGS        1
        #define BLITZEN_LOG_DEBUG           1
        #define BLITZEN_LOG_TRACE           1
    #else
        #define BLITZEN_LOG_INFO            0
        #define BLITZEN_LOG_WARNINGS        0
        #define BLITZEN_LOG_DEBUG           0
        #define BLITZEN_LOG_TRACE           0
    #endif

    enum class LogLevel : uint8_t
    {
        Fatal = 0,
        Error = 1, 
        Info = 2, 
        Warn = 3, 
        Debug = 4, 
        Trace = 5,

        MaxLevel = 6
    };

    void BlitLog(LogLevel level, const char* message, ...);

    #if BLITZEN_LOG_FATAL
        #define BLIT_FATAL(message, ...)     Log(BlitzenCore::LogLevel::Fatal, message, ##__VA_ARGS__);
    #else
        #define BLIT_FATAL(message, ...)    ;
    #endif

    #if BLITZEN_LOG_ERROR
        #define BLIT_ERROR(message, ...)     Log(BlitzenCore::LogLevel::Error, message, ##__VA_ARGS__);
    #else
        #define BLIT_ERROR(message, ...)    ;
    #endif

    #if BLITZEN_LOG_INFO
        #define BLIT_INFO(message, ...)      Log(BlitzenCore::LogLevel::Info, message, ##__VA_ARGS__);
    #else
        #define BLIT_INFO(message, ...)      ;
    #endif

    #if BLITZEN_LOG_WARNINGS
        #define BLIT_WARN(message, ...)     Log(BlitzenCore::LogLevel::Warn, message, ##__VA_ARGS__);
    #else
        #define BLIT_WARN(message, ...)     ;
    #endif

    #if BLITZEN_LOG_DEBUG
        #define BLIT_DBLOG(message, ...)     Log(BlitzenCore::LogLevel::Debug, message, __VA_ARGS__);
    #else
        #define BLIT_DBLOG(message, ...)    ;
    #endif

    #if BLITZEN_LOG_TRACE
        #define BLIT_TRACE(message, ...)     Log(BlitzenCore::LogLevel::Trace, message, __VA_ARGS__);
    #else
        #define BLIT_TRACE(message, ...)    ;
    #endif
}