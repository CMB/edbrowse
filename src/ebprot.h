/* Prototypes for edbrowse */

/*********************************************************************
Some time ago we used the Mozilla js engine, which is in c++.
If we ever go back to that, or use v8 or any other c++ engine,
we need to indicate that all the other functions are straight C.
*********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

// sourcefile=buffers.c
void undoSpecialClear(void);
void removeHiddenNumbers(pst p, uchar terminate, int cx, const Window *w);
pst fetchLineWindow(int n, int show, const Window *w);
pst fetchLineContext(int n, int show, int cx);
pst fetchLine(int n, int show);
void displayLine(int n);
void printDot(void);
void printPrompt(void);
void initializeReadline(void);
pst inputLine(const bool textEntry);
void saveSubstitutionStrings(void);
void restoreSubstitutionStrings(Window *nw);
Window *createWindow(void);
void freeWindowLines(struct lineMap *map);
void freeWindows(int cx, bool all);
bool cxCompare(int cx) ;
bool cxActive(int cx, bool error);
bool cxQuit(int cx, int action) ;
void cxSwitch(int cx, bool interactive) ;
void addToMap(int nlines, int destl);
bool addTextToBuffer(const uchar *inbuf, int length, int destl, bool showtrail) ;
void addTextToBackend(const char *inbuf);
void delText(int start, int end) ;
bool readFileArgv(const char *filename, int fromframe, const char *orig_head);
bool writeFile(const char *name, int mode);
Tag *line2frame(int ln);
bool jump2anchor(const Tag *jumptag, const char *newhash);
bool runCommand(const char *line);
bool edbrowseCommand(const char *line, bool script) ;
int sideBuffer(int cx, const char *text, int textlen, const char *bufname);
void freeEmptySideBuffer(int n);
bool browseCurrentBuffer(const char *suffix) ;
bool locateTagInBuffer(int tagno, int *ln1_p, int *ln2_p, char **p1_p, char **p2_p, char **s_p, char **t_p);
char *getFieldFromBuffer(int tagno, int ln0);
int fieldIsChecked(int tagno);

// sourcefile=dbodbc.c (and others)
bool fetchForeign(char *tname) ;

// sourcefile=dbops.c
int findColByName(const char *name) ;
bool sqlReadRows(const char *filename, char **bufptr) ;
void dbClose(void) ;
int unfoldRowCheck(int ln);
void sql_unfold(int start, int end, char action);
void showColumns(void) ;
void showForeign(void) ;
bool showTables(void) ;
bool sqlDelRows(int start, int end) ;
bool sqlUpdateRow(int ln, pst source, int slen, pst dest, int dlen) ;
bool sqlAddRows(int ln) ;
bool ebConnect(void) ;
int goSelect(int *startLine, char **rbuf) ;

// sourcefile=fetchmail.c
void setEnvelopeFormat(const char *s);
void setFetchLimit(const char *s);
int fetchMail(int account);
int fetchAllMail(void);
void scanMail(void);
bool emailTest(void);
void mail64Error(int err);
char *emailParse(char *buf);
bool setupReply(bool all);
bool imapBufferPresent(void);
bool imapBuffer(char *line);
bool imap1rf();
bool folderDescend(const char *path, bool rf);
bool folderSearch(const char *path, char *search, bool rf);
bool mailDescend(const char *path, char cmd);
bool imapMarkRead(int l1, int l2, char sign);
bool imapMovecopy(int l1, int l2, char cmd, char *dest);
bool imapDelete(int l1, int l2, char cmd);
bool imapMovecopyWhileReading(char cmd, char *dest);
bool imapDeleteWhileReading(void);
bool saveEmailWhileReading(char key, const char *name);
bool saveEmailWhileEnvelopes(char key, const char *name);
bool rfWhileReading();

// sourcefile=format.c
void prepareForBrowse(char *h, int h_len);
void prepareForField(char *h);
bool breakLine(const char *line, int len, int *newlen);
void breakLineSetup(void);
bool balanceLine(const char *line, int mark);
char *htmlReformat(char *buf);
void extractEmailAddresses(char *line);
void cutDuplicateEmails(char *tolist, char *cclist, const char *reply);
bool isEmailAddress(const char *s);
bool isEmailAddressList(char *s);
int byteOrderMark(const uchar *buf, int buflen);
bool looksBinary(const unsigned char *buf, int buflen);
void looks_8859_utf8(const uchar *buf, int buflen, bool * iso_p, bool * utf8_p);
void iso2utf(const uchar *inbuf, int inbuflen, uchar **outbuf_p, int *outbuflen_p);
void utf2iso(const uchar *inbuf, int inbuflen, uchar **outbuf_p, int *outbuflen_p);
void utf2iso1(char *s, size_t *lenp);
char *iso12utf(const char *t1, const char *t2, int *lenp);
void utfHigh(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p, bool inutf8, bool out32, bool outbig);
char *uni2utf8(unsigned int unichar);
void utfLow(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p, int bom);
void diagnoseAndConvert (char **rbuf, bool *isAllocated_p, int *partSize_p, const bool firstPart, const bool showMessage);
char *force_utf8( char *buf, int buflen);
char *base64Encode(const char *inbuf, int inlen, bool lines);
uchar base64Bits(char c);
int base64Decode(char *start, char **end);
void iuReformat(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p);
char *makeDosNewlines(char *p);
bool parseDataURI(const char *uri, char **mediatype, char **data, int *data_l);
uchar fromHex(char d, char e);
char *closeColor(const char *s);
void clearEmojis(void);
void loadEmojis(void);
char *selectEmoji(const char *p, int len);
void selectLanguage(void);
const char *i_message(int msg);
void i_puts(int msg);
void i_printf(int msg, ...);
void i_printfExit(int msg, ...);
void i_stringAndMessage(char **s, int *l, int messageNum);
void setError(int msg, ...); //? use these only in the foreground!
void showError(void);
void showErrorConditional(char cmd);
void showErrorAbort(void);
void eb_puts(const char *s);
bool helpUtility(void);

// sourcefile=html.c
void dwStart(void);
bool tagHandler(int seqno, const char *name);
void jSyncup(bool fromtimer, const Tag *active);
void jSideEffects(void);
void jClearSync(void);
void htmlMetaHelper(const Tag *t);
void prepareScript(Tag *t);
bool isRooted(const Tag *t);
void runScriptsPending(bool startbrowse);
void preFormatCheck(int tagno, bool * pretag, bool * slash);
char *htmlParse(char *buf, int remote);
bool htmlTest(void);
void infShow(int tagno, const char *search);
bool infReplace(int tagno, char *newtext, bool notify);
char *displayOptions(const Tag *sel);
bool infPush(int tagno, char **post_string);
void domSetsTagValue(Tag *t, const char *newtext);
void domSubmitsForm(Tag *t, bool reset);
bool charInOptions(char c);
void charFixOptions(char c);
bool charInFiles(char c);
void charFixFiles(char c);
const Tag *gebi_c(const Tag *t, const char *id, bool lookname);
void runningError(int msg, ...);
void rerender(int notify);
void delTags(int startRange, int endRange);
void runOnload(void);
const char *tack_fn(const char *e);
void domSetsTimeout(int n, const char *jsrc, const char *backlink, bool isInterval);
void scriptOnTimer( Tag *t);
bool timerWait(int *delay_sec, int *delay_ms);
void delTimers(const Frame *f);
void runTimer(void);
void showTimers(void);
void domOpensWindow(const char *href, const char *name);
int tableType(const Tag *t);
bool all_th(const Tag *tr);
char *render(void);
bool itext(int d);
struct htmlTag *line2tr(int ln);
bool showHeaders(int ln);
void html_from_setter( Tag *innerParent, const char *h);

// sourcefile=html-tags.c
void htmlScanner(const char *htmltext, Tag *above, bool isgen);
void setTagAttr(Tag *t, const char *name, char *val);
const char *attribVal(const Tag *t, const char *name);
bool attribPresent(const Tag *t, const char *name);
Tag *newTag(const Frame *f, const char *tagname);
void freeTags(struct ebWindow *w);
void initTagArray(void);
void traverseAll(void);
Tag *findOpenTag(Tag *t, int action);
Tag *findOpenSection(Tag *t);
Tag *findOpenList(Tag *t);
void formControl(Tag *t, bool namecheck);
void htmlInputHelper(Tag *t);
void prerender(void);
const char *fakePropName(void);
void decorate(void);
void rowspan(void);

// sourcefile=http.c
void eb_curl_global_init(void);
void eb_curl_global_cleanup(void);
size_t eb_curl_callback(char *incoming, size_t size, size_t nitems, struct i_get *g);
time_t parseHeaderDate(const char *date);
bool parseRefresh(char *ref, int *delay_p);
bool shortRefreshDelay(const char *r, int d);
bool httpConnect(struct i_get *g);
void *httpConnectBack1(void *ptr);
void *httpConnectBack2(void *ptr);
void *httpConnectBack3(void *ptr);
void ebcurl_setError(CURLcode curlret, const char *url, int action, const char *curl_error);
int ftpWrite(const char *url);
void setHTTPLanguage(const char *lang);
int prompt_and_read(int prompt, char *buffer, int buffer_length, int error_message, bool hide_echo);
int ebcurl_debug_handler(CURL * handle, curl_infotype info_desc, char *data, size_t size, struct i_get *g);
int bg_jobs(bool iponly);
CURLcode setCurlURL(CURL * h, const char *url);
bool frameExpand(bool expand, int ln1, int ln2);
int frameExpandLine(int ln, Tag *t);
bool reexpandFrame(void);

// sourcefile=main.c
void ebClose(int n);
void eeCheck(void);
void setDataSource(char *v);
bool javaOK(const char *url);
bool mustVerifyHost(const char *url);
const char *findProxyForURL(const char *url);
const char *findAgentForURL(const char *url);
const char *mailRedirect(const char *to, const char *from, const char *reply, const char *subj);
int runEbFunction(const char *line);
const char *getInputLineFromScript(void);
bool runBuffer(int b, const Window *w, bool stopflag, int ln1, int ln2);
struct DBTABLE *findTableDescriptor(const char *sn);
struct DBTABLE *newTableDescriptor(const char *name);
bool readConfigFile(void);
const char *fetchReplace(const char *u);

// sourcefile=sendmail.c
bool loadAddressBook(void);
const char *reverseAlias(const char *reply);
bool encodeAttachment(const char *file, int ismail, bool webform, const char **type_p, const char **enc_p, char **data_p, bool *long_p);
char *makeBoundary(void);
bool sendMail(int account, const char **recipients, const char *body, int subjat, const char **attachments, const char *refline, int nalt, bool dosig);
bool validAccount(int n);
bool sendMailCurrent(int sm_account, bool dosig);

// sourcefile=stringfile.c
// Everything in this file is threadsafe except those marked with @,
// which are still threadsafe the way we use them, e.g. only calling
// them from the foreground thread, or ? which is not safe.
// I'd like to do this markup for the other files as time permits.
void *allocMem(size_t n);
void *allocZeroMem(size_t n);
void *reallocMem(void *p, size_t n);
char *allocString(size_t n) ;
char *allocZeroString(size_t n) ;
char *reallocString(void *p, size_t n) ;
void nzFree(void *s) ;
void cnzFree(const void *s) ;
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
#define stringAndMessage(s, l, m) stringAndString(s, l, i_message(m))
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
const char *stringInBufLine(const char *s, const char *t);
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
unsigned pstLength(const uchar *s) ;
pst clonePstring(const uchar *s) ;
void copyPstring(pst s, const pst t) ;
int comparePstring(const uchar * s, const uchar * t);
int fdIntoMemory(int fd, char **data, int *len, bool inparts);
int fileIntoMemory(const char *filename, char **data, int *len, bool inparts);
bool memoryOutToFile(const char *filename, const char *data, int len, int msgcreate, int msgwrite) ;
void truncate0(const char *filename, int fh);
long long bufferSizeW(const Window *w, bool browsing);
long long bufferSize(int cx, bool browsing);
bool unfoldBufferW(const struct ebWindow *w, bool cr, char **data, int *len);
bool unfoldBuffer(int cx, bool cr, char **data, int *len);
void caseShift(char *s, char action) ;
void camelCase(char *s);
char fileTypeByName(const char *name, int showlink) ;
char fileTypeByHandle(int fd) ;
off_t fileSizeByName(const char *name) ;
off_t fileSizeByHandle(int fd) ;
time_t fileTimeByName(const char *name) ;
char *conciseSize(size_t n); //?
char *conciseTime(time_t t); //?
bool lsattrChars(const char *buf, char *dest);
char *lsattr(const char *path, const char *flags); //?
void ttySaveSettings(void) ; //@
void ttyRestoreSettings(void);
void ttyRaw(int charcount, int timeout, bool isecho);
void ttySetEcho(bool enable_echo);
#ifndef _INC_CONIO
int getche(void) ; //@
int getch(void) ; //@
#endif // #ifndef _INC_CONIO
char getLetter(const char *s) ;
char *getFileName(int msg, const char *defname, bool isnew, bool ws);
int shellProtectLength(const char *s);
void shellProtect(char *t, const char *s);
char *dirSuffixWindow(int n, const Window *w);
char *dirSuffix(int n);
char *dirSuffix2(int n, const char *path);
char *makeAbsPath(const char *f);
const char *nextScanFile(const char *base);
bool readDirectory(const char *filename, int endline, char cmd, struct lineMap **map_p);
bool delFiles(int start, int end, bool withtext, char origcmd, char *cmd_p);
bool moveFiles(int start, int end, int dest, char origcmd, char relative);
bool envFile(const char *line, const char **expanded); //@
char *envFileAlloc(const char *line); //@
bool envFileDown(const char *line, const char **expanded) ; //@
FILE *efopen(const char *name, const char *mode) ;
int eopen(const char *name, int mode, int perms) ;
void appendFile(const char *fname, const char *message, ...) ;
void appendFileNF(const char *filename, const char *msg) ;
int eb_system(const char *cmd, bool print_on_success);

// sourcefile=isup.c
void unpercentURL(char *url) ;
void unpercentString(char *s) ;
char *percentURL(const char *start, const char *end);
bool looksPercented(const char *start, const char *end);
char *htmlEscape0(const char *s, bool do_and);
#define htmlEscape(s) htmlEscape0((s), true)
#define htmlEscapeTextarea(s) htmlEscape0((s), false)
bool isURL(const char *url) ;
bool isSQL(const char *url);
bool isBrowseableURL(const char *url) ;
bool isDataURI(const char *u);
const char *getProtURL(const char *url) ; //?
bool missingProtURL(const char *url);
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
bool patternMatchURL(const char *url, const char *pattern);
bool frameSecurityFile(const char *thisfile);
bool receiveCookie(const char *url, const char *str) ;
void cookiesFromJar(void) ;
bool isInDomain(const char *d, const char *s);
void findcookies(char **s, int *l, const char *url, bool issecure) ;
void mergeCookies(void);
void setupEdbrowseCache(void);
void clearCache(void) ;
bool fetchCache(const char * url, const char *etag, time_t modtime, bool grab, char **data, int *data_len) ;
bool presentInCache(const char *url, bool *recent) ;
void storeCache(const char *url, const char *etag, time_t modtime, const char *data, int datalen) ;
bool getUserPass(const char *url, char *creds, bool find_proxy) ;
bool getUserPassRealm(const char *url, char *creds, const char *realm);
// Add authorization entries only in the foreground, but it's an
// atomic operation, so the previous routines can run from other threads.
bool addWebAuthorization(const char *url, const char *credentials, bool proxy, const char *realm);
const char *edbrowseTempFilename(const char *suffix, bool output);
const struct MIMETYPE *findMimeByURL(const char *url, uchar *sxfirst);
const struct MIMETYPE *findMimeBySuffix(const char *suffix);
const struct MIMETYPE *findMimeByFile(const char *filename);
const struct MIMETYPE *findMimeByContent(const char *content);
bool runPluginCommand(const struct MIMETYPE *m, const char *inurl, const char *infile, const char *indata, int inlength, char **outdata, int *outlength);
int playBuffer(const char *line, const char *playfile);
void ircSetFileName(Window *w);
bool ircWrite(void);
void ircRead(void);
bool ircSetup(char *line);
void ircClose(Window *w);
void ircReadlineControl(void);
void ircReadlineRelease(void);

// sourcefile=css.c
void writeShortCache(void);
bool matchMedia(char *t);
Frame *frameFromWindow(int gsn);
void cssDocLoad(int frameNumber, char *s, bool pageload);
void cssFree(Frame *f);
Tag **querySelectorAll(const char *selstring, Tag *top);
Tag *querySelector(const char *selstring, Tag *top);
bool querySelector0(const char *selstring, Tag *top);
void cssApply(int frameNumber, Tag *t, int pe);
void cssText(const char *rulestring);

// sourcefile=jseng-quick.c
void disconnectTagObject(Tag *t);
void reconnectTagObject(Tag *t);
bool has_property_t(const Tag *t, const char *name);
bool has_property_win(const Frame *f, const char *name) ;
void set_property_object_t(const Tag *t, const char *name, const Tag *t2);
bool run_function_bool_t(const Tag *t, const char *name);
bool run_function_bool_win(const Frame *f, const char *name);
void forceFrameExpand(Tag *t);
void my_ExecutePendingJobs(void);
void my_ExecutePendingMessages(void);
void my_ExecutePendingMessagePorts(void);
void delPendings(const Frame *f);
void js_main(void);
void createJSContext(Frame *f);
void freeJSContext(Frame *f);
void run_ontimer(const Frame *f, const char *backlink);
int run_function_onearg_t(const Tag *t, const char *name, const Tag *t2);
int run_function_onearg_win(const Frame *f, const char *name, const Tag *t2);
int run_function_onearg_doc(const Frame *f, const char *name, const Tag *t2);
void run_function_onestring_t(const Tag *t, const char *name, const char *s);
char *run_function_onestring1_t(const Tag *t, const char *name, const char *s);
void run_function_twostring_t(const Tag *t, const char *name, const char *s1, const char *s2);
void run_function_onestring_win(const Frame *f, const char *name, const char *s);
void jsRunData(const Tag *t, const char *filename, int lineno);
bool run_event_t(const Tag *t, const char *pname, const char *evname);
bool run_event_win(const Frame *f, const char *pname, const char *evname);
bool run_event_doc(const Frame *f, const char *pname, const char *evname);
bool bubble_event_t(const Tag *t, const char *name);
void set_property_bool_win(const Frame *f, const char *name, bool v);
void set_property_bool_doc(const Frame *f, const char *name, bool v);
char *get_property_url_t(const Tag *t, bool action);
char *get_style_string_t(const Tag *t, const char *name);
void delete_property_t(const Tag *t, const char *name);
void delete_property_win(const Frame *f, const char *name);
void delete_property_doc(const Frame *f, const char *name);
bool get_property_bool_t(const Tag *t, const char *name);
enum ej_proptype typeof_property_t(const Tag *t, const char *name);
int get_property_number_t(const Tag *t, const char *name);
char * get_property_string_t(const Tag *t, const char *name);
void set_property_bool_t(const Tag *t, const char *name, bool v);
void set_property_number_t(const Tag *t, const char *name, int v);
void set_property_string_t(const Tag *t, const char *name, const char * v);
void set_property_string_win(const Frame *f, const char *name, const char *v);
void set_property_object_doc(const Frame *f, const char *name, const Tag *t2);
void set_property_string_doc(const Frame *f, const char *name, const char *v);
void jsRunScriptWin(const char *str, const char *filename, 		 int lineno);
void jsRunScript_t(const Tag *t, const char *str, const char *filename, 		 int lineno);
char *jsRunScriptWinResult(const char *str, const char *filename, int lineno) ;
void establish_js_option(Tag *t, Tag *sel, Tag *og);
void establish_js_textnode(Tag *t, const char *fpn);
void domLink(Tag *t, const char *classname, const char *list, const Tag *owntag, int extra);
void rebuildSelectors(void);
bool has_gcs(const char *name);
enum ej_proptype typeof_gcs(const char *name);
int get_gcs_number(const char *name);
void set_gcs_number(const char *name, int n);
void set_gcs_bool(const char *name, bool v);
void set_gcs_string(const char *name, const char *s);
void jsClose(void);
void underKill(Tag *t);
void set_basehref(const char *b);
void set_location_hash(const char *h);

#ifdef __cplusplus
}
#endif
