/* This file is machine-generated, do not hand edit. */

/* sourcefile=auth.c */
eb_bool getUserPass(const char *url, char *creds, eb_bool find_proxy) ;
eb_bool addWebAuthorization(const char *url, const char *credentials, eb_bool proxy) ;

/* sourcefile=buffers.c */
pst fetchLine(int n, int show) ;
int currentBufferSize(void) ;
void displayLine(int n) ;
void initializeReadline(void) ;
pst inputLine(void) ;
eb_bool cxCompare(int cx) ;
eb_bool cxActive(int cx) ;
eb_bool cxQuit(int cx, int action) ;
void cxSwitch(int cx, eb_bool interactive) ;
void gotoLocation(char *url, int delay, eb_bool rf) ;
eb_bool addTextToBuffer(const pst inbuf, int length, int destl, eb_bool onside) ;
void delText(int start, int end) ;
eb_bool readFile(const char *filename, const char *post) ;
eb_bool unfoldBuffer(int cx, eb_bool cr, char **data, int *len) ;
eb_bool runCommand(const char *line) ;
eb_bool edbrowseCommand(const char *line, eb_bool script) ;
int sideBuffer(int cx, const char *text, int textlen, const char *bufname, eb_bool autobrowse) ;
eb_bool browseCurrentBuffer(void) ;
void updateFieldInBuffer(int tagno, const char *newtext, int notify, eb_bool required) ;
char *getFieldFromBuffer(int tagno) ;
int fieldIsChecked(int tagno) ;

/* sourcefile=cookies.c */
eb_bool domainSecurityCheck(const char *server, const char *domain) ;
eb_bool receiveCookie(const char *url, const char *str) ;
void cookiesFromJar(void) ;
void sendCookies(char **s, int *l, const char *url, eb_bool issecure) ;

/* sourcefile=dbstubs.c */
eb_bool sqlReadRows(const char *filename, char **bufptr) ;
void dbClose(void) ;
void showColumns(void) ;
void showForeign(void) ;
eb_bool showTables(void) ;
eb_bool sqlDelRows(int start, int end) ;
eb_bool sqlUpdateRow(pst source, int slen, pst dest, int dlen) ;
eb_bool sqlAddRows(int ln) ;
eb_bool ebConnect(void) ;
int goSelect(int *startLine, char **rbuf) ;

/* sourcefile=ebjs.c */
void createJavaContext(void) ;
void freeJavaContext(struct ebWindow *w) ;
void js_shutdown(void) ;
int javaParseExecute(jsobjtype obj, const char *str, const char *filename, int lineno) ;
enum ej_proptype has_property(jsobjtype obj, const char *name) ;
void delete_property(jsobjtype obj, const char *name) ;
char *get_property_string(jsobjtype obj, const char *name) ;
int get_property_number(jsobjtype obj, const char *name) ;
double get_property_float(jsobjtype obj, const char *name) ;
eb_bool get_property_bool(jsobjtype obj, const char *name) ;
jsobjtype get_property_object(jsobjtype parent, const char *name) ;
jsobjtype get_array_element_object(jsobjtype obj, int idx) ;
int set_property_string(jsobjtype obj, const char *name, const char *value) ;
int set_property_number(jsobjtype obj, const char *name, int n) ;
int set_property_float(jsobjtype obj, const char *name, double n) ;
int set_property_bool(jsobjtype obj, const char *name, eb_bool n) ;
int set_property_object(jsobjtype parent, const char *name, jsobjtype child) ;
jsobjtype instantiate_array(jsobjtype parent, const char *name) ;
int set_array_element_object(jsobjtype array, int idx, jsobjtype child) ;
jsobjtype instantiate(jsobjtype parent, const char *name, const char *classname) ;
int set_property_function(jsobjtype parent, const char *name, const char *body) ;
char *get_property_option(jsobjtype obj) ;
void domLink(const char *classname, const char *href, const char *list, jsobjtype owner, int radiosel) ;
jsobjtype new_js_object(void) ;
void setupJavaDom(void) ;
jsobjtype instantiate_url(jsobjtype parent, const char *name, const char *url) ;
char *get_property_url(jsobjtype owner, eb_bool action) ;
void rebuildSelectors(void) ;
void handlerSet(jsobjtype ev, const char *name, const char *code);

/* sourcefile=fetchmail.c */
int fetchMail(int account) ;
int fetchAllMail(void) ;
void scanMail(void) ;
eb_bool emailTest(void) ;
void unpackUploadedFile(const char *post, const char *boundary, char **postb, int *postb_l) ;
char *emailParse(char *buf) ;
eb_bool setupReply(eb_bool all) ;

