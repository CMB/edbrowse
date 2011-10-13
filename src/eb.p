/* This file is machine-generated, do not hand edit. */

/* sourcefile=main.c */
bool junkSubject(const char *s, char key) ;
const char * mailRedirect(const char *to, const char *from, const char *reply, const char *subj) ;
bool javaOK(const char *url) ;
void ebClose(int n) ;
void eeCheck(void) ;
void setDataSource(char *v) ;
int main(int argc, char **argv) ;
bool runEbFunction(const char *line) ;
bool bufferToProgram(const char *cmd, const char *suffix, bool trailPercent) ;
struct DBTABLE * findTableDescriptor(const char *sn) ;
struct DBTABLE * newTableDescriptor(const char *name) ;
struct MIMETYPE * findMimeBySuffix(const char *suffix) ;
struct MIMETYPE * findMimeByProtocol(const char *prot) ;
char * pluginCommand(const struct MIMETYPE *m, const char *file, const char *suffix) ;

/* sourcefile=buffers.c */
pst fetchLine(int n, int show) ;
int currentBufferSize(void) ;
void displayLine(int n) ;
void initializeReadline(void) ;
pst inputLine(void) ;
void freeUndoLines(const char *cmap) ;
bool cxCompare(int cx) ;
bool cxActive(int cx) ;
bool cxQuit(int cx, int action) ;
void cxSwitch(int cx, bool interactive) ;
void linesReset(void) ;
bool linesComing(int n) ;
void gotoLocation(char *url, int delay, bool rf) ;
bool addTextToBuffer(const pst inbuf, int length, int destl, bool onside) ;
void delText(int start, int end) ;
bool readFile(const char *filename, const char *post) ;
bool unfoldBuffer(int cx, bool cr, char **data, int *len) ;
bool runCommand(const char *line) ;
bool edbrowseCommand(const char *line, bool script) ;
int sideBuffer(int cx, const char *text, int textlen, const char *bufname, bool autobrowse) ;
bool browseCurrentBuffer(void) ;
void updateFieldInBuffer(int tagno, const char *newtext, int notify, bool required) ;
char * getFieldFromBuffer(int tagno) ;
int fieldIsChecked(int tagno) ;

/* sourcefile=url.c */
void unpercentURL(char *url) ;
bool isURL(const char *url) ;
bool isBrowseableURL(const char *url) ;
const char * getProtURL(const char *url) ;
const char * getHostURL(const char *url) ;
const char * getHostPassURL(const char *url) ;
const char * getUserURL(const char *url) ;
const char * getPassURL(const char *url) ;
const char * getDataURL(const char *url) ;
void getDirURL(const char *url, const char **start_p, const char **end_p) ;
bool getPortLocURL(const char *url, const char **portloc, int *port) ;
int getPortURL(const char *url) ;
bool isProxyURL(const char *url) ;
char * resolveURL(const char *base, const char *rel) ;
bool sameURL(const char *s, const char *t) ;
char * altText(const char *base) ;
char * encodePostData(const char *s) ;
char * decodePostData(const char *data, const char *name, int seqno) ;
void decodeMailURL(const char *url, char **addr_p, char **subj_p, char **body_p) ;

/* sourcefile=auth.c */
bool getUserPass(const char *url, char *creds, bool find_proxy) ;
bool addWebAuthorization(const char *url, int realm, const char *credentials, bool proxy) ;

/* sourcefile=http.c */
char * extractHeaderParam(const char *str, const char *item) ;
time_t parseHeaderDate(const char *date) ;
bool parseRefresh(char *ref, int *delay_p) ;
bool refreshDelay(int sec, const char *u) ;
char * copy_and_sanitize(const char *start, const char *end) ;
bool httpConnect(const char *from, const char *url) ;
void allIPs(void) ;
void setHTTPLanguage(const char *lang) ;
void my_curl_init(void) ;

