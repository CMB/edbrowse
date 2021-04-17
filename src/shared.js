// stringname=sharedJS
/*********************************************************************
Javascript that loads and runs in the master window.
Functions and classses defined here can be shared by all the edbrowse windows,
if we're very careful!
We have to make sure nothing can be hijacked, starting with the Object.
p = document.createElement("p"); p.toString();
A website might innocently do that.
Nail down prototype and some methods that might innocently be called.
*********************************************************************/

Object.defineProperty(this, "Object",{writable:false,configurable:false});
Object.defineProperty(Object, "prototype",{writable:false,configurable:false});
Object.defineProperty(Object.prototype, "toString",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "toLocaleString",{enumerable:false,writable:false,configurable:false});
// demin.js sets constructor to Object, which it was before, but that means,
// I can't nail this down until demin.js has run its course.
// Object.defineProperty(Object.prototype, "constructor",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "valueOf",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "hasOwnProperty",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "isPrototypeOf",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "propertyIsEnumerable",{enumerable:false,writable:false,configurable:false});

Object.defineProperty(this, "Function",{writable:false,configurable:false});
Object.defineProperty(Function, "prototype",{writable:false,configurable:false});
Object.defineProperty(Function.prototype, "call",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "apply",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "bind",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "toString",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "constructor",{enumerable:false,writable:false,configurable:false});

alert = eb$puts;
// print an error inline, at debug level 3 or higher.
function alert3(s) { logputs(3, s); }
function alert4(s) { logputs(4, s); }

// Dump the tree below a node, this is for debugging.
// Print the first line of text for a text node, and no braces
// because nothing should be below a text node.
// You can make this more elaborate and informative if you wish.
function dumptree(top) {
var nn = top.nodeName.toLowerCase();
var extra = "";
if(nn === "#text" && top.data) {
extra = top.data;
extra = extra.replace(/^[ \t\n]*/, "");
var l = extra.indexOf('\n');
if(l >= 0) extra = extra.substr(0,l);
if(extra.length > 120) extra = extra.substr(0,120);
}
if(nn === "option" && top.text)
extra = top.text;
if(nn === "a" && top.href)
extra = top.href.toString();
if(nn === "base" && top.href)
extra = top.href.toString();
if(extra.length) extra = ' ' + extra;
// some tags should never have anything below them so skip the parentheses notation for these.
if((nn == "base" || nn == "meta" || nn == "link" ||nn == "#text" || nn == "image" || nn == "option" || nn == "input" || nn == "script") &&
(!top.childNodes || top.childNodes.length == 0)) {
alert(nn + extra);
return;
}
alert(nn + " {" + extra);
if(top.dom$class == "Frame") {
if(top.eb$expf) top.contentWindow.dumptree(top.contentDocument);
} else if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
dumptree(c);
}
}
alert("}");
}

function uptrace(t) {
while(t) {
var msg = t.nodeName;
if(t.class) msg += "." + t.class;
if(t.id) msg += "#" + t.id;
alert(msg);
t = t.parentNode;
}
}

// implementation of getElementsByTagName, getElementsByName, and getElementsByClassName.
// These are recursive as they descend through the tree of nodes.

function getElementsByTagName(s) {
if(!s) { // missing or null argument
alert3("getElementsByTagName(type " + typeof s + ")");
return [];
}
s = s.toLowerCase();
return eb$gebtn(this, s);
}

function eb$gebtn(top, s) {
var a = [];
if(s === '*' || (top.nodeName && top.nodeName.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
// don't descend into another frame.
// The frame has no children through childNodes, so we don't really need this line.
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebtn(c, s));
}
}
return a;
}

function getElementsByName(s) {
if(!s) { // missing or null argument
alert3("getElementsByName(type " + typeof s + ")");
return [];
}
s = s.toLowerCase();
return eb$gebn(this, s);
}

function eb$gebn(top, s) {
var a = [];
if(s === '*' || (top.name && top.name.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebn(c, s));
}
}
return a;
}

function getElementById(s) {
if(!s) { // missing or null argument
alert3("getElementById(type " + typeof s + ")");
return null;
}
s = s.toLowerCase();
var a = eb$gebi(this, s);
return a.length ? a[0] : null;
}

// this could stop when it finds the first match, it just doesn't
function eb$gebi(top, s) {
var a = [];
if(s === '*' || (top.id && top.id.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebi(c, s));
}
}
return a;
}