/* sourcefile=format.c */
void prepareForBrowse(char *h, int h_len) ;
const char *skipHtmlComment(const char *h, int *lines) ;
eb_bool parseTag(char *e, const char **name, int *namelen, const char **attr, const char **end, int *lines) ;
char *htmlAttrVal(const char *e, const char *name) ;
eb_bool findEndScript(const char *h, const char *tagname, eb_bool is_js, char **end_p, char **new_p, int *lines) ;
void anchorSwap(char *buf) ;
eb_bool breakLine(const char *line, int len, int *newlen) ;
void breakLineSetup(void) ;
char *htmlReformat(const char *buf) ;
char *andTranslate(const char *s, eb_bool invisible) ;
void extractEmailAddresses(char *line) ;
void cutDuplicateEmails(char *tolist, char *cclist, const char *reply) ;
eb_bool looksBinary(const char *buf, int buflen) ;
void looks_8859_utf8(const char *buf, int buflen, eb_bool * iso_p, eb_bool * utf8_p) ;
void iso2utf(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p) ;
void utf2iso(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p) ;
void iuReformat(const char *inbuf, int inbuflen, char **outbuf_p, int *outbuflen_p) ;

/* sourcefile=html.cpp */
void freeTags(struct ebWindow *w) ;
eb_bool tagHandler(int seqno, const char *name) ;
void jsdw(void) ;
char *displayOptions(const struct htmlTag *sel) ;
void jSyncup(void) ;
void makeParentNode(const struct htmlTag *t) ;
struct htmlTag *newTag(const char *name) ;
void preFormatCheck(int tagno, eb_bool * pretag, eb_bool * slash) ;
char *htmlParse(char *buf, int remote) ;
void findField(const char *line, int ftype, int n, int *total, int *realtotal, int *tagno, char **href, const struct htmlTag **tagp) ;
void findInputField(const char *line, int ftype, int n, int *total, int *realtotal, int *tagno) ;
eb_bool lineHasTag(const char *p, const char *s) ;
eb_bool htmlTest(void) ;
void infShow(int tagno, const char *search) ;
eb_bool infReplace(int tagno, const char *newtext, int notify) ;
eb_bool infPush(int tagno, char **post_string) ;
void javaSetsTagVar(jsobjtype v, const char *val) ;
void javaSubmitsForm(jsobjtype v, eb_bool reset) ;
void javaOpensWindow(const char *href, const char *name) ;
void javaSetsTimeout(int n, const char *jsrc, jsobjtype to, eb_bool isInterval) ;
eb_bool handlerGoBrowse(const struct htmlTag *t, const char *name) ;

/* sourcefile=http.c */
size_t eb_curl_callback(char *incoming, size_t size, size_t nitems, struct eb_curl_callback_data *data) ;
char *extractHeaderParam(const char *str, const char *item) ;
time_t parseHeaderDate(const char *date) ;
eb_bool parseRefresh(char *ref, int *delay_p) ;
eb_bool refreshDelay(int sec, const char *u) ;
char *copy_and_sanitize(const char *start, const char *end) ;
eb_bool httpConnect(const char *from, const char *url) ;
void ebcurl_setError(CURLcode curlret, const char *url) ;
void setHTTPLanguage(const char *lang) ;
void http_curl_init(void) ;
int ebcurl_debug_handler(CURL * handle, curl_infotype info_desc, char *data, size_t size, void *unused) ;

/* sourcefile=main.c */
const char *mailRedirect(const char *to, const char *from, const char *reply, const char *subj) ;
eb_bool javaOK(const char *url) ;
void ebClose(int n) ;
void eeCheck(void) ;
void setDataSource(char *v) ;
eb_bool runEbFunction(const char *line) ;
eb_bool bufferToProgram(const char *cmd, const char *suffix, eb_bool trailPercent) ;
struct DBTABLE *findTableDescriptor(const char *sn) ;
struct DBTABLE *newTableDescriptor(const char *name) ;
struct MIMETYPE *findMimeBySuffix(const char *suffix) ;
struct MIMETYPE *findMimeByProtocol(const char *prot) ;
char *pluginCommand(const struct MIMETYPE *m, const char *file, const char *suffix) ;

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
eb_bool loadAddressBook(void) ;
const char *reverseAlias(const char *reply) ;
eb_bool encodeAttachment(const char *file, int ismail, eb_bool webform, const char **type_p, const char **enc_p, char **data_p) ;
char *makeBoundary(void) ;
eb_bool sendMail(int account, const char **recipients, const char *body, int subjat, const char **attachments, const char *refline, int nalt, eb_bool dosig) ;
eb_bool validAccount(int n) ;
eb_bool sendMailCurrent(int sm_account, eb_bool dosig) ;

