#ifndef _PORTABLE_SNPRINTF_H_
#define _PORTABLE_SNPRINTF_H_

#define PORTABLE_SNPRINTF_VERSION_MAJOR 2
#define PORTABLE_SNPRINTF_VERSION_MINOR 2

//android libc is defective(printfs drop non ascii chars)
//this causes strings to cutoff at dos escape codes
#define snprintf rabidcow_protectagainstusage1
#define vsnprintf rabidcow_protectagainstusage2
#define asprintf rabidcow_protectagainstusage3
#define vasprintf rabidcow_protectagainstusage4
#define asnprintf rabidcow_protectagainstusage5
#define vasnprintf rabidcow_protectagainstusage6
#include_next <stdio.h>
#undef snprintf
#undef vsnprintf
#undef asprintf
#undef vasprintf
#undef asnprintf
#undef vasnprintf

extern int snprintf(char *, size_t, const char *, /*args*/ ...);
extern int vsnprintf(char *, size_t, const char *, va_list);

extern int asprintf  (char **ptr, const char *fmt, /*args*/ ...);
extern int vasprintf (char **ptr, const char *fmt, va_list ap);
extern int asnprintf (char **ptr, size_t str_m, const char *fmt, /*args*/ ...);
extern int vasnprintf(char **ptr, size_t str_m, const char *fmt, va_list ap);

#endif
