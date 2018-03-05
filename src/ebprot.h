/* Prototypes for edbrowse */

/* sourcefile=auth.c */
bool getUserPass(const char *url, char *creds, bool find_proxy) ;
bool getUserPassRealm(const char *url, char *creds, const char *realm);
// Add authorization entries only in the foreground, but it's an
// atomic operation, so the previous routines can run from other threads.
bool addWebAuthorization(const char *url, const char *credentials, bool proxy, const char *realm);

/* sourcefile=buffers.c */
void removeHiddenNumbers(pst p, uchar terminate);
pst fetchLine(int n, int show) ;
void displayLine(int n) ;
void initializeReadline(void) ;
pst inputLine(void) ;
bool cxCompare(int cx) ;
bool cxActive(int cx) ;
bool cxQuit(int cx, int action) ;
void cxSwitch(int cx, bool interactive) ;
bool addTextToBuffer(const pst inbuf, int length, int destl, bool showtrail) ;
void delText(int start, int end) ;
bool readFileArgv(const char *filename, bool newbuf);
bool unfoldBufferW(const struct ebWindow *w, bool cr, char **data, int *len) ;
bool unfoldBuffer(int cx, bool cr, char **data, int *len) ;
bool runCommand(const char *line) ;
bool edbrowseCommand(const char *line, bool script) ;
int sideBuffer(int cx, const char *text, int textlen, const char *bufname);
void freeEmptySideBuffer(int n);
bool browseCurrentBuffer(void) ;
bool locateTagInBuffer(int tagno, int *ln_p, char **p_p, char **s_p, char **t_p);
char *getFieldFromBuffer(int tagno);
int fieldIsChecked(int tagno);

/* sourcefile=cookies.c */
bool domainSecurityCheck(const char *server, const char *domain) ;
bool receiveCookie(const char *url, const char *str) ;
void cookiesFromJar(void) ;
void sendCookies(char **s, int *l, const char *url, bool issecure) ;
void mergeCookies(void);

/* sourcefile=cache.c */
void setupEdbrowseCache(void);
void clearCache(void) ;
bool fetchCache(const char * url, const char *etag, time_t modtime, char **data, int *data_len) ;
void storeCache(const char *url, const char *etag, time_t modtime, const char *data, int datalen) ;
bool presentInCache(const char *url) ;

/* sourcefile=dbodbc.c (and others) */
bool fetchForeign(char *tname) ;

/* sourcefile=dbops.c */
int findColByName(const char *name) ;

/* sourcefile=dbstubs.c */
bool sqlReadRows(const char *filename, char **bufptr) ;
void dbClose(void) ;
void showColumns(void) ;
void showForeign(void) ;
bool showTables(void) ;
bool sqlDelRows(int start, int end) ;
bool sqlUpdateRow(pst source, int slen, pst dest, int dlen) ;
bool sqlAddRows(int ln) ;
bool ebConnect(void) ;
int goSelect(int *startLine, char **rbuf) ;

/* sourcefile=ebjs.c */
void javaSetsTagVar(jsobjtype v, const char *newtext);
void javaSetsInner(jsobjtype v, const char *newtext);
void dwStart(void);
void createJavaContext(void) ;
void freeJavaContext(struct ebFrame *f) ;
char *jsRunScriptResult(jsobjtype obj, const char *str, const char *filename, int lineno) ;
void jsRunScript(jsobjtype obj, const char *str, const char *filename, int lineno) ;
enum ej_proptype typeof_property(jsobjtype obj, const char *name) ;
bool has_property(jsobjtype obj, const char *name) ;
#define handlerPresent(obj, name) (typeof_property(obj, name) == EJ_PROP_FUNCTION)
void delete_property(jsobjtype obj, const char *name) ;
char *get_property_string(jsobjtype obj, const char *name) ;
int get_property_number(jsobjtype obj, const char *name) ;
double get_property_float(jsobjtype obj, const char *name) ;
bool get_property_bool(jsobjtype obj, const char *name) ;
jsobjtype get_property_object(jsobjtype parent, const char *name) ;
jsobjtype get_property_function(jsobjtype parent, const char *name);
jsobjtype get_array_element_object(jsobjtype obj, int idx) ;
int set_property_string(jsobjtype obj, const char *name, const char *value) ;
int set_property_number(jsobjtype obj, const char *name, int n) ;
int set_property_float(jsobjtype obj, const char *name, double n) ;
int set_property_bool(jsobjtype obj, const char *name, bool n) ;
int set_property_object(jsobjtype parent, const char *name, jsobjtype child) ;
jsobjtype instantiate_array(jsobjtype parent, const char *name) ;
int set_array_element_object(jsobjtype array, int idx, jsobjtype child) ;
jsobjtype instantiate_array_element(jsobjtype array, int idx, const char *classname) ;
jsobjtype instantiate(jsobjtype parent, const char *name, const char *classname) ;
int set_property_function(jsobjtype parent, const char *name, const char *body) ;
int get_arraylength(jsobjtype a);
char *get_property_option(jsobjtype obj) ;
void setupJavaDom(void) ;
char *get_property_url(jsobjtype owner, bool action) ;
void rebuildSelectors(void);
bool run_function_bool(jsobjtype obj, const char *name);
bool run_event_bool(jsobjtype obj, const char *pname, const char *eevname);
void run_function_onearg(jsobjtype obj, const char *name, jsobjtype o);
void set_basehref(const char *b);

