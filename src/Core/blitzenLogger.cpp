#include "blitLogger.h"
#include "blitAssert.h"

// Need this for string formatting
#include <stdarg.h>

namespace BlitzenCore
{
    void BlitLog(LogLevel level, const char* message, ...)
    {
        const char* logLevels[static_cast<size_t>(LogLevel::MaxLevel)] = {"{FATAL}: ", "{ERROR}: ", "{Info}: ", "{Warning}: ", "{Debug}: ", "{Trace}: "};
        uint8_t isError = level < LogLevel::Warn;

        //Temporary to avoid dynamic arrays for now
        char outMessage[3200];
        memset(outMessage, 0, sizeof(outMessage));

        // This variable is different depending on the platform
        #if _MSC_VER
            va_list argPtr;
        #else
            __builtin_va_list argPtr;
        #endif

        va_start(argPtr, message);
        vsnprintf(outMessage, 3200, message, argPtr);
        va_end(argPtr);

        // Pass out message to a clean buffer to be printed out
        char outMessage2[3200];
        sprintf(outMessage2, "%s%s\n", logLevels[static_cast<uint8_t>(level)], outMessage);
    }

    void ReportAssertionFailure(const char* expression, const char* message, const char* file, int32_t line)
    {
        BlitLog(LogLevel::Fatal, "Assertion failure: %s, message: %s, in file: %s, line: %d", expression, message, file, line);
    }
}