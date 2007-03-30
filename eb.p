/* This file is machine-generated, do not hand edit. */

/* sourcefile=main.c */
bool junkSubject(const char *s, char key) ;
const char * mailRedirect(const char *to, const char *from, const char *reply, const char *subj) ;
bool javaOK(const char *url) ;
void eeCheck(void) ;
int main(int argc, char **argv) ;
bool runEbFunction(const char *line) ;
bool bufferToProgram(const char *cmd, const char *suffix, bool trailPercent) ;
struct MIMETYPE * findMimeBySuffix(const char *suffix) ;
struct MIMETYPE * findMimeByProtocol(const char *prot) ;
char * pluginCommand(const struct MIMETYPE *m, const char *file, const char *suffix) ;

/* sourcefile=buffers.c */
pst fetchLine(int n, int show) ;
int currentBufferSize(void) ;
void displayLine(int n) ;
pst inputLine(void) ;
void freeUndoLines(const char *cmap) ;
bool cxCompare(int cx) ;
bool cxActive(int cx) ;
bool cxQuit(int cx, int action) ;
void cxSwitch(int cx, bool interactive) ;
void linesReset(void) ;
bool linesComing(int n) ;
void gotoLocation(char *url, int delay, bool rf) ;
bool addTextToBuffer(const pst inbuf, int length, int destl) ;
bool readFile(const char *filename, const char *post) ;
bool unfoldBuffer(int cx, bool cr, char **data, int *len) ;
bool runCommand(const char *line) ;
bool edbrowseCommand(const char *line, bool script) ;
int sideBuffer(int cx, const char *text, const char *bufname, bool autobrowse) ;
bool browseCurrentBuffer(void) ;
void updateFieldInBuffer(int tagno, const char *newtext, int notify, bool required) ;
char * getFieldFromBuffer(int tagno) ;
int fieldIsChecked(int tagno) ;

/* sourcefile=url.c */
void unpercentURL(char *url) ;
bool isURL(const char *url) ;
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
int fetchHistory(const char *prev, const char *next) ;
const char * firstURL(void) ;
char * resolveURL(const char *base, const char *rel) ;
bool sameURL(const char *s, const char *t) ;
char * altText(const char *base) ;
char * encodePostData(const char *s) ;
char * decodePostData(const char *data, const char *name, int seqno) ;
void decodeMailURL(const char *url, char **addr_p, char **subj_p, char **body_p) ;

/* sourcefile=auth.c */
char * getAuthString(const char *url) ;
bool addWebAuthorization(const char *url, int realm, const char *user, const char *password, bool proxy) ;

/* sourcefile=http.c */
void ssl_init(bool doConfig) ;
void ssl_must_verify(bool verify_flag) ;
char * extractHeaderItem(const char *head, const char *end, const char *item, const char **ptr) ;
char * extractHeaderParam(const char *str, const char *item) ;
time_t parseHeaderDate(const char *date) ;
bool parseRefresh(char *ref, int *delay_p) ;
bool refreshDelay(int sec, const char *u) ;
bool httpConnect(const char *from, const char *url) ;
bool ftpConnect(const char *url) ;
void allIPs(void) ;

/* sourcefile=sendmail.c */
bool loadAddressBook(void) ;
const char * reverseAlias(const char *reply) ;
bool serverPutLine(const char *buf) ;
bool serverGetLine(void) ;
void serverClose(void) ;
bool mailConnect(const char *host, int port) ;
char * base64Encode(const char *inbuf, int inlen, bool lines) ;
bool encodeAttachment(const char *file, int ismail, const char **type_p, const char **enc_p, char **data_p) ;
char * makeBoundary(void) ;
bool sendMail(int account, const char **recipients, const char *body, int subjat, const char **attachments, int nalt, bool dosig) ;
bool validAccount(int n) ;
bool sendMailCurrent(int sm_account, bool dosig) ;

/* sourcefile=fetchmail.c */
void loadBlacklist(void) ;
bool onBlacklist1(IP32bit tip) ;
void fetchMail(int account) ;
bool emailTest(void) ;
char * emailParse(char *buf) ;

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

/* sourcefile=cookies.c */
bool domainSecurityCheck(const char *server, const char *domain) ;
bool receiveCookie(const char *url, const char *str) ;
void cookiesFromJar(void) ;
void sendCookies(char **s, int *l, const char *url, bool issecure) ;

/* sourcefile=stringfile.c */
void * allocMem(unsigned n) ;
void * allocZeroMem(unsigned n) ;
void * reallocMem(void *p, unsigned n) ;
void nzFree(void *s) ;
uchar fromHex(char d, char e) ;
char * appendString(char *s, const char *p) ;
char * prependString(char *s, const char *p) ;
void skipWhite(const char **s) ;
void stripWhite(char *s) ;
void spaceCrunch(char *s, bool onespace, bool unprint) ;
char * initString(int *l) ;
void stringAndString(char **s, int *l, const char *t) ;
void stringAndBytes(char **s, int *l, const char *t, int cnt) ;
void stringAndChar(char **s, int *l, char c) ;
void stringAndNum(char **s, int *l, int n) ;
void stringAndKnum(char **s, int *l, int n) ;
char * cloneString(const char *s) ;
char * cloneMemory(const char *s, int n) ;
char * Cify(const char *s, int n) ;
char * pullString(const char *s, int l) ;
char * pullString1(const char *s, const char *t) ;
int stringIsNum(const char *s) ;
bool stringIsFloat(const char *s, double *dp) ;
bool memEqualCI(const char *s, const char *t, int len) ;
char * strstrCI(const char *base, const char *search) ;
bool stringEqualCI(const char *s, const char *t) ;
int stringInList(const char *const *list, const char *s) ;
int stringInListCI(const char *const *list, const char *s) ;
bool listIsEmpty(const struct listHead * l) ;
void initList(struct listHead *l) ;
void delFromList(void *x) ;
void addToListFront(struct listHead *l, void *x) ;
void addToListBack(struct listHead *l, void *x) ;
void addAtPosition(void *p, void *x) ;
void freeList(struct listHead *l) ;
bool isA(char c) ;
bool isquote(char c) ;
void varargLocals(va_list p, const char *msg, long *locals) ;
void errorPrint(const char *msg, ...) ;
void debugPrint(int lev, const char *msg, ...) ;
void setError(const char *msg, ...) ;
void showError(void) ;
void showErrorConditional(char cmd) ;
void showErrorAbort(void) ;
void browseError(const char *msg, ...) ;
void runningError(const char *msg, ...) ;
int perl2c(char *t) ;
unsigned pstLength(pst s) ;
pst clonePstring(pst s) ;
void copyPstring(pst s, const pst t) ;
bool fileIntoMemory(const char *filename, char **data, int *len) ;
void caseShift(char *s, char action) ;
char fileTypeByName(const char *name, bool showlink) ;
int fileSizeByName(const char *name) ;
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

/* sourcefile=jsdom.c */
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
const char * get_property_url(void *jv, bool doaction) ;
const char * get_property_string(void *jv, const char *name) ;
bool get_property_bool(void *jv, const char *name) ;
void * establish_js_option(void *ev, int idx) ;
bool handlerGo(void *obj, const char *name) ;
void handlerSet(void *ev, const char *name, const char *code) ;
void link_onunload_onclick(void *jv) ;
bool handlerPresent(void *ev, const char *name) ;

