/* Stand-alone test program for the spider monkey js interface,
 * version 2.4 or above.
 * This is just a hello world program. */

#define UINT32_MAX 4294967295
#include "jsapi.h"

/* The class of the global object. */
static JSClass global_class = { "global",
	JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub,
	JS_DeletePropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	nullptr,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

/* The error reporter callback. */
void reportError(JSContext * cx, const char *message, JSErrorReport * report)
{
	fprintf(stderr, "%s:%u:%s\n",
		report->filename ? report->filename : "[no filename]",
		(unsigned int)report->lineno, message);
}

static const char runCommand[] = "'hello world, the answer is ' + 6*7;";

int run(JSContext * cx)
{
	/* Enter a request before running anything in the context */
	JSAutoRequest ar(cx);

	/* Create the global object in a new compartment. */
	JSObject *global = JS_NewGlobalObject(cx, &global_class, nullptr);
	if (!global)
		return 3;

	/* Set the context's global */
	JSAutoCompartment ac(cx, global);
	JS_SetGlobalObject(cx, global);

	/* Populate the global object with the standard globals, like Object and Array. */
	if (!JS_InitStandardClasses(cx, global))
		 return 4;

	/* Your application code here. This may include JSAPI calls to create your own custom JS objects and run scripts. */
	js::RootedValue rval(cx);
	int rc = JS_EvaluateScript(cx, global, runCommand, strlen(runCommand),
				   "TestCommand", 0, rval.address());
	if (!rc)
		return 5;

	if (!JSVAL_IS_STRING(rval))
		return 0;

/* Result of the command is a string, print it out. */
	js::RootedString str(cx, JSVAL_TO_STRING(rval));
	size_t encodedLength = JS_GetStringEncodingLength(cx, str);
	rc = 0;
	char *buffer = (char *)malloc(encodedLength + 1);
	buffer[encodedLength] = '\0';
	size_t result = JS_EncodeStringToBuffer(cx, str, buffer, encodedLength);
	if (result == (size_t) - 1)
		rc = 6;
	else
		puts(buffer);
	free(buffer);

	return rc;
}

int main(int argc, const char *argv[])
{
	/* Create a JS runtime. */
	JSRuntime *rt = JS_NewRuntime(8L * 1024L * 1024L, JS_NO_HELPER_THREADS);
	if (!rt)
		return 1;

	/* Create a context. */
	JSContext *cx = JS_NewContext(rt, 8192);
	if (!cx)
		return 2;
	JS_SetOptions(cx, JSOPTION_VAROBJFIX);
	JS_SetErrorReporter(cx, reportError);

	int status = run(cx);

	JS_DestroyContext(cx);
	JS_DestroyRuntime(rt);

	/* Shut down the JS engine. */
	JS_ShutDown();

	return status;
}
