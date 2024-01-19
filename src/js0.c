/*********************************************************************
This is the simplest javascript program you can write.
To test the basic functionality of the js engine.
*********************************************************************/

#include "quickjs-libc.h"
#include <stdio.h>
#include <string.h>

int main()
{
JSRuntime *rt;
JSContext *cx; // master window context
JSValue mwo; // master window object
JSValue r;
const char *s;
int l;
char line[200];

rt = JS_NewRuntime();
cx = JS_NewContext(rt);
mwo = JS_GetGlobalObject(cx);

while(fgets(line, sizeof(line), stdin)) {
l = strlen(line);
line[--l] = 0; // chomp
if(!strcmp(line, "q")) break;
r = JS_Eval(cx, line, l, "snork", JS_EVAL_TYPE_GLOBAL);
// no checks for errors or exceptions or anything
s = JS_ToCString(cx, r);
puts(s);
JS_FreeCString(cx, s);
JS_FreeValue(cx, r);
}

// run this cleanup code, that verifies we freed all the objects and strings.
JS_FreeValue(cx, mwo);
JS_FreeContext(cx);
JS_FreeRuntime(rt);
return 0;
}