/* sourcefile=messages.c */
void selectLanguage(void) ;
void i_puts(int msg) ;
void i_printf(int msg, ...) ;
void i_printfExit(int msg, ...) ;
void i_stringAndMessage(char **s, int *l, int messageNum) ;
void setError(int msg, ...) ;
void showError(void) ;
void showErrorConditional(char cmd) ;
void showErrorAbort(void) ;
void browseError(int msg, ...) ;
void runningError(int msg, ...) ;
void i_caseShift(unsigned char *s, char action) ;

/* sourcefile=sendmail.c */
bool loadAddressBook(void) ;
const char * reverseAlias(const char *reply) ;
bool mailPutLine(const char *buf, bool realdot) ;
bool mailGetLine(void) ;
void mailPutGetError(const char *line) ;
void mailClose(void) ;
bool mailConnect(const char *host, int port, int secure) ;
char * base64Encode(const char *inbuf, int inlen, bool lines) ;
char * qpEncode(const char *line) ;
bool encodeAttachment(const char *file, int ismail, bool webform, const char **type_p, const char **enc_p, char **data_p) ;
char * makeBoundary(void) ;
bool sendMail(int account, const char **recipients, const char *body, int subjat, const char **attachments, const char *refline, int nalt, bool dosig) ;
bool validAccount(int n) ;
bool sendMailCurrent(int sm_account, bool dosig) ;

/* sourcefile=fetchmail.c */
void loadBlacklist(void) ;
bool onBlacklist1(IP32bit tip) ;
int fetchMail(int account) ;
void scanMail(void) ;
bool emailTest(void) ;
void unpackUploadedFile(const char *post, const char *boundary, char **postb, int *postb_l) ;
char * emailParse(char *buf) ;
bool setupReply(bool all) ;

/* sourcefile=html.c */
void freeTags(void *a) ;
bool tagHandler(int seqno, const char *name) ;
void jsdw(void) ;
void jSyncup(void) ;
void preFormatCheck(int tagno, bool * pretag, bool * slash) ;
char * htmlParse(char *buf, int remote) ;
void findField(const char *line, int ftype, int n, int *total, int *realtotal, int *tagno, char **href, void **evp) ;
void findInputField(const char *line, int ftype, int n, int *total, int *realtotal, int *tagno) ;
bool lineHasTag(const char *p, const char *s) ;
bool htmlTest(void) ;
void infShow(int tagno, const char *search) ;
bool infReplace(int tagno, const char *newtext, int notify) ;
bool infPush(int tagno, char **post_string) ;
void javaSetsTagVar(void *v, const char *val) ;
void javaSubmitsForm(void *v, bool reset) ;
void javaOpensWindow(const char *href, const char *name) ;
void javaSetsTimeout(int n, const char *jsrc, void *to, bool isInterval) ;

/* sourcefile=format.c */
void prepareForBrowse(char *h, int h_len) ;
const char * skipHtmlComment(const char *h, int *lines) ;
bool parseTag(char *e, const char **name, int *namelen, const char **attr, const char **end, int *lines) ;
char * htmlAttrVal(const char *e, const char *name) ;
bool findEndScript(const char *h, const char *tagname, bool is_js, char **end_p, char **new_p, int *lines) ;
void anchorSwap(char *buf) ;
bool breakLine(const char *line, int len, int *newlen) ;
void breakLineSetup(void) ;
char * htmlReformat(const char *buf) ;
char * andTranslate(const char *s, bool invisible) ;
void extractEmailAddresses(char *line) ;
void cutDuplicateEmails(char *tolist, char *cclist, const char *reply) ;
bool looksBinary(const char *buf, int buflen) ;
void looks_8859_utf8(const char *buf, int buflen, bool * iso_p, bool * utf8_p) ;
void iso2utf(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p) ;
void utf2iso(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p) ;
void iuReformat(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p) ;

/* sourcefile=cookies.c */
bool domainSecurityCheck(const char *server, const char *domain) ;
bool receiveCookie(const char *url, const char *str) ;
void cookiesFromJar(void) ;
void sendCookies(char **s, int *l, const char *url, bool issecure) ;

