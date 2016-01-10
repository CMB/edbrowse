/*\
 * vsprtf.hxx
 *
 * Copyright (c) 2014 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
\*/

#ifndef _VSPRTF_HXX_
#define _VSPRTF_HXX_
#ifdef __cplusplus
extern "C" {
#endif

extern int vasprintf (char **str, const char *fmt, va_list args);
extern int asprintf (char **str, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
#endif // #ifndef _VSPRTF_HXX_
// eof - vsprtf.hxx