function getElementsByClassName(s) {
s = s.toLowerCase();
return eb$gebcn(this, s);
}

function eb$gebcn(top, s) {
var a = [];
if(s === '*' || (top.class && top.class.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebcn(c, s));
}
}
return a;
}

function nodeContains(n) {  return eb$cont(this, n); }

function eb$cont(top, n) {
if(top === n) return true;
if(!top.childNodes) return false;
if(top.dom$class == "Frame") return false;
for(var i=0; i<top.childNodes.length; ++i)
if(eb$cont(top.childNodes[i], n)) return true;
return false;
}

function dispatchEvent (e) {
if(db$flags(1)) alert3("dispatch " + this.nodeName + " tag " + (this.eb$seqno >= 0 ? this.eb$seqno:"?") + " " + e.type);
e.target = this;
var t = this;
var pathway = [];
while(t) {
pathway.push(t);
if(t.nodeType == 9) break; // don't go past document up to a higher frame
t=t.parentNode;
}
var l = pathway.length;
while(l) {
t = pathway[--l];
e.eventPhase = (l?1:2); // capture or current target
var fn1 = "on" + e.type;
var fn2 = fn1 + "$$fn";
if(typeof t[fn2] == "function") {
if(db$flags(1)) alert3((l?"capture ":"current ") + t.nodeName + "." + e.type);
e.currentTarget = t;
var r = t[fn2](e);
if((typeof r == "boolean" || typeof r == "number") && !r) return false;
if(e.cancelled) return !e.defaultPrevented;
} else if(typeof t[fn1] == "function") {
if(db$flags(1)) alert3((l?"capture ":"current ") + t.nodeName + "." + e.type);
e.currentTarget = t;
if(db$flags(1)) alert3("fire assigned");
var r = t[fn1](e);
if(db$flags(1)) alert3("endfire assigned");
if((typeof r == "boolean" || typeof r == "number") && !r) return false;
if(e.cancelled) return !e.defaultPrevented;
}
}
if(!e.bubbles) return !e.defaultPrevented;
++l; // step up from the target
while(l < pathway.length) {
t = pathway[l++];
e.eventPhase = 3;
var fn2 = "on" + e.type + "$$fn";
if(typeof t[fn2] == "function") {
if(db$flags(1)) alert3("bubble " + t.nodeName + "." + e.type);
e.currentTarget = t;
var r = t[fn2](e);
if((typeof r == "boolean" || typeof r == "number") && !r) return false;
if(e.cancelled) return !e.defaultPrevented;
}
}
return !e.defaultPrevented;
};

/*********************************************************************
This is our addEventListener function.
It is bound to window, which is ok because window has such a function
to listen to load and unload.
Later on we will bind it to document and to other nodes via
class.prototype.addEventListener = addEventListener,
to cover all the instantiated objects in one go.
first arg is a string like click, second arg is a js handler,
Third arg is not used cause I don't understand it.
It calls a lower level function to do the work, which is also called by
attachEvent, as these are almost exactly the same functions.
A similar design applies for removeEventListener and detachEvent.
However, attachEvent is deprecated, and probably shouldn't be used.
I have it enabled for now...
This is frickin complicated, so set eventDebug to debug it.
*********************************************************************/

attachOn = true;

function addEventListener(ev, handler, iscapture) { this.eb$listen(ev,handler, iscapture, true); }
function removeEventListener(ev, handler, iscapture) { this.eb$unlisten(ev,handler, iscapture, true); }
if(attachOn) {
function attachEvent(ev, handler) { this.eb$listen(ev,handler, true, false); }
function detachEvent(ev, handler) { this.eb$unlisten(ev,handler, true, false); }
}

