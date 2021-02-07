/*\
 * strcasestr.h
 *
 * Copyright (c) 2021 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
 * from: https://stackoverflow.com/questions/27303062/strstr-function-like-that-ignores-upper-or-lower-case
 * Windows implementation of 'stristr', as a substitute for unix 'strcasestr' - see 'eb.h', under DOSLIKE macro
 *
\*/

#ifndef _STRCASESTR_H_
#define _STRCASESTR_H_

extern char* stristr( const char* str1, const char* str2 );

#endif // #ifndef _STRCASESTR_H_
// eof - strcasestr.h
