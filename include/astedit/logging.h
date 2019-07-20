#ifndef ASTEDIT_LOGGING_H_INCLUDED
#define ASTEDIT_LOGGING_H_INCLUDED

void log_writefv(const char *fmt, va_list ap);
void log_writef(const char *fmt, ...);
void log_write(const char *text, int length);
void log_write_cstring(const char *text);

void _log_begin(const char *filename, int line);
void log_end(void);

void _log_postfv(const char *filename, int line, const char *fmt, va_list ap);
void _log_postf(const char *filename, int line, const char *fmt, ...);

NORETURN void _fatalfv(const char *filename, int line, const char *fmt, va_list ap);
NORETURN void _fatalf(const char *filename, int line, const char *fmt, ...);
NORETURN void _fatal(const char *filename, int line, const char *text);

#define log_begin() _log_begin(__FILE__, __LINE__)
#define log_postfv(fmt, ap) _log_postfv(__FILE__, __LINE__, (fmt), (ap))
#define log_postf(fmt, ...) _log_postf(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define fatalfv(fmt, ap), _fatalfv(__FILE__, __LINE__, (fmt), (ap))
#define fatalf(fmt, ...) _fatalf(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define fatal(text) _fatal(__FILE__, __LINE__, (text))

#endif
