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
if(!s) { // missing or null argument
alert3("getElementsByTagName(type " + typeof s + ")");
return [];
}
s = s.toLowerCase() . replace (/^\s+/, '') . replace (/\s+$/, '');
if(s === "") return [];
var sa = s.split(/\s+/);
return eb$gebcn(this, sa);
}

function eb$gebcn(top, sa) {
var a = [];
if(top.cl$present) {
var i;
for(i=0; i<sa.length; ++i) {
var w = sa[i];
if(w === '*') { a.push(top); break; }
if(!top.classList.contains(w)) break;
}
if(i == sa.length) a.push(top);
}
if(top.childNodes) {
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebcn(c, sa));
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

// support functions for mutation records
function mrList(x) {
if(Array.isArray(x)) {
// return a copy of the array
return [].concat(x);
}
if(typeof x == "number") return [];
return x ? [x] : [];
}

function mrKids(r, b, y, z) {
r.target = b;
r.type = "childList";
r.oldValue = null;
r.addedNodes = mrList(y);
r.removedNodes = mrList(z);
r.nextSibling = r.previousSibling = null; // this is for innerHTML
// if adding a single node then we can just compute the siblings
if(y && y.nodeType && y.parentNode)
r.previousSibling = y.previousSibling, r.nextSibling = y.nextSibling;
// if z is a node it is removeChild(), and is gone,
// and y is the integer where it was.
if(z && z.nodeType && typeof y == "number") {
var c = b.childNodes;
var l = c.length;
r.nextSibling = y < l ? c[y] : null;
--y;
r.previousSibling = y >= 0 ? c[y] : null;
}
}

// placeholder for URL class, I'm not comfortable sharing our hand-built
// URL class yet.
// But this has to be here for the Blob code.
URL = {};

// Code beyond this point is third party, but necessary for the operation of the browser.

/* Blob.js
 * A Blob, File, FileReader & URL implementation.
 * 2019-04-19
 *
 * By Eli Grey, http://eligrey.com
 * By Jimmy Wärting, https://github.com/jimmywarting
 * License: MIT
 *   See https://github.com/eligrey/Blob.js/blob/master/LICENSE.md
 */

;(function () {
  var global = typeof window === 'object'
      ? window : typeof self === 'object'
      ? self : this

  var BlobBuilder = global.BlobBuilder
    || global.WebKitBlobBuilder
    || global.MSBlobBuilder
    || global.MozBlobBuilder

  global.URL = global.URL || global.webkitURL || function (href, a) {
  	a = document.createElement('a')
  	a.href = href
  	return a
  }

  var origBlob = global.Blob
  var createObjectURL = URL.createObjectURL
  var revokeObjectURL = URL.revokeObjectURL
  var strTag = global.Symbol && global.Symbol.toStringTag
  var blobSupported = false
  var blobSupportsArrayBufferView = false
  var arrayBufferSupported = !!global.ArrayBuffer
  var blobBuilderSupported = BlobBuilder
    && BlobBuilder.prototype.append
    && BlobBuilder.prototype.getBlob

  try {
    // Check if Blob constructor is supported
    blobSupported = new Blob(['ä']).size === 2

    // Check if Blob constructor supports ArrayBufferViews
    // Fails in Safari 6, so we need to map to ArrayBuffers there.
    blobSupportsArrayBufferView = new Blob([new Uint8Array([1, 2])]).size === 2
  } catch (e) {}

  /**
   * Helper function that maps ArrayBufferViews to ArrayBuffers
   * Used by BlobBuilder constructor and old browsers that didn't
   * support it in the Blob constructor.
   */
  function mapArrayBufferViews (ary) {
    return ary.map(function (chunk) {
      if (chunk.buffer instanceof ArrayBuffer) {
        var buf = chunk.buffer

        // if this is a subarray, make a copy so we only
        // include the subarray region from the underlying buffer
        if (chunk.byteLength !== buf.byteLength) {
          var copy = new Uint8Array(chunk.byteLength)
          copy.set(new Uint8Array(buf, chunk.byteOffset, chunk.byteLength))
          buf = copy.buffer
        }

        return buf
      }

      return chunk
    })
  }

  function BlobBuilderConstructor (ary, options) {
    options = options || {}

    var bb = new BlobBuilder()
    mapArrayBufferViews(ary).forEach(function (part) {
      bb.append(part)
    })

    return options.type ? bb.getBlob(options.type) : bb.getBlob()
  }

  function BlobConstructor (ary, options) {
    return new origBlob(mapArrayBufferViews(ary), options || {})
  }

  if (global.Blob) {
    BlobBuilderConstructor.prototype = Blob.prototype
    BlobConstructor.prototype = Blob.prototype
  }



  /********************************************************/
  /*               String Encoder fallback                */
  /********************************************************/
  function stringEncode (string) {
    var pos = 0
    var len = string.length
    var Arr = global.Uint8Array || Array // Use byte array when possible

    var at = 0  // output position
    var tlen = Math.max(32, len + (len >> 1) + 7)  // 1.5x size
    var target = new Arr((tlen >> 3) << 3)  // ... but at 8 byte offset

    while (pos < len) {
      var value = string.charCodeAt(pos++)
      if (value >= 0xd800 && value <= 0xdbff) {
        // high surrogate
        if (pos < len) {
          var extra = string.charCodeAt(pos)
          if ((extra & 0xfc00) === 0xdc00) {
            ++pos
            value = ((value & 0x3ff) << 10) + (extra & 0x3ff) + 0x10000
          }
        }
        if (value >= 0xd800 && value <= 0xdbff) {
          continue  // drop lone surrogate
        }
      }

      // expand the buffer if we couldn't write 4 bytes
      if (at + 4 > target.length) {
        tlen += 8  // minimum extra
        tlen *= (1.0 + (pos / string.length) * 2)  // take 2x the remaining
        tlen = (tlen >> 3) << 3  // 8 byte offset

        var update = new Uint8Array(tlen)
        update.set(target)
        target = update
      }

      if ((value & 0xffffff80) === 0) {  // 1-byte
        target[at++] = value  // ASCII
        continue
      } else if ((value & 0xfffff800) === 0) {  // 2-byte
        target[at++] = ((value >> 6) & 0x1f) | 0xc0
      } else if ((value & 0xffff0000) === 0) {  // 3-byte
        target[at++] = ((value >> 12) & 0x0f) | 0xe0
        target[at++] = ((value >> 6) & 0x3f) | 0x80
      } else if ((value & 0xffe00000) === 0) {  // 4-byte
        target[at++] = ((value >> 18) & 0x07) | 0xf0
        target[at++] = ((value >> 12) & 0x3f) | 0x80
        target[at++] = ((value >> 6) & 0x3f) | 0x80
      } else {
        // FIXME: do we care
        continue
      }

      target[at++] = (value & 0x3f) | 0x80
    }

    return target.slice(0, at)
  }

  /********************************************************/
  /*               String Decoder fallback                */
  /********************************************************/
  function stringDecode (buf) {
    var end = buf.length
    var res = []

    var i = 0
    while (i < end) {
      var firstByte = buf[i]
      var codePoint = null
      var bytesPerSequence = (firstByte > 0xEF) ? 4
        : (firstByte > 0xDF) ? 3
          : (firstByte > 0xBF) ? 2
            : 1

      if (i + bytesPerSequence <= end) {
        var secondByte, thirdByte, fourthByte, tempCodePoint

        switch (bytesPerSequence) {
          case 1:
            if (firstByte < 0x80) {
              codePoint = firstByte
            }
            break
          case 2:
            secondByte = buf[i + 1]
            if ((secondByte & 0xC0) === 0x80) {
              tempCodePoint = (firstByte & 0x1F) << 0x6 | (secondByte & 0x3F)
              if (tempCodePoint > 0x7F) {
                codePoint = tempCodePoint
              }
            }
            break
          case 3:
            secondByte = buf[i + 1]
            thirdByte = buf[i + 2]
            if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80) {
              tempCodePoint = (firstByte & 0xF) << 0xC | (secondByte & 0x3F) << 0x6 | (thirdByte & 0x3F)
              if (tempCodePoint > 0x7FF && (tempCodePoint < 0xD800 || tempCodePoint > 0xDFFF)) {
                codePoint = tempCodePoint
              }
            }
            break
          case 4:
            secondByte = buf[i + 1]
            thirdByte = buf[i + 2]
            fourthByte = buf[i + 3]
            if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80 && (fourthByte & 0xC0) === 0x80) {
              tempCodePoint = (firstByte & 0xF) << 0x12 | (secondByte & 0x3F) << 0xC | (thirdByte & 0x3F) << 0x6 | (fourthByte & 0x3F)
              if (tempCodePoint > 0xFFFF && tempCodePoint < 0x110000) {
                codePoint = tempCodePoint
              }
            }
        }
      }

      if (codePoint === null) {
        // we did not generate a valid codePoint so insert a
        // replacement char (U+FFFD) and advance only 1 byte
        codePoint = 0xFFFD
        bytesPerSequence = 1
      } else if (codePoint > 0xFFFF) {
        // encode to utf16 (surrogate pair dance)
        codePoint -= 0x10000
        res.push(codePoint >>> 10 & 0x3FF | 0xD800)
        codePoint = 0xDC00 | codePoint & 0x3FF
      }

      res.push(codePoint)
      i += bytesPerSequence
    }

    var len = res.length
    var str = ''
    var i = 0

    while (i < len) {
      str += String.fromCharCode.apply(String, res.slice(i, i += 0x1000))
    }

    return str
  }

  // string -> buffer
  var textEncode = typeof TextEncoder === 'function'
    ? TextEncoder.prototype.encode.bind(new TextEncoder())
    : stringEncode

  // buffer -> string
  var textDecode = typeof TextDecoder === 'function'
    ? TextDecoder.prototype.decode.bind(new TextDecoder())
    : stringDecode

  function FakeBlobBuilder () {
    function isDataView (obj) {
      return obj && DataView.prototype.isPrototypeOf(obj)
    }
    function bufferClone (buf) {
      var view = new Array(buf.byteLength)
      var array = new Uint8Array(buf)
      var i = view.length
      while (i--) {
        view[i] = array[i]
      }
      return view
    }
    function array2base64 (input) {
      var byteToCharMap = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/='

      var output = []

      for (var i = 0; i < input.length; i += 3) {
        var byte1 = input[i]
        var haveByte2 = i + 1 < input.length
        var byte2 = haveByte2 ? input[i + 1] : 0
        var haveByte3 = i + 2 < input.length
        var byte3 = haveByte3 ? input[i + 2] : 0

        var outByte1 = byte1 >> 2
        var outByte2 = ((byte1 & 0x03) << 4) | (byte2 >> 4)
        var outByte3 = ((byte2 & 0x0F) << 2) | (byte3 >> 6)
        var outByte4 = byte3 & 0x3F

        if (!haveByte3) {
          outByte4 = 64

          if (!haveByte2) {
            outByte3 = 64
          }
        }

        output.push(
          byteToCharMap[outByte1], byteToCharMap[outByte2],
          byteToCharMap[outByte3], byteToCharMap[outByte4]
        )
      }

      return output.join('')
    }

    var create = Object.create || function (a) {
      function c () {}
      c.prototype = a
      return new c()
    }

    if (arrayBufferSupported) {
      var viewClasses = [
        '[object Int8Array]',
        '[object Uint8Array]',
        '[object Uint8ClampedArray]',
        '[object Int16Array]',
        '[object Uint16Array]',
        '[object Int32Array]',
        '[object Uint32Array]',
        '[object Float32Array]',
        '[object Float64Array]'
      ]

      var isArrayBufferView = ArrayBuffer.isView || function (obj) {
        return obj && viewClasses.indexOf(Object.prototype.toString.call(obj)) > -1
      }
    }

    function concatTypedarrays (chunks) {
      var size = 0
      var i = chunks.length
      while (i--) { size += chunks[i].length }
      var b = new Uint8Array(size)
      var offset = 0
      for (i = 0, l = chunks.length; i < l; i++) {
        var chunk = chunks[i]
        b.set(chunk, offset)
        offset += chunk.byteLength || chunk.length
      }

      return b
    }

    /********************************************************/
    /*                   Blob constructor                   */
    /********************************************************/
    function Blob (chunks, opts) {
      chunks = chunks || []
      opts = opts == null ? {} : opts
      for (var i = 0, len = chunks.length; i < len; i++) {
        var chunk = chunks[i]
        if (chunk instanceof Blob) {
          chunks[i] = chunk._buffer
        } else if (typeof chunk === 'string') {
          chunks[i] = textEncode(chunk)
        } else if (arrayBufferSupported && (ArrayBuffer.prototype.isPrototypeOf(chunk) || isArrayBufferView(chunk))) {
          chunks[i] = bufferClone(chunk)
        } else if (arrayBufferSupported && isDataView(chunk)) {
          chunks[i] = bufferClone(chunk.buffer)
        } else {
          chunks[i] = textEncode(String(chunk))
        }
      }

      this._buffer = global.Uint8Array
        ? concatTypedarrays(chunks)
        : [].concat.apply([], chunks)
      this.size = this._buffer.length

      this.type = opts.type || ''
      if (/[^\u0020-\u007E]/.test(this.type)) {
        this.type = ''
      } else {
        this.type = this.type.toLowerCase()
      }
    }

    Blob.prototype.arrayBuffer = function () {
      return Promise.resolve(this._buffer)
    }

    Blob.prototype.text = function () {
      return Promise.resolve(textDecode(this._buffer))
    }

    Blob.prototype.slice = function (start, end, type) {
      var slice = this._buffer.slice(start || 0, end || this._buffer.length)
      return new Blob([slice], {type: type})
    }

    Blob.prototype.toString = function () {
      return '[object Blob]'
    }

    /********************************************************/
    /*                   File constructor                   */
    /********************************************************/
    function File (chunks, name, opts) {
      opts = opts || {}
      var a = Blob.call(this, chunks, opts) || this
      a.name = name.replace(/\//g, ':')
      a.lastModifiedDate = opts.lastModified ? new Date(opts.lastModified) : new Date()
      a.lastModified = +a.lastModifiedDate

      return a
    }

    File.prototype = create(Blob.prototype)
    File.prototype.constructor = File

    if (Object.setPrototypeOf) {
      Object.setPrototypeOf(File, Blob)
    } else {
      try { File.__proto__ = Blob } catch (e) {}
    }

    File.prototype.toString = function () {
      return '[object File]'
    }

    /********************************************************/
    /*                FileReader constructor                */
    /********************************************************/
    function FileReader () {
    	if (!(this instanceof FileReader)) {
        throw new TypeError("Failed to construct 'FileReader': Please use the 'new' operator, this DOM object constructor cannot be called as a function.")
      }

    	var delegate = document.createDocumentFragment()
    	this.addEventListener = delegate.addEventListener
    	this.dispatchEvent = function (evt) {
    		var local = this['on' + evt.type]
    		if (typeof local === 'function') local(evt)
    		delegate.dispatchEvent(evt)
    	}
    	this.removeEventListener = delegate.removeEventListener
    }

    function _read (fr, blob, kind) {
    	if (!(blob instanceof Blob)) {
        throw new TypeError("Failed to execute '" + kind + "' on 'FileReader': parameter 1 is not of type 'Blob'.")
      }

    	fr.result = ''

    	setTimeout(function () {
    		this.readyState = FileReader.LOADING
    		fr.dispatchEvent(new Event('load'))
    		fr.dispatchEvent(new Event('loadend'))
    	})
    }

    FileReader.EMPTY = 0
    FileReader.LOADING = 1
    FileReader.DONE = 2
    FileReader.prototype.error = null
    FileReader.prototype.onabort = null
    FileReader.prototype.onerror = null
    FileReader.prototype.onload = null
    FileReader.prototype.onloadend = null
    FileReader.prototype.onloadstart = null
    FileReader.prototype.onprogress = null

    FileReader.prototype.readAsDataURL = function (blob) {
    	_read(this, blob, 'readAsDataURL')
    	this.result = 'data:' + blob.type + ';base64,' + array2base64(blob._buffer)
    }

    FileReader.prototype.readAsText = function (blob) {
    	_read(this, blob, 'readAsText')
    	this.result = textDecode(blob._buffer)
    }

    FileReader.prototype.readAsArrayBuffer = function (blob) {
      _read(this, blob, 'readAsText')
       // return ArrayBuffer when possible
      this.result = (blob._buffer.buffer || blob._buffer).slice()
    }

    FileReader.prototype.abort = function () {}

    /********************************************************/
    /*                         URL                          */
    /********************************************************/
    URL.createObjectURL = function (blob) {
      return blob instanceof Blob
        ? 'data:' + blob.type + ';base64,' + array2base64(blob._buffer)
        : createObjectURL.call(URL, blob)
    }

    URL.revokeObjectURL = function (url) {
      revokeObjectURL && revokeObjectURL.call(URL, url)
    }

    /********************************************************/
    /*                         XHR                          */
    /********************************************************/
    var _send = global.XMLHttpRequest && global.XMLHttpRequest.prototype.send
    if (_send) {
      XMLHttpRequest.prototype.send = function (data) {
        if (data instanceof Blob) {
          this.setRequestHeader('Content-Type', data.type)
          _send.call(this, textDecode(data._buffer))
        } else {
          _send.call(this, data)
        }
      }
    }

    global.FileReader = FileReader
    global.File = File
    global.Blob = Blob
  }

  function fixFileAndXHR () {
    var isIE = !!global.ActiveXObject || (
      '-ms-scroll-limit' in document.documentElement.style &&
      '-ms-ime-align' in document.documentElement.style
    )

    // Monkey patched
    // IE don't set Content-Type header on XHR whose body is a typed Blob
    // https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/6047383
    var _send = global.XMLHttpRequest && global.XMLHttpRequest.prototype.send
    if (isIE && _send) {
      XMLHttpRequest.prototype.send = function (data) {
        if (data instanceof Blob) {
          this.setRequestHeader('Content-Type', data.type)
          _send.call(this, data)
        } else {
          _send.call(this, data)
        }
      }
    }

    try {
      new File([], '')
    } catch (e) {
      try {
        var klass = new Function('class File extends Blob {' +
          'constructor(chunks, name, opts) {' +
            'opts = opts || {};' +
            'super(chunks, opts || {});' +
            'this.name = name.replace(/\//g, ":");' +
            'this.lastModifiedDate = opts.lastModified ? new Date(opts.lastModified) : new Date();' +
            'this.lastModified = +this.lastModifiedDate;' +
          '}};' +
          'return new File([], ""), File'
        )()
        global.File = klass
      } catch (e) {
        var klass = function (b, d, c) {
          var blob = new Blob(b, c)
          var t = c && void 0 !== c.lastModified ? new Date(c.lastModified) : new Date()

          blob.name = d.replace(/\//g, ':')
          blob.lastModifiedDate = t
          blob.lastModified = +t
          blob.toString = function () {
            return '[object File]'
          }

          if (strTag) {
            blob[strTag] = 'File'
          }

          return blob
        }
        global.File = klass
      }
    }
  }

  if (blobSupported) {
    fixFileAndXHR()
    global.Blob = blobSupportsArrayBufferView ? global.Blob : BlobConstructor
  } else if (blobBuilderSupported) {
    fixFileAndXHR()
    global.Blob = BlobBuilderConstructor
  } else {
    FakeBlobBuilder()
  }

  if (strTag) {
    File.prototype[strTag] = 'File'
    Blob.prototype[strTag] = 'Blob'
    FileReader.prototype[strTag] = 'FileReader'
  }

  var blob = global.Blob.prototype
  var stream

  function promisify(obj) {
    return new Promise(function(resolve, reject) {
      obj.onload =
      obj.onerror = function(evt) {
        obj.onload =
        obj.onerror = null

        evt.type === 'load'
          ? resolve(obj.result || obj)
          : reject(new Error('Failed to read the blob/file'))
      }
    })
  }


  try {
    new ReadableStream({ type: 'bytes' })
    stream = function stream() {
      var position = 0
      var blob = this

      return new ReadableStream({
        type: 'bytes',
        autoAllocateChunkSize: 524288,

        pull: function (controller) {
          var v = controller.byobRequest.view
          var chunk = blob.slice(position, position + v.byteLength)
          return chunk.arrayBuffer()
          .then(function (buffer) {
            var uint8array = new Uint8Array(buffer)
            var bytesRead = uint8array.byteLength

            position += bytesRead
            v.set(uint8array)
              controller.byobRequest.respond(bytesRead)

            if(position >= blob.size)
              controller.close()
          })
        }
      })
    }
  } catch (e) {
    try {
      new ReadableStream({})
      stream = function stream(blob){
        var position = 0
        var blob = this

        return new ReadableStream({
          pull: function (controller) {
            var chunk = blob.slice(position, position + 524288)

            return chunk.arrayBuffer().then(function (buffer) {
              position += buffer.byteLength
              var uint8array = new Uint8Array(buffer)
              controller.enqueue(uint8array)

              if (position == blob.size)
                controller.close()
            })
          }
        })
      }
    } catch (e) {
      try {
        new Response('').body.getReader().read()
        stream = function stream() {
          return (new Response(this)).body
        }
      } catch (e) {
        stream = function stream() {
          throw new Error('Include https://github.com/MattiasBuelens/web-streams-polyfill')
        }
      }
    }
  }


  if (!blob.arrayBuffer) {
    blob.arrayBuffer = function arrayBuffer() {
      var fr = new FileReader()
      fr.readAsArrayBuffer(this)
      return promisify(fr)
    }
  }

  if (!blob.text) {
    blob.text = function text() {
      var fr = new FileReader()
      fr.readAsText(this)
      return promisify(fr)
    }
  }

  if (!blob.stream) {
    blob.stream = stream
  }
})()

/*********************************************************************
MessagePortPolyfill and MessageChannelPolyfill
https://github.com/rocwind/message-port-polyfill
MIT license.
*********************************************************************/

MessagePortPolyfill = /** @class */ (function () {
function MessagePortPolyfill() {
this.onmessage = null;
this.onmessageerror = null;
this.otherPort = null;
this.onmessageListeners = [];
}
MessagePortPolyfill.prototype.dispatchEvent = function (event) {
if (this.onmessage) {
this.onmessage(event);
}
this.onmessageListeners.forEach(function (listener) { return listener(event);
});
return true;
};
MessagePortPolyfill.prototype.postMessage = function (message) {
if (!this.otherPort) {
return;
}
this.otherPort.dispatchEvent({ data: message });
};
MessagePortPolyfill.prototype.addEventListener = function (type, listener) {
if (type !== 'message') {
return;
}
if (typeof listener !== 'function' ||
this.onmessageListeners.indexOf(listener) !== -1) {
return;
}
this.onmessageListeners.push(listener);
};
MessagePortPolyfill.prototype.removeEventListener = function (type, listener) {
if (type !== 'message') {
return;
}
var index = this.onmessageListeners.indexOf(listener);
if (index === -1) {
return;
}
this.onmessageListeners.splice(index, 1);
};
MessagePortPolyfill.prototype.start = function () {
// do nothing at this moment
};
MessagePortPolyfill.prototype.close = function () {
// do nothing at this moment
};
return MessagePortPolyfill;
}());
MessageChannelPolyfill = /** @class */ (function () {
function MessageChannelPolyfill() {
this.port1 = new MessagePortPolyfill();
this.port2 = new MessagePortPolyfill();
this.port1.otherPort = this.port2;
this.port2.otherPort = this.port1;
}
return MessageChannelPolyfill;
}());

// end third party code.

// lock down, for security.
var flist = ["alert","alert3","alert4","dumptree","uptrace",
"eb$newLocation","eb$logElement",
"getElementsByTagName", "getElementsByClassName", "getElementsByName", "getElementById","nodeContains",
"eb$gebtn","eb$gebn","eb$gebcn","eb$gebid","eb$cont",
"dispatchEvent","addEventListener","removeEventListener","attachOn",
"attachEvent","detachEvent","eb$listen","eb$unlisten",
"NodeFilter","createNodeIterator","createTreeWalker",
"logtime","defport","setDefaultPort","camelCase","dataCamel","isabove",
"classList","classListAdd","classListRemove","classListReplace","classListToggle","classListContains",
"mrList","mrKids",
"URL", "File", "FileReader", "Blob",
"MessagePortPolyfill", "MessageChannelPolyfill",
];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(this, flist[i], {writable:false,configurable:false});

Object.defineProperty(URL, "createObjectURL", {writable:false,configurable:false});
Object.defineProperty(URL, "revokeObjectURL", {writable:false,configurable:false});
Object.defineProperty(Blob, "text", {writable:false,configurable:false});
Object.defineProperty(Blob, "stream", {writable:false,configurable:false});
Object.defineProperty(Blob, "arrayBuffer", {writable:false,configurable:false});