/* sourcefile=fetchmail.c */
int fetchMail(int account) ;
int fetchAllMail(void) ;
void scanMail(void) ;
bool emailTest(void) ;
void mail64Error(int err);
char *emailParse(char *buf) ;
bool setupReply(bool all) ;

/* sourcefile=format.c */
void prepareForBrowse(char *h, int h_len) ;
void prepareForField(char *h);
bool breakLine(const char *line, int len, int *newlen) ;
void breakLineSetup(void) ;
char *htmlReformat(char *buf) ;
void extractEmailAddresses(char *line) ;
void cutDuplicateEmails(char *tolist, char *cclist, const char *reply) ;
int byteOrderMark(const uchar *buf, int buflen);
bool looksBinary(const unsigned char *buf, int buflen);
void looks_8859_utf8(const uchar *buf, int buflen, bool * iso_p, bool * utf8_p);
void iso2utf(const uchar *inbuf, int inbuflen, uchar **outbuf_p, int *outbuflen_p);
void utf2iso(const uchar *inbuf, int inbuflen, uchar **outbuf_p, int *outbuflen_p);
void utfHigh(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p, bool inutf8, bool out32, bool outbig);
char *uni2utf8(unsigned int unichar);
void utfLow(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p, int bom);
char *force_utf8( char *buf, int buflen);
char *base64Encode(const char *inbuf, int inlen, bool lines);
void iuReformat(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p) ;
bool parseDataURI(const char *uri, char **mediatype, char **data, int *data_l);

/* sourcefile=html.c */
bool tagHandler(int seqno, const char *name) ;
void jSideEffects(void) ;
void jSyncup(bool fromtimer) ;
void jClearSync(void);
void htmlMetaHelper(struct htmlTag *t);
extern void runScriptsPending(void);
void preFormatCheck(int tagno, bool * pretag, bool * slash) ;
char *htmlParse(char *buf, int remote) ;
bool htmlTest(void) ;
void infShow(int tagno, const char *search) ;
bool infReplace(int tagno, const char *newtext, bool notify) ;
bool infPush(int tagno, char **post_string) ;
struct htmlTag *tagFromJavaVar(jsobjtype v);
void javaSubmitsForm(jsobjtype v, bool reset) ;
bool bubble_event(const struct htmlTag *t, const char *name) ;
void runningError(int msg, ...) ;
void rerender(bool notify);
void delTags(int startRange, int endRange);
extern void runOnload(void);
void javaSetsTimeout(int n, const char *jsrc, jsobjtype to, bool isInterval);
bool timerWait(int *delay_sec, int *delay_ms);
void delTimers(struct ebFrame *f);
void runTimer(void);
void javaOpensWindow(const char *href, const char *name) ;
void javaSetsLinkage(bool after, char type, jsobjtype p, const char *rest);

/* sourcefile=html-tidy.c */
void html2nodes(const char *htmltext, bool startpage);