/* sourcefile=stringfile.c */
void * allocMem(size_t n) ;
void * allocZeroMem(size_t n) ;
void * reallocMem(void *p, size_t n) ;
void nzFree(void *s) ;
uchar fromHex(char d, char e) ;
char * appendString(char *s, const char *p) ;
char * prependString(char *s, const char *p) ;
void skipWhite(const char **s) ;
void stripWhite(char *s) ;
void spaceCrunch(char *s, bool onespace, bool unprint) ;
char * strmove(char *dest, const char *src) ;
char * initString(int *l) ;
void stringAndString(char **s, int *l, const char *t) ;
void stringAndBytes(char **s, int *l, const char *t, int cnt) ;
void stringAndChar(char **s, int *l, char c) ;
void stringAndNum(char **s, int *l, int n) ;
void stringAndKnum(char **s, int *l, int n) ;
char * cloneString(const char *s) ;
char * cloneMemory(const char *s, int n) ;
void clipString(char *s) ;
void leftClipString(char *s) ;
void shiftRight(char *s, char first) ;
char * Cify(const char *s, int n) ;
char * pullString(const char *s, int l) ;
char * pullString1(const char *s, const char *t) ;
int stringIsNum(const char *s) ;
bool stringIsFloat(const char *s, double *dp) ;
bool stringIsPDF(const char *s) ;
bool isSQL(const char *s) ;
bool memEqualCI(const char *s, const char *t, int len) ;
char * strstrCI(const char *base, const char *search) ;
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
void ttySaveSettings(void) ;
int getche(void) ;
int getch(void) ;
char getLetter(const char *s) ;
char * nextScanFile(const char *base) ;
bool sortedDirList(const char *dir, int *start, int *end) ;
bool envFile(const char *line, const char **expanded) ;
const char * currentOS(void) ;
const char * currentMachine(void) ;
FILE * efopen(const char *name, const char *mode) ;
int eopen(const char *name, int mode, int perms) ;
void appendFile(const char *fname, const char *message, ...) ;
void appendFileNF(const char *filename, const char *msg) ;

/* sourcefile=jsdom.c */
JSString * our_JS_NewStringCopyN(JSContext * cx, const char *s, size_t n) ;
JSString * our_JS_NewStringCopyZ(JSContext * cx, const char *s) ;
char * our_JSEncodeString(JSString *str) ;
char * transcode_get_js_bytes(JSString * s) ;
void * createJavaContext(void) ;
void freeJavaContext(void *jsc) ;
void establish_innerHTML(void *jv, const char *start, const char *end, bool is_ta) ;
void jMyContext(void) ;
bool javaParseExecute(void *this, const char *str, const char *filename, int lineno) ;
void * domLink(const char *classname, const char *symname, const char *idname, const char *href, const char *href_url, const char *list, void *owner, int radiosel) ;

/* sourcefile=jsloc.c */
const char * stringize(long v) ;
void initLocationClass(void) ;
void establish_property_string(void *jv, const char *name, const char *value, bool readonly) ;
void establish_property_number(void *jv, const char *name, int value, bool readonly) ;
void establish_property_bool(void *jv, const char *name, bool value, bool readonly) ;
void * establish_property_array(void *jv, const char *name) ;
void establish_property_object(void *parent, const char *name, void *child) ;
void establish_property_url(void *jv, const char *name, const char *url, bool readonly) ;
void set_property_string(void *jv, const char *name, const char *value) ;
void set_property_number(void *jv, const char *name, int value) ;
void set_property_bool(void *jv, const char *name, int value) ;
char * get_property_url(void *jv, bool doaction) ;
char * get_property_string(void *jv, const char *name) ;
bool get_property_bool(void *jv, const char *name) ;
char * get_property_option(void *jv) ;
void * establish_js_option(void *ev, int idx) ;
bool handlerGo(void *obj, const char *name) ;
void handlerSet(void *ev, const char *name, const char *code) ;
void link_onunload_onclick(void *jv) ;
bool handlerPresent(void *ev, const char *name) ;

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

