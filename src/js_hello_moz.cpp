// This program originally came from the Mozilla site, for moz 52.
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/How_to_embed_the_JavaScript_engine
// I tweaked it for moz 60.
// Then I added so much stuff to it you'd hardly recognize it.
// It has become my sandbox.

#include <jsapi.h>
#include <js/Initialization.h>

static JSClassOps global_ops = {
// different members in different versions, so specify explicitly
    trace : JS_GlobalObjectTraceHook
};

/* The class of the global object. */
static JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    &global_ops
};

#include "eb.h"

// A few functions from the edbrowse world, so I can write and test
// some other functions that rely on those functions.

static struct ebWindow win0;
struct ebWindow *cw = &win0;
Frame *cf = &win0.f0;
int context = 0;
bool allowJS = true;
bool showHover, doColors;
char *dbarea, *dblogin, *dbpw;	/* to log into the database */
bool fetchBlobColumns;
void setDataSource(char *v){}
bool binaryDetect = true;
bool sendReferrer, allowRedirection, curlAuthNegotiate, ftpActive;
bool htmlGenerated;
uchar browseLocal;
char *changeFileName;
char *sslCerts;
char *configFile, *addressFile, *cookieFile;
char *recycleBin, *sigFile, *sigFileEnd;
char *ebUserDir;
char *cacheDir;
int cacheSize, cacheCount;
bool inInput, listNA;
pst linePending;
int fileSize;
int displayLength = 500;
char *newlocation;
Frame *newloc_f;
bool newloc_r;
int newloc_d;
bool runEbFunction(const char *line){ return true; }
bool inputReadLine;
bool caseInsensitive, searchStringsAll, searchWrap = true;
const char *jsSourceFile;
int jsLineno;
int gfsn;
struct MACCOUNT accounts[MAXACCOUNT];
int localAccount, maxAccount;
struct MIMETYPE mimetypes[MAXMIME];
int maxMime;
const char *version = "3.7.7";
volatile bool intFlag;
time_t intStart;
bool ismc, isimap, passMail;
char *mailDir, *mailUnread, *mailStash, *mailReply;
struct ebSession sessionList[10], *cs;
int maxSession;
int webTimeout = 30, mailTimeout = 30;
int verifyCertificates = 1;
char *userAgents[MAXAGENT + 1];
char *currentAgent;
void ebClose(int n) { exit(n); }
bool javaOK(const char *url) { return true; }
bool mustVerifyHost(const char *url) { return false; }
const char *findAgentForURL(const char *url) { return 0; }
const char eol[] = "\r\n";
const char *findProxyForURL(const char *url) { return 0; }
const char *mailRedirect(const char *to, const char *from, 			 const char *reply, const char *subj) { return 0; }
void readConfigFile(void) { }
const char *fetchReplace(const char *u){return 0;}

extern JSContext *cxa; // the context for all
extern JS::RootedObject *rw0;
extern JS::RootedObject *mw0;

static void ReportJSException(void)
{
if(JS_IsExceptionPending(cxa)) {
JS::RootedValue exception(cxa);
if(JS_GetPendingException(cxa,&exception) &&
exception.isObject()) {
// I don't think we need this line.
// JS::AutoSaveExceptionState savedExc(cxa);
JS::RootedObject exceptionObject(cxa,
&exception.toObject());
JSErrorReport *what =
JS_ErrorFromException(cxa,exceptionObject);
if(what) {
if(!stringEqual(what->filename, "noname"))
printf("%s line %d: ", what->filename, what->lineno);
puts(what->message().c_str());
// what->filename what->lineno
}
}
JS_ClearPendingException(cxa);
}
}

extern const char *stringize(JS::HandleValue v);
extern JSObject *frameToCompartment(const Frame *f);

static void unlinkJSContext(int sn)
{
char buf[16];
sprintf(buf, "g%d", sn);
debugPrint(3, "remove js context %d", sn);
// I think we're already in a compartment, but just to be safe...
        JSAutoCompartment ac(cxa, *rw0);
JS_DeleteProperty(cxa, *rw0, buf);
}

// This assumes you are in the compartment where you want to exec the file
static void execScript(const char *script)
{
        JS::CompileOptions opts(cxa);
        opts.setFileAndLine("noname", 0);
JS::RootedValue v(cxa);
        bool ok = JS::Evaluate(cxa, opts, script, strlen(script), &v);
if(!ok)
ReportJSException();
else
puts(stringize(v));
}

int main(int argc, const char *argv[])
{
bool iaflag = false; // interactive
int c; // compartment
int top; // number of windows
char buf[16];

// It's a test program, let's see the stuff.
debugLevel = 5;
selectLanguage();

static char myhome[] = "/snork";
home = myhome;

if(argc > 1 && !strcmp(argv[1], "-i")) iaflag = true;
top = iaflag ? 3 : 1;

for(c=0; c<top; ++c) {
sprintf(buf, "session %d", c+1);
cf->fileName = buf;
cf->gsn = c+1;
cf->owner = cw;
cf->jslink = false;
createJSContext(cf);
if(!cf->jslink) {
printf("create failed on %d\n", c+1); 
return 3;
}
}

c = 0; // back to the first window
//  puts("after loop");

{
static char tempname[] = "session 1";
cf->gsn = 1;
cf->fileName = tempname;
        JSAutoCompartment ac(cxa, frameToCompartment(cf));
execScript("'hello world, it is '+new Date()");
}

if(iaflag) {
// end with q to quit
char *line;
while(line = (char*)inputLine()) {
perl2c(line);
if(stringEqual(line, "q") || stringEqual(line, "qt"))
break;

// show context
if(stringEqual(line, "e")) {
puts(cf->fileName);
continue;
}

// change context?
if(line[0] == 'e' &&
line[1] >= '1' && line[1] <= '3' &&
!line[2]) {
static char tempname[16];
sprintf(tempname, "session %c", line[1]);
puts(tempname);
cf->fileName = tempname;
c = line[1] - '1';
cf->gsn = c+1;
continue;
}

	{
        JSAutoCompartment ac(cxa, frameToCompartment(cf));
JS::RootedValue v(cxa);
if(line[0] == '<') {
char *data;
int datalen;
if(fileIntoMemory(line+1, &data, &datalen)) {
jsRunScriptWin(data, line+1, 1);
nzFree(data);
puts("ok");
} else {
printf("cannot open %s\n", line+1);
}
} else {
char *res = jsRunScriptWinResult(line, "noname", 0);
if(res) puts(res);
nzFree(res);
}
}
}
}

// I should be able to remove globals in any order, need not be a stack
for(c=0; c<top; ++c)
unlinkJSContext(c+1);

// rooted objects have to free in the reverse (stack) order.
delete mw0;
delete rw0;

puts("destroy");
JS_DestroyContext(cxa);
    JS_ShutDown();
    return 0;
}