/* sourcefile=decorate.c */
void traverseAll(int start);
const char *attribVal(const struct htmlTag *t, const char *name);
struct htmlTag *findOpenTag(struct htmlTag *t, int action);
struct htmlTag *findOpenList(struct htmlTag *t);
void formControl(struct htmlTag *t, bool namecheck);
void htmlInputHelper(struct htmlTag *t);
char *displayOptions(const struct htmlTag *sel) ;
void prerender(int start);
jsobjtype instantiate_url(jsobjtype parent, const char *name, const char *url) ;
char *render(int start);
void decorate(int start);
void freeTags(struct ebWindow *w) ;
struct htmlTag *newTag(const char *tagname) ;
void initTagArray(void);
void tag_gc(void);
void tag_gc(void);
void htmlNodesIntoTree(int start, struct htmlTag *attach);
void underKill(struct htmlTag *t);
void killTag(struct htmlTag *t);
void html_from_setter( jsobjtype innerParent, const char *h);

/* sourcefile=http.c */
size_t eb_curl_callback(char *incoming, size_t size, size_t nitems, struct i_get *g) ;
uchar base64Bits(char c);
int base64Decode(char *start, char **end);
time_t parseHeaderDate(const char *date) ;
bool parseRefresh(char *ref, int *delay_p) ;
bool shortRefreshDelay(const char *r, int d);
bool httpConnect(struct i_get *g);
void ebcurl_setError(CURLcode curlret, const char *url, int action, const char *curl_error);
void setHTTPLanguage(const char *lang);
int prompt_and_read(int prompt, char *buffer, int buffer_length, int error_message, bool hide_echo);
int ebcurl_debug_handler(CURL * handle, curl_infotype info_desc, char *data, size_t size, struct i_get *g);
int bg_jobs(bool iponly);
void addNovsHost(char *host);
void deleteNovsHosts(void);
CURLcode setCurlURL(CURL * h, const char *url);
bool frameExpand(bool expand, int ln1, int ln2);
int frameExpandLine(int ln, jsobjtype fo);
struct htmlTag *line2frame(int ln);
bool reexpandFrame(void);
bool frameSecurityFile(const char *thisfile);

/* sourcefile=main.c */
const char *mailRedirect(const char *to, const char *from, const char *reply, const char *subj);
bool javaOK(const char *url);
void eb_curl_global_init(void);
void ebClose(int n);
bool isSQL(const char *s);
void setDataSource(char *v);
bool runEbFunction(const char *line);
struct DBTABLE *findTableDescriptor(const char *sn);
struct DBTABLE *newTableDescriptor(const char *name);
void readConfigFile(void);

/* sourcefile=plugin.c */
const struct MIMETYPE *findMimeBySuffix(const char *suffix);
const struct MIMETYPE *findMimeByURL(const char *url);
const struct MIMETYPE *findMimeByFile(const char *filename);
const struct MIMETYPE *findMimeByContent(const char *content);
const struct MIMETYPE *findMimeByProtocol(const char *prot);
bool runPluginCommand(const struct MIMETYPE *m, const char *inurl, const char *infile, const char *indata, int inlength, char **outdata, int *outlength);
int playBuffer(const char *line, const char *playfile);

/* sourcefile=sendmail.c */
bool loadAddressBook(void) ;
const char *reverseAlias(const char *reply) ;
bool encodeAttachment(const char *file, int ismail, bool webform, const char **type_p, const char **enc_p, char **data_p) ;
char *makeBoundary(void) ;
bool sendMail(int account, const char **recipients, const char *body, int subjat, const char **attachments, const char *refline, int nalt, bool dosig) ;
bool validAccount(int n) ;
bool sendMailCurrent(int sm_account, bool dosig) ;

/* sourcefile=messages.c */
void eeCheck(void) ;
void eb_puts(const char *s);
void selectLanguage(void) ; //@ called once at startup
const char *i_getString(int msg); //@ the i_ functions are ok in utf8 mode
void i_puts(int msg) ;
void i_printf(int msg, ...) ;
void i_printfExit(int msg, ...) ;
void i_stringAndMessage(char **s, int *l, int messageNum) ;
void setError(int msg, ...) ; //? use these only in the foreground!
void showError(void) ;
void showErrorConditional(char cmd) ;
void showErrorAbort(void) ;
#if 0
void i_caseShift(unsigned char *s, char action) ;
#endif

