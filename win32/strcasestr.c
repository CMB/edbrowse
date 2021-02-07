/*\
 * strcasestr.c
 *
 * Copyright (c) 2021 - Geoff R. McLane
 * Licence: GNU GPL version 2
 *
 * from: https://stackoverflow.com/questions/27303062/strstr-function-like-that-ignores-upper-or-lower-case
 * Windows implementation of 'stristr', as a substitute for unix 'strcasestr' - see 'eb.h', under DOSLIKE macro
 *
\*/

#include <stdio.h>
#include <ctype.h>
// other includes
#include "strcasestr.h"

static const char *module = "strcasestr";

// implementation
char* stristr( const char* str1, const char* str2 )
{
    const char* p1 = str1 ;
    const char* p2 = str2 ;
    const char* r = *p2 == 0 ? str1 : 0 ;

    while( *p1 != 0 && *p2 != 0 )
    {
        if( tolower( (unsigned char)*p1 ) == tolower( (unsigned char)*p2 ) )
        {
            if( r == 0 )
            {
                r = p1 ;
            }

            p2++ ;
        }
        else
        {
            p2 = str2 ;
            if( r != 0 )
            {
                p1 = r + 1 ;
            }

            if( tolower( (unsigned char)*p1 ) == tolower( (unsigned char)*p2 ) )
            {
                r = p1 ;
                p2++ ;
            }
            else
            {
                r = 0 ;
            }
        }

        p1++ ;
    }

    return *p2 == 0 ? (char*)r : 0 ;
}

// eof = strcasestr.c
