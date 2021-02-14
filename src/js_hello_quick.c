#include <stdio.h>
#include <string.h>
#include "quickjs-libc.h"
#include "quickjs.h"

// gcc js_hello_quick.c -o js_hello_quick quickjs.c libregexp.c libunicode.c cutils.c -lm -ldl -lpthread

int main(int argc, char **argv)
{

JSRuntime *rt;
JSContext *ctx;
int flags;
rt = JS_NewRuntime();
ctx = JS_NewContext(rt);
JSValue val;
const char *filename;

char *x = "'hello world, the answer is ' + 6*7;";

val = JS_Eval(ctx, x, strlen(x), filename, JS_EVAL_TYPE_GLOBAL);
printf("%s\n",JS_AtomToCString(ctx,JS_ValueToAtom(ctx,val)));
}