function eb$listen(ev, handler, iscapture, addon) {
if(addon) ev = "on" + ev;
var evfn = ev + "$$fn";
var evarray = ev + "$$array"; // array of handlers
var iscap = false, once = false, passive = false;
// legacy, iscapture could be boolean, or object, or missing
var captype = typeof iscapture;
if(captype == "boolean" && iscapture) iscap = true;
if(captype == "object") {
if(iscapture.capture || iscapture.useCapture) iscap = true;
if(iscapture.once) once = true;
if(iscapture.passive) passive = true; // don't know how to implement this yet
}
if(!handler) {
alert3((addon ? "listen " : "attach ") + this.nodeName + "." + ev + " for " + (iscap?"capture":"bubble") + " with null handler");
return;
}
if(iscap) handler.do$capture = true; else handler.do$bubble = true;
if(once) handler.do$once = true;
if(passive) handler.do$passive = true;
// event handler serial number, for debugging
if(!handler.ehsn) handler.ehsn = db$flags(4);
if(db$flags(1))  alert3((addon ? "listen " : "attach ") + this.nodeName + "." + ev + " tag " + (this.eb$seqno >= 0 ? this.eb$seqno : -1) + " handler " + handler.ehsn + " for " + (handler.do$capture?"capture":"bubble"));

if(!this[evarray]) {
/* attaching the first handler */
if(db$flags(1))  alert3("establish " + this.nodeName + "." + evfn);
eval(
'this["'+evfn+'"] = function(e){ var rc, a = this["' + evarray + '"]; \
if(this["' + ev + '"] && e.eventPhase < 3) { \
alert3("fire orig tag " + (this.eb$seqno >= 0 ? this.eb$seqno : -1)); rc = this["' + ev + '"](e); alert3("endfire orig");} \
if((typeof rc == "boolean" || typeof rc == "number") && !rc) return false; \
for(var i = 0; i<a.length; ++i) a[i].did$run = false; \
for(var i = 0; i<a.length; ++i) { var h = a[i];if(h.did$run) continue; \
if(e.eventPhase== 1 && !h.do$capture || e.eventPhase == 3 && !h.do$bubble) continue; \
var ehsn = h.ehsn; \
if(ehsn) ehsn = "" + ehsn; else ehsn = ""; /* from int to string */ \
h.did$run = true; alert3("fire tag " + (this.eb$seqno >= 0 ? this.eb$seqno : -1) + (ehsn.length ? " handler " + ehsn : "")); rc = h.call(this,e); alert3("endfire handler " + ehsn); \
if(h.do$once) { alert3("once"); this.removeEventListener(e.type, h, h.do$capture); } \
if((typeof rc == "boolean" || typeof rc == "number") && !rc) return false; \
i = -1; \
} return true; };');

this[evarray] = [];
}

var prev_fn = this[ev];
if(prev_fn && handler == prev_fn) {
if(db$flags(1)) alert3("handler duplicates orig");
delete this[ev];
}

for(var j=0; j<this[evarray].length; ++j)
if(this[evarray][j] == handler) {
if(db$flags(1)) alert3("handler is duplicate, move to the end");
this[evarray].splice(j, 1);
break;
}

this[evarray].push(handler);
}

// here is unlisten, the opposite of listen.
// what if every handler is removed and there is an empty array?
// the assumption is that this is not a problem.
function eb$unlisten(ev, handler, iscapture, addon) {
var ehsn = (handler.ehsn ? handler.ehsn : 0);
if(addon) ev = "on" + ev;
if(db$flags(1))  alert3((addon ? "unlisten " : "detach ") + this.nodeName + "." + ev + " tag " + (this.eb$seqno >= 0 ? this.eb$seqno : -1) + " handler " + ehsn);
var evarray = ev + "$$array"; // array of handlers
// remove original html handler after other events have been added.
if(this[ev] == handler) {
delete this[ev];
return;
}
// If other events have been added, check through the array.
if(this[evarray]) {
var a = this[evarray]; // shorthand
for(var i = 0; i<a.length; ++i)
if(a[i] == handler) {
if(iscapture && a[i].do$capture || !iscapture && a[i].do$bubble) {
a.splice(i, 1);
return;
}
}
}
}

// Here comes the Iterator and Walker.
// I wouldn't bother, except for some tests in acid3.
NodeFilter = {
SHOW_ALL:-1,
SHOW_ELEMENT:1,
SHOW_ATTRIBUTE:2,
SHOW_TEXT:4,
SHOW_CDATA_SECTION:8,
SHOW_ENTITY_REFERENCE:16,
SHOW_ENTITY:32,
SHOW_PROCESSING_INSTRUCTION:64,
SHOW_COMMENT:128,
SHOW_DOCUMENT:256,
SHOW_DOCUMENT_TYPE:512,
SHOW_DOCUMENT_FRAGMENT:1024,
SHOW_NOTATION:2048,
// not sure of the values for these
FILTER_ACCEPT:1,
FILTER_REJECT:2,
FILTER_SKIP:3,
};

