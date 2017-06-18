/* Stand-alone test program for duktape
 * This is just a hello world program.
 * Needs some work, WRT the global object, but this is a good starting point. */

#include <duktape.h>

static const char runCommand[] = "'hello world, the answer is ' + 6*7;";

int run(duk_context *cx, const char *script)
{
	/* Your application code here. This may include JSAPI calls to create your own custom JS objects and run scripts. */
	int rc = duk_peval_string(cx, script);
	/* If duk_peval fails, the error string is at top of stack;
	 * if it succeeds, the result is at top.
	 */
	if (rc) {
		/* Use duk_safe_to_string() to convert error into string.  This API
		 * call is guaranteed not to throw an error during the coercion.
		 */
		printf("Script error: %s\n", duk_safe_to_string(cx, -1));
	} else if (duk_is_string(cx, -1)) {
		printf("%s\n", duk_safe_to_string(cx, -1));
	} else {
		printf("Got non-string result after successful eval, not printing it.\n");
	}
	duk_pop(cx);

	return rc;
}

int main(int argc, const char *argv[])
{
	/* Create a JS runtime. */
	duk_context *cx = duk_create_heap_default();

	if (!cx) {
		printf("Failed to create duktape context, cannot proceed.\n");
		return 1;
	}

	int status = run(cx, runCommand);
	/* If you want to see the error reporter in action:
	status = run(cx, "foo();");
	 */

	duk_destroy_heap(cx);

	return status;
}
