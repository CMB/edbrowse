/* Stand-alone test program for duktape
 * This is just a hello world program.
 * Needs some work, WRT the global object, but this is a good starting point. */

#include <duktape.h>

int run(duk_context * cx, const char *script)
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
		printf("Non-string result after successful eval.\n");
	}
	duk_pop(cx);

	return rc;
}

int main(int argc, const char *argv[])
{
	duk_context *cx = duk_create_heap_default();

	if (!cx) {
		printf("Failed to create duktape context, cannot proceed.\n");
		return 1;
	}

	duk_idx_t base = duk_get_top(cx);
/* the base is 0, we haven't done anything yet.
	printf("base %d\n", base);
*/

	int status = run(cx, "'hello world, the answer is ' + 6*7;");
	/* If you want to see the error reporter in action:
	   status = run(cx, "foo();");
	 */

/* make window object as global */
	duk_push_global_object(cx);
	duk_push_string(cx, "window");
	duk_push_global_object(cx);
	duk_def_prop(cx, base,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE));

/* now create a variable snork and see if we can access it via window.snork */
/* this is a regular variable, writable, deletable. */
	duk_push_string(cx, "snork");
	duk_push_int(cx, 27);
	duk_def_prop(cx, base,
		     (DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_ENUMERABLE |
		      DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE));
/* now see if it's there under window */
/* Also make sure the standard classes are there by taking square root. */
	status = run(cx, "'sqrt(window.snork) = ' + Math.sqrt(window.snork)");
/* and pop the global object, which is still sitting there on stack */
	duk_pop(cx);

	duk_destroy_heap(cx);

	return status;
}
