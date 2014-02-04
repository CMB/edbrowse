/* This file is machine-generated, do not hand edit. */

/* sourcefile=html.cpp */
void javaSetsTimeout(int n, const char *jsrc, JS::HandleObject to,
		     eb_bool isInterval);
void javaSetsTagVar(JS::HandleObject v, const char *val);
void javaSubmitsForm(JS::HandleObject v, eb_bool reset);
void javaOpensWindow(const char *href, const char *name);

/* sourcefile=jsdom.cpp */
eb_bool isJSAlive(void);
JSString *our_JS_NewStringCopyN(JSContext * cx, const char *s, size_t n);
JSString *our_JS_NewStringCopyZ(JSContext * cx, const char *s);
char *our_JSEncodeString(JSString * str);
struct ebWindowJSState *createJavaContext(void);
void freeJavaContext(struct ebWindowJSState *state);
void establish_innerHTML(JS::HandleObject jv, const char *start,
			 const char *end, eb_bool is_ta);
void jMyContext(void);
eb_bool javaParseExecute(JS::HandleObject obj, const char *str,
			 const char *filename, int lineno);
eb_bool javaParseExecuteGlobal(const char *str, const char *filename,
			       int lineno);
JSObject *domLink(const char *classname, const char *symname,
		  const char *idname, const char *href, const char *href_url,
		  const char *list, JS::HandleObject owner, int radiosel);

/* sourcefile=jsloc.cpp */
const char *stringize(jsval v);
void initLocationClass(void);
void establish_property_string(JS::HandleObject jv, const char *name,
			       const char *value, eb_bool readonly);
void establish_property_number(JS::HandleObject jv, const char *name, int value,
			       eb_bool readonly);
void establish_property_bool(JS::HandleObject jv, const char *name,
			     eb_bool value, eb_bool readonly);
JSObject *establish_property_array(JS::HandleObject jv, const char *name);
void establish_property_object(JS::HandleObject parent, const char *name,
			       JS::HandleObject child);
void establish_property_url(JS::HandleObject jv, const char *name,
			    const char *url, eb_bool readonly);
void set_property_string(JSObject * jv, const char *name, const char *value);
void set_global_property_string(const char *name, const char *value);
void set_property_number(JS::HandleObject jv, const char *name, int value);
void set_property_bool(JS::HandleObject jv, const char *name, int value);
char *get_property_url(JS::HandleObject jv, eb_bool doaction);
char *get_property_string(JS::HandleObject jv, const char *name);
eb_bool get_property_bool(JS::HandleObject jv, const char *name);
char *get_property_option(JS::HandleObject jv);
JSObject *establish_js_option(JS::HandleObject ev, int idx);
eb_bool handlerGo(JS::HandleObject obj, const char *name);
void handlerSet(JS::HandleObject ev, const char *name, const char *code);
void link_onunload_onclick(JS::HandleObject jv);
eb_bool handlerPresent(JS::HandleObject ev, const char *name);
