#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <stdio.h>
#include <stdlib.h>

void log_writefv(const char *fmt, va_list ap)
{
        vfprintf(stderr, fmt, ap);
}

void log_writef(const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        log_writefv(fmt, ap);
        va_end(ap);
}

void log_write(const char *text, int length)
{
        fwrite(text, length, 1, stderr);
}

void log_write_cstring(const char *text)
{
        fprintf(stderr, "%s", text);
}


void _log_begin(struct LogInfo logInfo)
{
        log_writef("In %s:%d: ", logInfo.filename, logInfo.line);
}

void log_end(void)
{
        log_writef("\n");
}


void _log_postfv(struct LogInfo logInfo, const char *fmt, va_list ap)
{
        _log_begin(logInfo);
        log_writefv(fmt, ap);
        log_end();
}

void _log_postf(struct LogInfo logInfo, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        _log_postfv(logInfo, fmt, ap);
        va_end(ap);
}


NORETURN void _fatalfv(struct LogInfo logInfo, const char *fmt, va_list ap)
{
        _log_begin(logInfo);
        log_writefv(fmt, ap);
        log_end();
        exit(1);
}

NORETURN void _fatalf(struct LogInfo logInfo, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        _fatalfv(logInfo, fmt, ap);
        //va_end(ap);
}

NORETURN void _fatal(struct LogInfo logInfo, const char *text)
{
        _fatalf(logInfo, "%s", text);
}
