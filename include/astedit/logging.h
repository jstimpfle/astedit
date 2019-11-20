#ifndef ASTEDIT_LOGGING_H_INCLUDED
#define ASTEDIT_LOGGING_H_INCLUDED

/* The meta information for log messages is likely to always be filename +
 * line number. But since any other module should be able to hook into the
 * logging system we want to avoid too tight of a coupling. These modules
 * should make their own macros that use MAKE_LOGINFO() to forward to the
 * logging module. */
struct LogInfo {
        const char *filename;
        int line;
};

void log_writefv(const char *fmt, va_list ap);
void log_writef(const char *fmt, ...);
void log_write(const char *text, int length);
void log_write_cstring(const char *text);

void _log_begin(struct LogInfo logInfo);
void log_end(void);

void _log_postfv(struct LogInfo logInfo, const char *fmt, va_list ap);
void _log_postf(struct LogInfo logInfo, const char *fmt, ...);

NORETURN void _fatalfv(struct LogInfo logInfo, const char *fmt, va_list ap);
NORETURN void _fatalf(struct LogInfo logInfo, const char *fmt, ...);
NORETURN void _fatal(struct LogInfo logInfo, const char *text);

#define MAKE_LOGINFO() ((struct LogInfo) { __FILE__, __LINE__ } )
#define log_begin() _log_begin(MAKE_LOGINFO())
#define log_postfv(fmt, ap) _log_postfv(MAKE_LOGINFO(), (fmt), (ap))
#define log_postf(fmt, ...) _log_postf(MAKE_LOGINFO(), (fmt), ##__VA_ARGS__)
#define fatalfv(fmt, ap), _fatalfv(MAKE_LOGINFO(), (fmt), (ap))
#define fatalf(fmt, ...) _fatalf(MAKE_LOGINFO(), (fmt), ##__VA_ARGS__)
#define fatal(text) _fatal(MAKE_LOGINFO(), (text))

#endif