// sourcefile=stringfile.c
// Everything in this file is threadsafe except those marked with @,
// which are still threadsafe the way we use them, e.g. only calling
// them from the foreground thread, or ? which is not safe.
// I'd like to do this markup for the other files as time permits.
void *allocMem(size_t n) ;
void *allocZeroMem(size_t n) ;
void *reallocMem(void *p, size_t n) ;
char *allocString(size_t n) ;
char *allocZeroString(size_t n) ;
char *reallocString(void *p, size_t n) ;
void nzFree(void *s) ;
void cnzFree(const void *s) ;
uchar fromHex(char d, char e) ;
char *appendString(char *s, const char *p) ;
char *prependString(char *s, const char *p) ;
void skipWhite(const char **s) ;
#define skipWhite2(s) skipWhite((const char **)s)
void trimWhite(char *s) ;
void stripWhite(char *s) ;
void spaceCrunch(char *s, bool onespace, bool unprint) ;
char *strmove(char *dest, const char *src) ;
char *initString(int *l) ;
void stringAndString(char **s, int *l, const char *t) ;
void stringAndBytes(char **s, int *l, const char *t, int cnt) ;
void stringAndChar(char **s, int *l, char c) ;
void stringAndNum(char **s, int *l, int n) ;
void stringAndKnum(char **s, int *l, int n) ;
char *cloneString(const char *s) ;
char *cloneMemory(const char *s, int n) ;
void leftClipString(char *s) ;
void shiftRight(char *s, char first) ;
char *Cify(const char *s, int n) ;
char *pullString(const char *s, int l) ;
char *pullString1(const char *s, const char *t) ;
int stringIsNum(const char *s) ;
bool stringIsDate(const char *s) ;
bool stringIsFloat(const char *s, double *dp) ;
bool memEqualCI(const char *s, const char *t, int len) ;
char *strstrCI(const char *base, const char *search) ;
bool stringEqual(const char *s, const char *t) ;
bool stringEqualCI(const char *s, const char *t) ;
int stringInList(const char *const *list, const char *s) ;
int stringInListCI(const char *const *list, const char *s) ;
int charInList(const char *list, char c) ;
bool listIsEmpty(const struct listHead * l) ;
void initList(struct listHead *l) ;
void delFromList(void *x) ;
void addToListFront(struct listHead *l, void *x) ;
void addToListBack(struct listHead *l, void *x) ;
void addAtPosition(void *p, void *x) ;
void freeList(struct listHead *l) ;
bool isA(char c) ;
bool isquote(char c) ;
void errorPrint(const char *msg, ...) ;
void debugPrint(int lev, const char *msg, ...) ;
void setDebugFile(const char *name); //@
void nl(void) ;
int perl2c(char *t) ;
unsigned pstLength(pst s) ;
pst clonePstring(pst s) ;
void copyPstring(pst s, const pst t) ;
bool fdIntoMemory(int fd, char **data, int *len) ;
bool fileIntoMemory(const char *filename, char **data, int *len) ;
bool memoryOutToFile(const char *filename, const char *data, int len, int msgcreate, int msgwrite) ;
void caseShift(char *s, char action) ;
char fileTypeByName(const char *name, bool showlink) ;
char fileTypeByHandle(int fd) ;
int fileSizeByName(const char *name) ;
int fileSizeByHandle(int fd) ;
time_t fileTimeByName(const char *name) ;
char *conciseSize(size_t n); //?
char *conciseTime(time_t t); //?
bool lsattrChars(const char *buf, char *dest);
char *lsattr(const char *path, const char *flags); //?
void ttySaveSettings(void) ; //@
void ttySetEcho(bool enable_echo);
#ifndef _INC_CONIO
int getche(void) ; //@
int getch(void) ; //@
#endif // #ifndef _INC_CONIO
char getLetter(const char *s) ;
char *getFileName(int msg, const char *defname, bool isnew, bool ws);
int shellProtectLength(const char *s);
void shellProtect(char *t, const char *s);
const char *nextScanFile(const char *base); //?
bool sortedDirList(const char *dir, struct lineMap **map_p, int *count_p, int othersort, bool reverse) ; //?
bool envFile(const char *line, const char **expanded); //?
bool envFileDown(const char *line, const char **expanded) ; //?
FILE *efopen(const char *name, const char *mode) ;
int eopen(const char *name, int mode, int perms) ;
void appendFile(const char *fname, const char *message, ...) ;
void appendFileNF(const char *filename, const char *msg) ;
int eb_system(const char *cmd, bool print_on_success);

