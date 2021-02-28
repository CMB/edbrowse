#include <string.h>

#include <quickjs/quickjs-libc.h>

#define TOP 3

#define stringEqual !strcmp

int main(int argc, char **argv)
{
int c;
JSRuntime *rt;
JSContext *cx[3];
JSValue val;
const char *filename = "interactive";
const char *first = "'hello world, the answer is ' + 6*7;";
char line[80];

rt = JS_NewRuntime();
for(c=0; c<TOP; ++c)
cx[c] = JS_NewContext(rt);

c = 0;
val = JS_Eval(cx[c], first, strlen(first), filename, JS_EVAL_TYPE_GLOBAL);
printf("%s\n",JS_AtomToCString(cx[c],JS_ValueToAtom(cx[c],val)));

while(fgets(line, sizeof(line), stdin)) {
int l = strlen(line);
// chomp
if(l && line[l-1] == '\n') line[--l] = 0;
if(!l) // empty line
continue;
if(stringEqual(line, "q") || stringEqual(line, "qt"))
break;

// show context
if(stringEqual(line, "e")) {
printf("session %d\n", c+1);
continue;
}

// change context
if(line[0] == 'e' &&
line[1] >= '1' && line[1] <= '0'+TOP &&
!line[2]) {
static char tempname[16];
sprintf(tempname, "session %c", line[1]);
puts(tempname);
c = line[1] - '1';
continue;
}

if(line[0] == '<') {
  puts("execute file not yet implemented");
} else {
val = JS_Eval(cx[c], line, strlen(line), filename, JS_EVAL_TYPE_GLOBAL);
printf("%s\n",JS_AtomToCString(cx[c],JS_ValueToAtom(cx[c],val)));
}
}

// clean up
for(c=0; c<TOP; ++c)
JS_FreeContext(cx[c]);

JS_FreeRuntime(rt);
}