/* sourcefile=stringfile.c */
void *allocMem(size_t n) ;
void *allocZeroMem(size_t n) ;
void *reallocMem(void *p, size_t n) ;
void nzFree(void *s) ;
uchar fromHex(char d, char e) ;
char *appendString(char *s, const char *p) ;
char *prependString(char *s, const char *p) ;
void skipWhite(const char **s) ;
void stripWhite(char *s) ;
void spaceCrunch(char *s, eb_bool onespace, eb_bool unprint) ;
char *strmove(char *dest, const char *src) ;
char *initString(int *l) ;
void stringAndString(char **s, int *l, const char *t) ;
void stringAndBytes(char **s, int *l, const char *t, int cnt) ;
void stringAndChar(char **s, int *l, char c) ;
void stringAndNum(char **s, int *l, int n) ;
void stringAndKnum(char **s, int *l, int n) ;
char *cloneString(const char *s) ;
char *cloneMemory(const char *s, int n) ;
void clipString(char *s) ;
void leftClipString(char *s) ;
void shiftRight(char *s, char first) ;
char *Cify(const char *s, int n) ;
char *pullString(const char *s, int l) ;
char *pullString1(const char *s, const char *t) ;
int stringIsNum(const char *s) ;
eb_bool stringIsDate(const char *s) ;
eb_bool stringIsFloat(const char *s, double *dp) ;
eb_bool stringIsPDF(const char *s) ;
eb_bool isSQL(const char *s) ;
eb_bool memEqualCI(const char *s, const char *t, int len) ;
char *strstrCI(const char *base, const char *search) ;
eb_bool stringEqualCI(const char *s, const char *t) ;
int stringInList(const char *const *list, const char *s) ;
int stringInListCI(const char *const *list, const char *s) ;
int charInList(const char *list, char c) ;
eb_bool listIsEmpty(const struct listHead * l) ;
void initList(struct listHead *l) ;
void delFromList(void *x) ;
void addToListFront(struct listHead *l, void *x) ;
void addToListBack(struct listHead *l, void *x) ;
void addAtPosition(void *p, void *x) ;
void freeList(struct listHead *l) ;
eb_bool isA(char c) ;
eb_bool isquote(char c) ;
void errorPrint(const char *msg, ...) ;
void debugPrint(int lev, const char *msg, ...) ;
void nl(void) ;
int perl2c(char *t) ;
unsigned pstLength(pst s) ;
pst clonePstring(pst s) ;
void copyPstring(pst s, const pst t) ;
eb_bool fdIntoMemory(int fd, char **data, int *len) ;
eb_bool fileIntoMemory(const char *filename, char **data, int *len) ;
eb_bool memoryOutToFile(const char *filename, const char *data, int len, int msgcreate, int msgwrite) ;
void caseShift(char *s, char action) ;
char fileTypeByName(const char *name, eb_bool showlink) ;
char fileTypeByHandle(int fd) ;
int fileSizeByName(const char *name) ;
int fileSizeByHandle(int fd) ;
time_t fileTimeByName(const char *name) ;
void ttySaveSettings(void) ;
int getche(void) ;
int getch(void) ;
char getLetter(const char *s) ;
char *nextScanFile(const char *base) ;
eb_bool sortedDirList(const char *dir, struct lineMap **map_p, int *count_p) ;
eb_bool envFile(const char *line, const char **expanded) ;
FILE *efopen(const char *name, const char *mode) ;
int eopen(const char *name, int mode, int perms) ;
void appendFile(const char *fname, const char *message, ...) ;
void appendFileNF(const char *filename, const char *msg) ;
IP32bit tcp_name_ip(const char *name) ;
char *tcp_ip_dots(IP32bit ip) ;
int tcp_isDots(const char *s) ;
IP32bit tcp_dots_ip(const char *s) ;

/* sourcefile=url.c */
void unpercentURL(char *url) ;
eb_bool isURL(const char *url) ;
eb_bool isBrowseableURL(const char *url) ;
const char *getProtURL(const char *url) ;
const char *getHostURL(const char *url) ;
const char *getHostPassURL(const char *url) ;
const char *getUserURL(const char *url) ;
const char *getPassURL(const char *url) ;
const char *getDataURL(const char *url) ;
void getDirURL(const char *url, const char **start_p, const char **end_p) ;
eb_bool getPortLocURL(const char *url, const char **portloc, int *port) ;
int getPortURL(const char *url) ;
eb_bool isProxyURL(const char *url) ;
char *resolveURL(const char *base, const char *rel) ;
eb_bool sameURL(const char *s, const char *t) ;
char *altText(const char *base) ;
char *encodePostData(const char *s) ;
char *decodePostData(const char *data, const char *name, int seqno) ;
void decodeMailURL(const char *url, char **addr_p, char **subj_p, char **body_p) ;
const char *findProxyForURL(const char *url) ;
void addNovsHost(char *host) ;
CURLcode setCurlURL(CURL * h, const char *url) ;

