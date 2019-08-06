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


void _log_begin(const char *filename, int line)
{
        log_writef("In %s line %d: ", filename, line);
}

void log_end(void)
{
        log_writef("\n");
}


void _log_postfv(const char *filename, int line, const char *fmt, va_list ap)
{
        _log_begin(filename, line);
        log_writefv(fmt, ap);
        log_end();
}

void _log_postf(const char *filename, int line, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        _log_postfv(filename, line, fmt, ap);
        va_end(ap);
}


NORETURN void _fatalfv(const char *filename, int line, const char *fmt, va_list ap)
{
        _log_begin(filename, line);
        log_writefv(fmt, ap);
        log_end();
        exit(1);
}

NORETURN void _fatalf(const char *filename, int line, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        _fatalfv(filename, line, fmt, ap);
        //va_end(ap);
}

NORETURN void _fatal(const char *filename, int line, const char *text)
{
        _fatalf(filename, line, "%s", text);
}