// This implementation only works on the nodes of a tree
// created object is in the master context; is that ever a problem?
function createNodeIterator(root, mask, callback, unused) {
var o = {}; // the created iterator object
if(typeof callback != "function") callback = null;
o.callback = callback;
if(typeof mask != "number")
mask = 0xffffffff;
// let's reuse some software
if(typeof root == "object") {
o.list = eb$gebtn(root, "*");
if(!root.nodeType)
alert3("NodeIterator root object is not a node");
} else {
o.list = [];
alert3("NodeIterator root is not an object");
}
// apply filters
var i, j;
for(i=j=0; i<o.list.length; ++i) {
var alive = true;
var nt = o.list[i].nodeType;
if(nt == 9 && !(mask&NodeFilter.SHOW_DOCUMENT)) alive = false;
if(nt == 3 && !(mask&NodeFilter.SHOW_TEXT)) alive = false;
if(nt == 1 && !(mask&NodeFilter.SHOW_ELEMENT)) alive = false;
if(nt == 11 && !(mask&NodeFilter.SHOW_DOCUMENT_FRAGMENT)) alive = false;
if(nt == 8 && !(mask&NodeFilter.SHOW_COMMENT)) alive = false;
if(alive)
o.list[j++] = o.list[i];
}
o.list.length = j;
o.idx = 0;
o.bump = function(incr) {
var n = this.idx;
if(incr > 0) --n;
while(true) {
n += incr;
if(n < 0 || n >= this.list.length) return null;
var a = this.list[n];
var rc = NodeFilter.FILTER_ACCEPT;
if(this.callback) rc = this.callback(a);
if(rc == NodeFilter.FILTER_ACCEPT) { if(incr > 0) ++n; this.idx = n; return a; }
// I don't understand the difference between skip and reject
}
}
o.nextNode = function() { return this.bump(1); }
o.previousNode = function() { return this.bump(-1); }
return o;
}

function createTreeWalker(root, mask, callback, unused) {
var o = {}; // the created iterator object
if(typeof callback != "function") callback = null;
o.callback = callback;
if(typeof mask != "number")
mask = 0xffffffff;
if(typeof root == "object") {
o.list = eb$gebtn(root, "*");
if(!root.nodeType)
alert3("TreeWalker root object is not a node");
o.currentNode = root;
} else {
o.list = [];
alert3("TreeWalker root is not an object");
o.currentNode = null;
}
// apply filters
var i, j;
for(i=j=0; i<o.list.length; ++i) {
var alive = true;
var nt = o.list[i].nodeType;
if(nt == 9 && !(mask&NodeFilter.SHOW_DOCUMENT)) alive = false;
if(nt == 3 && !(mask&NodeFilter.SHOW_TEXT)) alive = false;
if(nt == 1 && !(mask&NodeFilter.SHOW_ELEMENT)) alive = false;
if(nt == 11 && !(mask&NodeFilter.SHOW_DOCUMENT_FRAGMENT)) alive = false;
if(nt == 8 && !(mask&NodeFilter.SHOW_COMMENT)) alive = false;
if(alive)
o.list[j++] = o.list[i];
}
o.list.length = j;
o.bump = function(incr) {
var n = this.list.indexOf(this.currentNode);
if(n < 0 || n >= this.list.length) return null;
while(true) {
n += incr;
if(n < 0 || n >= this.list.length) return null;
var a = this.list[n];
var rc = NodeFilter.FILTER_ACCEPT;
if(this.callback) rc = this.callback(a);
if(rc == NodeFilter.FILTER_ACCEPT) { this.currentNode = a; return a; }
}
}
o.nextNode = function() { return this.bump(1); }
o.previousNode = function() { return this.bump(-1); }
o.endkid = function(incr) {
if(typeof this.currentNode != "object") return null;
var a = incr > 0 ? this.currentNode.firstChild : this.currentNode.lastChild;
while(a) {
if(this.list.indexOf(a) >= 0) {
var rc = NodeFilter.FILTER_ACCEPT;
if(this.callback) rc = this.callback(a);
if(rc == NodeFilter.FILTER_ACCEPT) { this.currentNode = a; return a; }
}
a = incr > 0 ? a.nextSibling() : a.previousSibling();
}
return null;
}
o.firstChild = function() { return this.endkid(1); }
o.lastChild = function() { return this.endkid(-1); }
o.nextkid = function(incr) {
if(typeof this.currentNode != "object") return null;
var a = incr > 0 ? this.currentNode.nextSibling : this.currentNode.previousSibling;
while(a) {
if(this.list.indexOf(a) >= 0) {
var rc = NodeFilter.FILTER_ACCEPT;
if(this.callback) rc = this.callback(a);
if(rc == NodeFilter.FILTER_ACCEPT) { this.currentNode = a; return a; }
}
a = incr > 0 ? a.nextSibling() : a.previousSibling();
}
return null;
}
o.nextSibling = function() { return this.nextkid(1); }
o.previousSibling = function() { return this.nextkid(-1); }
o.parentNode = function() {
if(typeof this.currentNode != "object") return null;
var a = this.currentNode.parentNode;
if(a && this.list.indexOf(a) >= 0) {
var rc = NodeFilter.FILTER_ACCEPT;
if(this.callback) rc = this.callback(a);
if(rc == NodeFilter.FILTER_ACCEPT) { this.currentNode = a; return a; }
}
return null;
}
return o;
}