/* sourcefile=url.c */
void unpercentURL(char *url) ;
void unpercentString(char *s) ;
char *percentURL(const char *start, const char *end);
bool looksPercented(const char *start, const char *end);
char *htmlEscape0(const char *s, bool do_and);
#define htmlEscape(s) htmlEscape0((s), true)
#define htmlEscapeTextarea(s) htmlEscape0((s), false)
bool isURL(const char *url) ;
bool isBrowseableURL(const char *url) ;
bool isDataURI(const char *u);
const char *getProtURL(const char *url) ; //?
const char *getHostURL(const char *url) ; //?
bool getProtHostURL(const char *url, char *pp, char *hp);
int getCredsURL(const char *url, char *buf);
const char *getDataURL(const char *url) ;
void getDirURL(const char *url, const char **start_p, const char **end_p) ;
char *findHash(const char *s);
char *getFileURL(const char *url, bool chophash); //?
bool getPortLocURL(const char *url, const char **portloc, int *port) ;
int getPortURL(const char *url) ;
bool isProxyURL(const char *url) ;
char *resolveURL(const char *base, const char *rel) ;
bool sameURL(const char *s, const char *t) ;
char *altText(const char *base) ; //?
char *encodePostData(const char *s, const char *keep_chars) ;
char *decodePostData(const char *data, const char *name, int seqno) ;
void decodeMailURL(const char *url, char **addr_p, char **subj_p, char **body_p) ;

/* sourcefile=jseng-duk.c */
void connectTagObject(struct htmlTag *t, jsobjtype p);
void disconnectTagObject(struct htmlTag *t);
int js_main(void);
// the native versions of the api functions in ebjs.c
void createJavaContext_nat(void);
void freeJavaContext_nat(void);
enum ej_proptype typeof_property_nat(jsobjtype obj, const char *name) ;
bool has_property_nat(jsobjtype obj, const char *name) ;
void delete_property_nat(jsobjtype obj, const char *name) ;
char *get_property_string_nat(jsobjtype obj, const char *name) ;
int get_property_number_nat(jsobjtype parent, const char *name) ;
double get_property_float_nat(jsobjtype parent, const char *name) ;
bool get_property_bool_nat(jsobjtype parent, const char *name) ;
jsobjtype get_property_object_nat(jsobjtype parent, const char *name) ;
jsobjtype get_property_function_nat(jsobjtype parent, const char *name) ;
jsobjtype get_array_element_object_nat(jsobjtype obj, int idx) ;
int set_property_string_nat(jsobjtype obj, const char *name, const char *value) ;
int set_property_number_nat(jsobjtype obj, const char *name, int n) ;
int set_property_float_nat(jsobjtype obj, const char *name, double n) ;
int set_property_bool_nat(jsobjtype obj, const char *name, bool n) ;
int set_property_object_nat(jsobjtype parent, const char *name, jsobjtype child) ;
jsobjtype instantiate_array_nat(jsobjtype parent, const char *name) ;
int set_array_element_object_nat(jsobjtype array, int idx, jsobjtype child) ;
jsobjtype instantiate_array_element_nat(jsobjtype array, int idx, const char *classname) ;
jsobjtype instantiate_nat(jsobjtype parent, const char *name, const char *classname) ;
int set_property_function_nat(jsobjtype parent, const char *name, const char *body) ;
int get_arraylength_nat(jsobjtype a);
bool run_function_bool_nat(jsobjtype obj, const char *name);
void run_function_onearg_nat(jsobjtype obj, const char *name, jsobjtype o);
char *run_script_nat(const char *s);

/* sourcefile=css.c */
void cssDocLoad(char *s);
void cssFree(struct ebFrame *f);
jsobjtype querySelectorAll(const char *selstring, jsobjtype topobj);
jsobjtype querySelector(const char *selstring, jsobjtype topobj);
void cssAttributeCrunch(char *s);
void cssApply(jsobjtype node, jsobjtype destination);
void cssText(jsobjtype node, const char *rulestring);