logtime = function(debug, level, obj) {
var today=new Date;
var h=today.getHours();
var m=today.getMinutes();
var s=today.getSeconds();
// add a zero in front of numbers<10
if(h < 10) h = "0" + h;
if(m < 10) m = "0" + m;
if(s < 10) s = "0" + s;
logputs(debug, "console " + level + " [" + h + ":" + m + ":" + s + "] " + obj);
}

defport = {
http: 80,
https: 443,
pop3: 110,
pop3s: 995,
imap: 220,
imaps: 993,
smtp: 25,
submission: 587,
smtps: 465,
proxy: 3128,
ftp: 21,
sftp: 22,
scp: 22,
ftps: 990,
tftp: 69,
gopher: 70,
finger: 79,
telnet: 23,
smb: 139
};

// returns default port as an integer, based on protocol
function setDefaultPort(p) {
var port = 0;
p = p.toLowerCase().replace(/:/, "");
if(defport.hasOwnProperty(p)) port = defport[p];
return port;
}

function camelCase(t) {
return t.replace(/-./g, function(f){return f[1].toUpperCase()});
}
function dataCamel(t) { return camelCase(t.replace(/^data-/,"")); }

isabove = function(a, b) {
var j = 0;
while(b) {
if(b == a) { var e = new Error; e.HIERARCHY_REQUEST_ERR = e.code = 3; throw e; }
if(++j == 1000) { alert3("isabove loop"); break; }
b = b.parentNode;
}
}

// Functions that support classList
function classListRemove() {
for(var i=0; i<arguments.length; ++i) {
for(var j=0; j<this.length; ++j) {
if(arguments[i] != this[j]) continue;
this.splice(j, 1);
--j;
}
}
this.node.class = this.join(' ');
}

function classListAdd() {
for(var i=0; i<arguments.length; ++i) {
for(var j=0; j<this.length; ++j)
if(arguments[i] == this[j]) break;
if(j == this.length) this.push(arguments[i]);
}
this.node.class = this.join(' ');
}

function classListReplace(o, n) {
if(!o) return;
if(!n) { this.remove(o); return; }
for(var j=0; j<this.length; ++j)
if(o == this[j]) { this[j] = n; break; }
this.node.class = this.join(' ');
}

function classListContains(t) {
if(!t) return false;
for(var j=0; j<this.length; ++j)
if(t == this[j]) return true;
return false;
}

function classListToggle(t, force) {
if(!t) return false;
if(arguments.length > 1) {
if(force) this.add(t); else this.remove(t);
return force;
}
if(this.contains(t)) { this.remove(t); return false; }
this.add(t); return true;
}

function classList(node) {
var c = node.class;
if(!c) c = "";
// turn string into array
var a = c.replace(/^\s+/, "").replace(/\s+$/, "").split(/\s+/);
// remember the node you came from
a.node = node;
// attach functions
a.remove = classListRemove;
a.add = classListAdd;
a.replace = classListReplace;
a.contains = classListContains;
a.toggle = classListToggle;
return a;
}

// lock down
var flist = ["alert","alert3","alert4","dumptree","uptrace",
"eb$newLocation","eb$logElement",
"getElementsByTagName", "getElementsByClassName", "getElementsByName", "getElementById","nodeContains",
"eb$gebtn","eb$gebn","eb$gebcn","eb$gebid","eb$cont",
"dispatchEvent","addEventListener","removeEventListener","attachOn",
"attachEvent","detachEvent","eb$listen","eb$unlisten",
"NodeFilter","createNodeIterator","createTreeWalker",
"logtime","defport","setDefaultPort","camelCase","dataCamel","isabove",
"classList","classListAdd","classListRemove","classListReplace","classListToggle","classListContains",
];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(this, flist[i], {writable:false,configurable:false});
