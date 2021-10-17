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

Object.defineProperty(this, "self",{writable:false,configurable:false,value:this});

Object.defineProperty(this, "Object",{writable:false,configurable:false});
Object.defineProperty(Object, "prototype",{writable:false,configurable:false});
// URLSearchParams displaces toString, so we can't nail down
// Object.prototype.toString until that has run.
// Object.defineProperty(Object.prototype, "toString",{enumerable:false,writable:false,configurable:false});
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

alert = puts;
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

/*********************************************************************
Show the scripts, where they come from, type, length, whether deminimized.
This uses getElementsByTagname() so you see all the scripts,
not just those that were in the original html.
The list is left in $ss for convenient access.
my$win() is used to get the window of the running context, where you are,
as there are no scripts in the shared window, where this is compiled.
*********************************************************************/

function showscripts() {
var i, s, m;
var w = my$win(), d = my$doc();
var slist = d.getElementsByTagName("script");
for(i=0; i<slist.length; ++i) {
s = slist[i];
m = i + ": ";
if(s.type) m += s.type;
else m += "default";
m += " ";
if(s.src) {
var ss = s.src.toString();
if(ss.match(/^data:/)) ss = "data";
m += ss;
} else {
m += "inline";
}
if(typeof s.text === "string")
m += " length " + s.text.length;
else
m += " length ?";
if(s.expanded) m += " deminimized";
alert(m);
}
w.$ss = slist;
}

function showframes() {
var i, s, m;
var w = my$win(), d = my$doc();
var slist = d.getElementsByTagName("iframe");
for(i=0; i<slist.length; ++i) {
s = slist[i];
m = i + ": cx" + (s.eb$expf ? s.contentWindow.eb$ctx : "?") + " " + s.src;
// anything else worth printing here?
alert(m);
}
w.$ff = slist;
}

function searchscripts(t) {
var w = my$win();
if(!w.$ss) showscripts();
for(var i=0; i<w.$ss.length; ++i)
if(w.$ss[i].text && w.$ss[i].text.indexOf(t) >= 0) alert(i);
}

function snapshot() {
var w = my$win();
// wlf is native to support the snapshot functionality: write local file.
wlf('<base href="' + w.eb$base + '">\n', "from");
var jslocal = "";
var idx = 0;
if(!w.$ss) showscripts();
for(var i=0; i<w.$ss.length; ++i) {
var s = w.$ss[i];
if(typeof s.text === "string" &&
(s.src && s.src.length || s.expanded)) {
var ss = "inline";
if(s.src && s.src.length) ss = s.src.toString();
if(ss.match(/^data:/)) continue;
// assumes the search piece of the url is spurious and unreliable
ss = ss.replace(/\?.*/, "");
++idx;
wlf(s.text, "f" + idx + ".js");
jslocal += "f" + idx + ".js:" + ss + "\n";
}
}
idx = 0;
for(var i=0; i<w.cssSource.length; ++i) {
var s = w.cssSource[i];
if(typeof s.data === "string" && s.data.length &&
s.src && s.src.length) {
var ss = s.src.toString();
// assumes the search piece of the url is spurious and unreliable
ss = ss.replace(/\?.*/, "");
++idx;
wlf(s.data, "f" + idx + ".css");
jslocal += "f" + idx + ".css:" + ss + "\n";
}
}
wlf(jslocal, "jslocal");
alert(".   ub   ci+   /<head/r from   w base   qt");
}

// called internally when we are running a snapshot.
function eb$base$snapshot() {
var w = my$win(), d = my$doc();
d.URL = w.eb$base;
var u = new w.URL(w.eb$base);
// changing things behind the scenes, so as not to trigger redirection
var loc1 = w.location$2;
var loc2 = d.location$2;
loc1.href$val = loc2.href$val = u.href$val;
loc1.protocol$val = loc2.protocol$val = u.protocol$val;
loc1.hostname$val = loc2.hostname$val = u.hostname$val;
loc1.host$val = loc2.host$val = u.host$val;
loc1.port$val = loc2.port$val = u.port$val;
loc1.pathname$val = loc2.pathname$val = u.pathname$val;
loc1.search$val = loc2.search$val = u.search$val;
loc1.hash$val = loc2.hash$val = u.hash$val;
}

function set_location_hash(h) { h = '#'+h; var loc = my$win().location$2; loc.hash$val = h;
loc.href$val = loc.href$val.replace(/#.*/, "") + h; }

// run an expression in a loop.
function aloop(s$$, t$$, exp$$) {
if(Array.isArray(s$$)) {
aloop(0, s$$.length, t$$);
return;
}
if(typeof s$$ !== "number" || typeof t$$ !== "number" || typeof exp$$ !== "string") {
alert("aloop(array, expression) or aloop(start, end, expression)");
return;
}
exp$$ = "for(var i=" + s$$ +"; i<" + t$$ +"; ++i){" + exp$$ + "}";
my$win().eval(exp$$);
}

// document.head, document.body; shortcuts to head and body.
function getElement() {
  var e = this.lastChild;
if(!e) { alert3("missing html node"); return null; }
if(e.nodeName != "HTML") alert3("html node name " + e.nodeName);
return e
}

function getHead() {
 var e = this.documentElement;
if(!e) return null;
// In case somebody adds extra nodes under <html>, I search for head and body.
// But it should always be head, body.
for(var i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "HEAD") return e.childNodes[i];
alert3("missing head node"); return null;
}

function setHead(h) {
 var i, e = this.documentElement;
if(!e) return;
for(i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "HEAD") break;
if(i < e.childNodes.length) e.removeChild(e.childNodes[i]); else i=0;
if(h) {
if(h.nodeName != "HEAD") { alert3("head replaced with node " + h.nodeName); h.nodeName = "HEAD"; }
if(i == e.childNodes.length) e.appendChild(h);
else e.insertBefore(h, e.childNodes[i]);
}
}

function getBody() {
 var e = this.documentElement;
if(!e) return null;
for(var i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "BODY") return e.childNodes[i];
alert3("missing body node"); return null;
}

function setBody(b) {
 var i, e = this.documentElement;
if(!e) return;
for(i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "BODY") break;
if(i < e.childNodes.length) e.removeChild(e.childNodes[i]);
if(b) {
if(b.nodeName != "BODY") { alert3("body replaced with node " + b.nodeName); b.nodeName = "BODY"; }
if(i == e.childNodes.length) e.appendChild(b);
else e.insertBefore(b, e.childNodes[i]);
}
}

// implementation of getElementsByTagName, getElementsByName, and getElementsByClassName.
// The return is an array, and you might put weird things on Array.prototype,
// and then expect to use them, so let's return your Array.

function getElementsByTagName(s) {
if(!s) { // missing or null argument
alert3("getElementsByTagName(type " + typeof s + ")");
return new (my$win().Array);
}
s = s.toLowerCase();
return eb$gebtn(this, s);
}

function eb$gebtn(top, s) {
var a = new (my$win().Array);
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
return new (my$win().Array);
}
s = s.toLowerCase();
return eb$gebn(this, s);
}

function eb$gebn(top, s) {
var a = new (my$win().Array);
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
return new (my$win().Array);
}
s = s.toLowerCase() . replace (/^\s+/, '') . replace (/\s+$/, '');
if(s === "") return new (my$win().Array);
var sa = s.split(/\s+/);
return eb$gebcn(this, sa);
}

function eb$gebcn(top, sa) {
var a = new (my$win().Array);
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

/*********************************************************************
I'm going to call Fixup from appendChild, removeChild, setAttribute,
anything that changes something we might be observing.
If we are indeed observing, I call the callback function right away.
That's not how we're suppose to do it.
I am suppose to queue up the change records, then call the callback
function later, after this script is done, asynchronously, maybe on a timer.
I could combine a dozen "kids have changed" records into one, to say,
"hey, the kids have changed."
And an attribute change record etc.
So they are expecting an array of change records.
I send an array of length 1, 1 record, right now.
It's just easier.
Support functions mrKids and mrList are below.
*********************************************************************/

function mutFixup(b, isattr, y, z) {
var w = my$win();
var list = w.mutList;
// most of the time there are no observers, so loop over that first
// whence this function does nothing and doesn't slow things down too much.
for(var j = 0; j < list.length; ++j) {
var o = list[j]; // the observer
if(!o.active) continue;
var r; // mutation record
if(isattr) { // the easy case
if(o.attr && o.target == b) {
r = new w.MutationRecord;
r.type = "attributes";
r.attributeName = y;
r.target = b;
r.oldValue = z;
o.callback([r], o);
}
continue;
}
// ok a child of b has changed
if(o.kids && o.target == b) {
r = new w.MutationRecord;
mrKids(r, b, y, z);
o.callback([r], o);
continue;
}
if(!o.subtree) continue;
// climb up the tree
for(var t = b; t && t.nodeType == 1; t = t.parentNode) {
if(o.subtree && o.target == t) {
r = new w.MutationRecord;
mrKids(r, b, y, z);
o.callback([r], o);
break;
}
}
}
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

/*********************************************************************
If you append a documentFragment you're really appending all its kids.
This is called by the various appendChild routines.
Since we are appending many nodes, I'm not sure what to return.
*********************************************************************/

function appendFragment(p, frag) { var c; while(c = frag.firstChild) p.appendChild(c); return null; }
function insertFragment(p, frag, l) { var c; while(c = frag.firstChild) p.insertBefore(c, l); return null; }

/*********************************************************************
Here comes a bunch of stuff regarding the childNodes array,
holding the children under a given html node.
The functions eb$apch1 and eb$apch2 are native. They perform appendChild in js.
The first has no side effects, because the linkage was already performed
within edbrowse via html, and a linkage side effect would only confuse things.
The second, eb$apch2, has side effects, as js code calls appendChild
and those links have to pass back to edbrowse.
But, the wrapper function appendChild makes another check;
if the child is already linked into the tree, then we have to unlink it first,
before we put it somewhere else.
This is a call to removeChild, also native, which unlinks in js,
and passses the remove side effect back to edbrowse.
The same reasoning holds for insertBefore.
These functions also check for a hierarchy error using isabove(),
which throws an exception.
*********************************************************************/

function appendChild(c) {
if(!c) return null;
if(c.nodeType == 11) return appendFragment(this, c);
isabove(c, this);
if(c.parentNode) c.parentNode.removeChild(c);
var r = this.eb$apch2(c);
mutFixup(this, false, c, null);
return r;
}

function prependChild(c) {
var v;
isabove(c, this);
if(this.childNodes.length) v = this.insertBefore(c, this.childNodes[0]);
else v = this.appendChild(c);
return v;
}

function insertBefore(c, t) {
if(!c) return null;
if(!t) return this.appendChild(c);
isabove(c, this);
if(c.nodeType == 11) return insertFragment(this, c, t);
if(c.parentNode) c.parentNode.removeChild(c);
var r = this.eb$insbf(c, t);
mutFixup(this, false, r, null);
return r;
}

function replaceChild(newc, oldc) {
var lastentry;
var l = this.childNodes.length;
var nextinline;
for(var i=0; i<l; ++i) {
if(this.childNodes[i] != oldc)
continue;
if(i == l-1)
lastentry = true;
else {
lastentry = false;
nextinline = this.childNodes[i+1];
}
this.removeChild(oldc);
if(lastentry)
this.appendChild(newc);
else
this.insertBefore(newc, nextinline);
break;
}
}

function hasChildNodes() { return (this.childNodes.length > 0); }

function eb$getSibling (obj,direction) {
var pn = obj.parentNode;
if(!pn) return null;
var j, l;
l = pn.childNodes.length;
for (j=0; j<l; ++j)
if (pn.childNodes[j] == obj) break;
if (j == l) {
// child not found under parent, error
return null;
}
switch(direction) {
case "previous":
return (j > 0 ? pn.childNodes[j-1] : null);
case "next":
return (j < l-1 ? pn.childNodes[j+1] : null);
default:
// the function should always have been called with either 'previous' or 'next' specified
return null;
}
}

function eb$getElementSibling (obj,direction) {
var pn = obj.parentNode;
if(!pn) return null;
var j, l;
l = pn.childNodes.length;
for (j=0; j<l; ++j)
if (pn.childNodes[j] == obj) break;
if (j == l) {
// child not found under parent, error
return null;
}
switch(direction) {
case "previous":
for(--j; j>=0; --j)
if(pn.childNodes[j].nodeType == 1) return pn.childNodes[j];
return null;
case "next":
for(++j; j<l; ++j)
if(pn.childNodes[j].nodeType == 1) return pn.childNodes[j];
return null;
default:
// the function should always have been called with either 'previous' or 'next' specified
return null;
}
}

function insertAdjacentElement(pos, e) {
var n, p = this.parentNode;
if(!p || typeof pos != "string") return null;
pos = pos.toLowerCase();
switch(pos) {
case "beforebegin": return p.insertBefore(e, this);
case "afterend": n = this.nextSibling; return n ? p.insertBefore(e, n) : p.appendChild(e);
case "beforeend": return this.appendChild(e);
case "afterbegin": return this.prependChild(e);
return null;
}
}

function append() {
var d = my$doc();
var i, l = arguments.length;
for(i=0; i<l; ++i) {
var c = arguments[i];
if(typeof c == "string") c = d.createTextNode(c);
// should now be a valid node
if(c.nodeType > 0) this.appendChild(c);
}
}

function prepend() {
var d = my$doc();
var i, l = arguments.length;
for(i=l-1; i>=0; --i) {
var c = arguments[i];
if(typeof c == "string") c = d.createTextNode(c);
// should now be a valid node
if(c.nodeType > 0) this.prependChild(c);
}
}

function after() {
var d = my$doc();
var p = this.parentNode;
if(!p) return;
var i, l = arguments.length;
var n = this.nextSibling;
for(i=0; i<l; ++i) {
var c = arguments[i];
if(typeof c == "string") c = d.createTextNode(c);
// should now be a valid node
if(c.nodeType > 0)
n ? p.insertBefore(c,n) : p.appendChild(c);
}
}

function before() {
var d = my$doc();
var p = this.parentNode;
if(!p) return;
var i, l = arguments.length;
for(i=0; i<l; ++i) {
var c = arguments[i];
if(typeof c == "string") c = d.createTextNode(c);
// should now be a valid node
if(c.nodeType > 0) p.insertBefore(c, this);
}
}

function replaceWith() {
var d = my$doc();
var p = this.parentNode;
if(!p) return;
var i, l = arguments.length;
var n = this.nextSibling;
for(i=0; i<l; ++i) {
var c = arguments[i];
if(typeof c == "string") c = d.createTextNode(c);
// should now be a valid node
if(c.nodeType > 0)
n ? p.insertBefore(c,n) : p.appendChild(c);
}
p.removeChild(this);
}

/*********************************************************************
Yes, Form is weird.
If you add an input to a form, it adds under childNodes in the usual way,
but also must add in the elements[] array.
Same for insertBefore and removeChild.
When adding an input element to a form,
linnk form[element.name] to that element.
*********************************************************************/

function formname(parent, child) {
var s;
if(typeof child.name === "string")
s = child.name;
else if(typeof child.id === "string")
s = child.id;
else return;
if(!parent[s]) parent[s] = child;
if(!parent.elements[s]) parent.elements[s] = child;
}

function formAppendChild(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return mw$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.nodeName === "INPUT" || newobj.nodeName === "SELECT") {
this.elements.push(newobj);
newobj.form = this;
formname(this, newobj);
}
return newobj;
}

function formInsertBefore(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return mw$.insertFragment(this, newobj, item);
var r = this.insertBeforeNative(newobj, item);
if(!r) return null;
if(newobj.nodeName === "INPUT" || newobj.nodeName === "SELECT") {
for(var i=0; i<this.elements.length; ++i)
if(this.elements[i] == item) {
this.elements.splice(i, 0, newobj);
break;
}
newobj.form = this;
formname(this, newobj);
}
return newobj;
}

function formRemoveChild(item) {
if(!item) return null;
if(!this.removeChildNative(item))
return null;
if(item.nodeName === "INPUT" || item.nodeName === "SELECT") {
for(var i=0; i<this.elements.length; ++i)
if(this.elements[i] == item) {
this.elements.splice(i, 1);
break;
}
delete item.form;
if(item.name$2 && this[item.name$2] == item) delete this[item.name$2];
if(item.name$2 && this.elements[item.name$2] == item) delete this.elements[item.name$2];
}
return item;
}

// It's crude, but just reindex all the rows in a table
function rowReindex(t) {
// climb up to find Table
while(t.dom$class != "Table") {
if(t.dom$class == "Frame") return;
t = t.parentNode;
if(!t) return;
}

var i, j, n = 0;
var s; // section
t.rows.length = 0;
if(s = t.tHead) {
for(j=0; j<s.rows.length; ++j)
t.rows.push(s.rows[j]), s.rows[j].rowIndex = n++, s.rows[j].sectionRowIndex = j;
}
for(i=0; i<t.tBodies.length; ++i) {
s = t.tBodies[i];
for(j=0; j<s.rows.length; ++j)
t.rows.push(s.rows[j]), s.rows[j].rowIndex = n++, s.rows[j].sectionRowIndex = j;
}
if(s = t.tFoot) {
for(j=0; j<s.rows.length; ++j)
t.rows.push(s.rows[j]), s.rows[j].rowIndex = n++, s.rows[j].sectionRowIndex = j;
}

j = 0;
for(s=t.firstChild; s; s=s.nextSibling)
if(s.dom$class == "tRow")
t.rows.push(s), s.rowIndex = n++, s.sectionRowIndex = j;
}

// insert row into a table or body or head or foot
function insertRow(idx) {
if(idx === undefined) idx = -1;
if(typeof idx !== "number") return null;
var t = this;
var nrows = t.rows.length;
if(idx < 0) idx = nrows;
if(idx > nrows) return null;
// Should this be ownerDocument, the context that crated the table,
// or my$doc(), the running context. I think the latter is safer.
var r = my$doc().createElement("tr");
if(t.dom$class != "Table") {
if(idx == nrows) t.appendChild(r);
else t.insertBefore(r, t.rows[idx]);
} else {
// put this row in the same section as the next row
if(idx == nrows) {
if(nrows) t.rows[nrows-1].parentNode.appendChild(r);
else if(t.tHead) t.tHead.appendChild(r);
else if(t.tBodies.length) t.tBodies[0].appendChild(r);
else if(t.tFoot) t.tFoot.appendChild(r);
// No sections, what now? acid test 51 suggests if should not go into the table.
} else {
t.rows[idx].parentNode.insertBefore(r, t.rows[idx]);
}
}
return r;
}

function deleteRow(r) {
if(r.dom$class != "tRow") return;
this.removeChild(r);
}

function insertCell(idx) {
if(idx === undefined) idx = -1;
if(typeof idx !== "number") return null;
var t = this;
var n = t.childNodes.length;
if(idx < 0) idx = n;
if(idx > n) return null;
var r = my$doc().createElement("td");
if(idx == n)
t.appendChild(r);
else
t.insertBefore(r, t.childNodes[idx]);
return r;
}

function deleteCell(r) {
if(r.dom$class != "Cell") return;
this.removeChild(r);
}

/*********************************************************************
This is a workaround, when setAttribute is doing something it shouldn't,
like form.setAttribute("elements") or some such.
I call these implicit members, we shouldn't mess with them.
*********************************************************************/

function implicitMember(o, name) {
return name === "elements" && o.dom$class == "Form" ||
name === "rows" && (o.dom$class == "Table" || o.dom$class == "tBody" || o.dom$class == "tHead" || o.dom$class == "tFoot") ||
name === "tBodies" && o.dom$class == "Table" ||
(name === "cells" || name === "rowIndex" || name === "sectionRowIndex") && o.dom$class == "tRow" ||
name === "className" ||
// no clue what getAttribute("style") is suppose to do
name === "style" ||
name === "htmlFor" && o.dom$class == "Label" ||
name === "options" && o.dom$class == "Select";
}

/*********************************************************************
Set and clear attributes. This is done in 3 different ways,
the third using attributes as a NamedNodeMap.
This may be overkill - I don't know.
*********************************************************************/

function getAttribute(name) {
var w = my$win();
name = name.toLowerCase();
if(implicitMember(this, name)) return null;
// has to be a real attribute
if(!this.attributes$2) return null;
if(!this.attributes[name]) return null;
var v = this.attributes[name].value;
if(v.dom$class == "URL" || v instanceof w.URL) return v.toString();
var t = typeof v;
if(t == "undefined") return null;
// possibly any object should run through toString(), as we did with URL, idk
return v; }
function hasAttribute(name) { return this.getAttribute(name) !== null; }

function getAttributeNames(name) {
var w = my$win();
var a = new w.Array;
if(!this.attributes$2) return a;
for(var l = 0; l < this.attributes$2.length; ++l)
a.push(this.attributes$2[l].name);
return a;
}

function getAttributeNS(space, name) {
if(space && !name.match(/:/)) name = space + ":" + name;
return this.getAttribute(name);
}
function hasAttributeNS(space, name) { return this.getAttributeNS(space, name) !== null;}

function setAttribute(name, v) {
var w = my$win();
name = name.toLowerCase();
// special code for style
if(name == "style" && this.style.dom$class == "CSSStyleDeclaration") {
this.style.cssText = v;
return;
}
if(implicitMember(this, name)) return;
var oldv = null;
// referencing attributes should create it on demand, but if it doesn't...
if(!this.attributes) this.attributes$2 = new w.NamedNodeMap;
if(!this.attributes[name]) {
var a = new w.Attr();
a.owner = this;
a.name = name;
a.specified = true;
// don't have to set value because there is a getter that grabs value
// from the html node, see Attr class.
this.attributes.push(a);
// easy hash access
this.attributes[name] = a;
} else {
oldv = this.attributes[name].value;
}
if(v !== "from@@html") {
if(name.substr(0,5) == "data-") {
// referencing dataset should create it on demand, but if it doesn't...
if(!this.dataset) this.dataset$2 = {};
this.dataset[dataCamel(name)] = v;
} else this[name] = v;
}
mutFixup(this, true, name, oldv);
}
function markAttribute(name) { this.setAttribute(name, "from@@html"); }
function setAttributeNS(space, name, v) {
if(space && !name.match(/:/)) name = space + ":" + name;
this.setAttribute(name, v);
}

function removeAttribute(name) {
if(!this.attributes$2) return;
    name = name.toLowerCase();
// special code for style
if(name == "style" && this.style.dom$class == "CSSStyleDeclaration") {
// wow I have no clue what this means but it happens, https://www.maersk.com
return;
}
var oldv = null;
if(name.substr(0,5) == "data-") {
var n = dataCamel(name);
if(this.dataset$2 && this.dataset$2[n]) { oldv = this.dataset$2[n]; delete this.dataset$2[n]; }
} else {
    if (this[name]) { oldv = this[name]; delete this[name]; }
}
// acid test 59 says there's some weirdness regarding button.type
if(name === "type" && this.nodeName == "BUTTON") this[name] = "submit";
// acid test 48 removes class before we can check its visibility.
// class is undefined and last$class is undefined, so getComputedStyle is never called.
if(name === "class" && !this.last$class) this.last$class = "@@";
if(name === "id" && !this.last$id) this.last$id = "@@";
var a = this.attributes[name]; // hash access
if(!a) return;
// Have to roll our own splice.
var i, found = false;
for(i=0; i<this.attributes.length-1; ++i) {
if(!found && this.attributes[i] == a) found = true;
if(found) this.attributes[i] = this.attributes[i+1];
}
this.attributes.length = i;
delete this.attributes[i];
delete this.attributes[name];
mutFixup(this, true, name, oldv);
}
function removeAttributeNS(space, name) {
if(space && !name.match(/:/)) name = space + ":" + name;
this.removeAttribute(name);
}

// this returns null if no such attribute, is that right,
// or should we return a new Attr node with no value?
function getAttributeNode(name) {
if(!this.attributes$2) return null;
    name = name.toLowerCase();
return this.attributes[name] ? this.attributes[name] : null;
}

/*********************************************************************
cloneNode creates a copy of the node and its children recursively.
The argument 'deep' refers to whether or not the clone will recurs.
clone1 is a helper function that is not tied to any particular prototype.
It's frickin complicated, so set cloneDebug to debug it.
*********************************************************************/

function clone1(node1,deep) {
var node2;
var i, j;
var kids = null;
var debug = db$flags(2);
var w = my$win();
var d = my$doc();

// WARNING: don't use instanceof Array here.
// Array is a different class in another frame.
if(Array.isArray(node1.childNodes))
kids = node1.childNodes;

// We should always be cloning a node.
if(debug) alert3("clone " + node1.nodeName + " {");
if(debug) {
if(kids) alert3("kids " + kids.length);
else alert3("no kids, type " + typeof node1.childNodes);
}

if(node1.nodeName == "#text")
node2 = d.createTextNode();
else if(node1.nodeName == "#comment")
node2 = d.createComment();
else if(node1.nodeName == "#document-fragment")
node2 = d.createDocumentFragment();
else if(node1.dom$class == "CSSStyleDeclaration")
node2 = d.createElement("style");
else
node2 = d.createElement(node1.nodeName);
if(node1 == w.cloneRoot1) w.cloneRoot2 = node2;

if (deep && kids) {
for(i = 0; i < kids.length; ++i) {
var current_item = kids[i];
node2.appendChild(clone1(current_item,true));
}
}

var lostElements = false;

// now for strings and functions and such.
for (var item in node1) {
// don't copy the things that come from prototype
if(!node1.hasOwnProperty(item)) continue;

// children already handled
if(item === "childNodes" || item === "parentNode") continue;

if(implicitMember(node1, item)) continue;

if (typeof node1[item] === 'function') {
// event handlers shouldn't carry across.
if(item.match(/^on[a-zA-Z]+(\$\$fn|\$2|)$/)) continue;
if(debug) alert3("copy function " + item);
node2[item] = node1[item];
continue;
}

if(node1[item] === node1) {
if(debug) alert3("selflink through " + item);
node2[item] = node2;
continue;
}

// various kinds of arrays
if(Array.isArray(node1[item])) {

// event handlers shouldn't carry across.
if(item.match(/^on[a-zA-Z]+\$\$array$/)) continue;

/*********************************************************************
Ok we need some special code here for form.elements,
an array of input nodes within the form.
We are preserving links, rather like tar or cpio.
The same must be done for an array of rows beneath <table>,
or an array of cells in a row, and perhaps others.
But the thing is, we don't have to do that, because appendChild
does it for us, as side effects, for these various classes.
*********************************************************************/

node2[item] = new w.Array;

// special code here for an array of radio buttons within a form.
if(node1.dom$class == "Form" && node1[item].length &&
node1[item][0].dom$class == "Element" && node1[item][0].name == item) {
var a1 = node1[item];
var a2 = node2[item];
if(debug) alert3("linking form.radio " + item + " with " + a1.length + " buttons");
a2.type = a1.type;
a2.nodeName = a1.nodeName;
a2.class = a1.class;
a2.last$class = a1.last$class;
for(i = 0; i < a1.length; ++i) {
var p = findObject(a1[i]);
if(p.length) {
a2.push(correspondingObject(p));
} else {
a2.push(null);
if(debug) alert3("oops, button " + i + " not linked");
}
}
continue;
}

// It's a regular array.
if(debug) alert3("copy array " + item + " with " + node1[item].length + " members");
for(i = 0; i < node1[item].length; ++i)
node2[item].push(node1[item][i]);
continue;
}

if(typeof node1[item] === "object") {
// An object, but not an array.

// skip the on-demand background objects
if(item === "style$2") continue;
if(item === "attributes$2") continue;
if(item === "dataset$2") continue;
if(item === "ownerDocument") continue; // handled by createElement
if(item === "validity") continue; // created by constructor

if(node1[item] === null) { node2[item] = null; continue; }

// Check for URL objects.
if(node1[item].dom$class == "URL") {
var u = node1[item];
if(debug) alert3("copy URL " + item);
node2[item] = new w.z$URL(u.href);
continue;
}

// some sites displace my URL with theirs
if(node1[item] instanceof w.URL) {
var u = node1[item];
if(debug) alert3("copy URL " + item);
node2[item] = new w.URL(u.toString());
continue;
}

// Look for a link from A to B within the tree of nodes,
// A.foo = B, and try to preserve that link in the new tree, A1.foo = B1,
// rather like tar or cpio preserving hard links.
var p = findObject(node1[item]);
if(p.length) {
if(debug) alert3("link " + item + " " + p);
node2[item] = correspondingObject(p);
} else {
// I don't think we should point to a generic object that we don't know anything about.
if(debug) alert3("unknown object " + item);
}
continue;
}

if (typeof node1[item] === 'string') {
// don't copy strings that are really setters; we'll be copying inner$html
// as a true string so won't need to copy innerHTML, and shouldn't.
if(item == "innerHTML")
continue;
if(item == "innerText")
continue;
if(item == "value" &&
!Array.isArray(node1) && !(node1.dom$class == "Option"))
continue;
if(debug) {
var showstring = node1[item];
if(showstring.length > 140) showstring = "long";
alert3("copy string " + item + " = " + showstring);
}
node2[item] = node1[item];
continue;
}

if (typeof node1[item] === 'number') {
if(item == "eb$seqno" || item == "eb$gsn") continue;
if(debug) alert3("copy number " + item + " = " + node1[item]);
node2[item] = node1[item];
continue;
}

if (typeof node1[item] === 'boolean') {
if(debug) alert3("copy boolean " + item + " = " + node1[item]);
node2[item] = node1[item];
continue;
}
}

// copy style object if present and its subordinate strings.
if (node1.style$2 && node1.style$2.dom$class == "CSSStyleDeclaration") {
if(debug) alert3("copy style");
node2.style$2 = new w.CSSStyleDeclaration;
node2.style$2.element = node2;
for (var l in node1.style$2){
if(!node1.style$2.hasOwnProperty(l)) continue;
if (typeof node1.style$2[l] === 'string' ||
typeof node1.style$2[l] === 'number') {
if(debug) alert3("copy stattr " + l);
node2.style$2[l] = node1.style$2[l];
}
}
}

if (node1.attributes$2) { // has attributes
if(debug) alert3("copy attributes");
for(var l=0; l<node1.attributes.length; ++l) {
if(debug) alert3("copy attribute " + node1.attributes[l].name);
node2.setAttribute(node1.attributes[l].name, node1.attributes[l].value);
}
}

// This is an ugly patch for radio button arrays that don't get linked into the elements array.
if(lostElements) {
var e1 = node1.elements;
var e2 = node2.elements;
if(debug) alert3("looking for lost radio elements");
for(i=0; i<e2.length; ++i) {
if(e2[i]) continue;
if(e1[i].nodeName !== "RADIO") {
if(debug) alert3("oops, lost element " + i + " is type " + e1[i].nodeName);
continue;
}
for (var item in node1) {
if(!node1.hasOwnProperty(item)) continue;
if(node1[item] !== e1[i]) continue;
e2[i] = node2[item];
if(debug) alert3("patching element " + i + " through to " + item);
break;
}
}
}

if(debug) alert3("}");
return node2;
}

// Find an object in a tree of nodes being cloned.
// Return a sequence of numbers, for children, from the root.
function findObject(t) {
var w = my$win();
var p = "";
while(t != w.cloneRoot1) {
var up = t.parentNode;
if(!up || up.nodeType == 9 || !up.childNodes) return "";
var i;
for(i=0; i<up.childNodes.length; ++i)
if(up.childNodes[i] == t) break;
if(i == up.childNodes.length) return "";
p = "," + i + p;
t = up;
}
return p + ',';
}

// The inverse of the above.
function correspondingObject(p) {
var w = my$win();
var c = w.cloneRoot2;
p = p.substr(1);
while(p) {
var j = p.replace(/,.*/, "");
if(!c.childNodes || j >= c.childNodes.length) return "";
c = c.childNodes[j];
p = p.replace(/^\d+,/, "");
}
return c;
}

// symbolic constants for compareDocumentPosition
Object.defineProperty(this,"DOCUMENT_POSITION_DISCONNECTED",{writable:false,configurable:false,value:1});
Object.defineProperty(this,"DOCUMENT_POSITION_PRECEDING",{writable:false,configurable:false,value:2});
Object.defineProperty(this,"DOCUMENT_POSITION_FOLLOWING",{writable:false,configurable:false,value:4});
Object.defineProperty(this,"DOCUMENT_POSITION_CONTAINS",{writable:false,configurable:false,value:8});
Object.defineProperty(this,"DOCUMENT_POSITION_CONTAINED_BY",{writable:false,configurable:false,value:16});

/*********************************************************************
compareDocumentPosition:
The documentation I found was entirely unclear as to the meaning
of preceding and following.
Does A precede B if it appears first in a depth first search of the tree,
or if it appears first wherein they have the same parent,
or if they are siblings?
I have no clue, so I'm going for the latter, partly because it's easy.
That means the relationships are disjoint.
A can't contain B and precede B simultaneously.
So I don't know why they say these are bits in a bitmask.
Also not clear if "contains" can descend into a subframe. I don't check for this.
*********************************************************************/

compareDocumentPosition = function(w) {
if(this === w) return DOCUMENT_POSITION_DISCONNECTED;
if(this.parentNode === w.parentNode) {
if(this.nextSibling === w) return DOCUMENT_POSITION_FOLLOWING;
if(this.previousSibling === w) return DOCUMENT_POSITION_PRECEDING;
return DOCUMENT_POSITION_DISCONNECTED;
}
var t = this;
while(t.parentNode) {
t = t.parentNode;
if(t === w) return DOCUMENT_POSITION_CONTAINED_BY;
}
var t = w;
while(t.parentNode) {
t = t.parentNode;
if(t === this) return DOCUMENT_POSITION_CONTAINS;
}
return DOCUMENT_POSITION_DISCONNECTED;
}

function cssGather(pageload, newwin) {
var w = my$win();
if(!pageload && newwin && newwin.eb$visible) w = newwin;
var d =w.document;
var css_all = "";
w.cssSource = [];
var a, i, t;

a = d.querySelectorAll("link,style");
for(i=0; i<a.length; ++i) {
t = a[i];
if(t.dom$class == "Link") {
if(t.css$data && (
t.type && t.type.toLowerCase() == "text/css" ||
t.rel && t.rel.toLowerCase() == "stylesheet")) {
w.cssSource.push({data: t.css$data, src:t.href});
css_all += "@ebdelim0" + t.href + "{}\n";
css_all += t.css$data;
}
}
if(t.dom$class == "CSSStyleDeclaration") {
if(t.css$data) {
w.cssSource.push({data: t.css$data, src:w.eb$base});
css_all += "@ebdelim0" + w.eb$base + "{}\n";
css_all += t.css$data;
}
}
}

// If the css didn't change, then no need to rebuild the selectors
if(!pageload && css_all == w.last$css_all)
return;

w.last$css_all = css_all;
w.css$ver++;
cssDocLoad(w.eb$ctx, css_all, pageload);
}

	// disregarding pseudoelements for now
function getComputedStyle(e,pe) {
var s, w = my$win();

/*********************************************************************
Some sites call getComputedStyle on the same node over and over again.
http://songmeanings.com/songs/view/3530822107858535238/
Can we remember the previous call and just return the same style object?
Can we know that nothing has changed in between the two calls?
I can track when the tree changes, and even the class,
but what about individual attributes?
I haven't found a way to do this without breaking acid test 33 and others.
We're not sharing DOM classes yet, so hark back to the calling window
to create the Style element.
*********************************************************************/

s = new w.CSSStyleDeclaration;
s.element = e;

/*********************************************************************
What if js has added or removed style objects from the tree?
Maybe the selectors and rules are different from when they were first compiled.
Does this ever happen? It does in acid test 33.
Does it ever happen in the real world? I don't know.
If not, this is a big waste of time and resources.
How big? Well not too bad I guess.
Strings are parsed in C, which is pretty fast,
but it really falls flat when the css has @import which pulls in another
css file, and now we have to fetch that file on every call to getComputedStyle.
Nodes are created, and technically their class changed,
in that there was no node and no class before, and that induces a call
to getComputedStyle, and that fetches the file, again.
The imported css file could be fetched 100 times just to load the page.
I get around this by the shortcache feature in css.c.
If the css has changed in any way, I recompile the descriptors
and increment the css version, stored in css$ver;
Any information we might have saved about nodes and descriptors,
for speed and optimization, is lost if the version changes.
Remember that "this" is the window object.
*********************************************************************/

cssGather(false, this);

this.soj$ = s;
cssApply(this.eb$ctx, e);
delete this.soj$;

/*********************************************************************
Now for the confusion.
https://developer.mozilla.org/en-US/docs/Web/API/Window/getComputedStyle
Very clearly states s is the result of css pages and <style> tags,
and not javascript assigned values.

  The returned object is the same {CSSStyleDeclaration} type as the object
  returned from the element's {style} property.
  However, the two objects have different purposes:
  * The object from getComputedStyle is read-only,
  and should be used to inspect the element's style — including those set by a
  <style> element or an external stylesheet.
  * The element.style object should be used to set styles on that element,
  or inspect styles directly added to it from JavaScript manipulation or the
  global style attribute.

See - if js sets a style attribute directly it is not suppose to carry
across to the new style object.
But in stark contradiction to this paragraph,
browsers carry the style attributes across no matter how they were set.
Huh???
Well we have to do the same so here we go.
*********************************************************************/

if(e.style$2) {
for(var k in e.style) {
if(!e.style.hasOwnProperty(k)) continue;
if(k.match(/\$(\$scy|pri)$/)) continue;
if(typeof e.style[k] == 'object') continue;

/*********************************************************************
This should be a real attribute now.
If it was set by the css system, and is no longer,
maybe we shouldn't carry it across.
Acid test: see how the slash comes back to light after class hidden is removed.
<span id="slash" class="hidden">/</span>
Specificity indicates it comes from css, except for 100000,
which is style.cssText = "color:green", and that should carry across.
*********************************************************************/

if(!s[k] &&  e.style[k+"$$scy"] < 100000) continue;

// Ok carry this one across.
s[k] = e.style[k];
}
}

return s;
}

// A different version, run when the class or id changes.
// It writes the changes back to the style node, does not create a new one.
function computeStyleInline(e) {
var s, w = my$win();
var created = false;

e.last$class = e.class, e.last$id = e.id;

// don't put a style under a style.
// There are probably other nodes I should skip too.
if(e.dom$class == "CSSStyleDeclaration") return;
if(e.nodeType != 1 && e.nodeType != 3) return;

if(s = e.style$2) {
// Unlike the above, we remove previous values that were set by css,
// because css is being reapplied.
for(var k in s) {
if(!s.hasOwnProperty(k)) continue;
if(!k.match(/\$(\$scy|pri)$/)) continue;
if(k.match(/\$\$scy$/) && s[k] == 100000) continue;
// this one goes away
delete s[k];
delete s[k.replace(/\$(\$scy|pri)$/, "")];
}
} else {
// create a style object, but if it comes up empty, we'll remove it again.
s = new w.CSSStyleDeclaration;
created = true;
}

// This is called on a (possibly large) subtree of nodes,
// so please verify the css style sheets before hand.
// cssGather(false, this);

// apply all the css rules
w.soj$ = s;
cssApply(w.eb$ctx, e);
delete w.soj$;
// style has been recomputed
if(created) {
// is there anything there?
for(var k in s) {
if(!s.hasOwnProperty(k)) continue;
if(k == "element" || k == "ownerDocument")
continue;
e.style$2 = s;
s.element = e;
break;
}
}

// descend into the children
if(e.childNodes)
for(var i=0; i<e.childNodes.length; ++i)
computeStyleInline(e.childNodes[i]);
}

function cssTextGet() {
var s = "";
for(var k in this) {
if(!k.match(/\$(\$scy|pri)$/)) continue;
k=k.replace(/\$(\$scy|pri)$/, "");
var l = this[k];
if(l.match(/[ \t;"'{}]/)) {
if(l.match(/"/)) l = "'" + l + "'";
else l = '"' + l + '"';
}
s=s+ k + ':' + l + '; ';
}
return s;
}

function injectSetup(which) {
var w = my$win();
var d = my$doc();
var z = this;
switch(which) {
case 'a':
if(!this.inj$after) {
z = this.appendChild(d.createTextNode());
z.inj$css = true;
this.inj$after = true;
} else z = this.lastChild;
break;
case 'b':
if(!this.inj$before) {
z = this.prependChild(d.createTextNode());
z.inj$css = true;
this.inj$before = true;
} else z = this.firstChild;
break;
}
w.soj$ = z.style;
}

/*********************************************************************
This function doesn't do all it should, and I'm not even sure what it should do.
If class changes from x to y, it throws out the old css derived attributes
and rebuilds the style using computeStyleInline().
Rules with .x don't apply any more; rules with .y now apply.
If prior javascript had specifically set style.foo = "bar",
if will persist if foo was not derived from css;
but it will go away and be recomputed if foo came from css.
Maybe that's the right thing to do, maybe not, I don't know.
In theory, changing class could effect the style of any node anywhere in the tree.
In fact, setting any attribute in one node could change the style of any node
anywhere in the tree.
I don't recompute the styles for every node in the entire tree
every time you set an attribute in a node;
it would be tremendously slow!
I only watch for changes to class or id,
and when that happens I recompute styles for that node and the subtree below.
That is my compromise.
Finally, any hover effects from .y are not considered, just as they are not
considered in getComputedStyle().
And any hover effects from .x are lost.
Injected text, as in .x:before { content:hello } remains.
I don't know if that's right either.
*********************************************************************/

function eb$visible(t) {
// see the DIS_ values in eb.h
var c, rc = 0;
var so; // style object
if(!t) return 0;
if(t.hidden || t["aria-hidden"]) return 1;
// If class has changed, recompute style.
// If id has changed, recompute style, but I don't think that ever happens.
if(t.class != t.last$class || t.id != t.last$id) {
var w = my$win();
if(t.last$class) alert3("restyle " + t.nodeName + "." + t.last$class + "." + t.class+"#"+t.last$id+"#"+t.id);
else alert4("restyle " + t.nodeName + "." + t.last$class + "." + t.class+"#"+t.last$id+"#"+t.id);
if(w.rr$start) {
cssGather(false, w);
delete w.rr$start;
}
computeStyleInline(t);
}
if(!(so = t.style$2)) return 0;
if(so.display == "none" || so.visibility == "hidden") {
rc = 1;
// It is hidden, does it come to light on hover?
if(so.hov$vis) rc = 2;
return rc;
}
if((c = so.color) && c != "inherit") {
rc = (c == "transparent" ? 4 : 3);
if(rc == 4 && so.hov$col) rc = 5;
}
return rc;
}

function insertAdjacentHTML(flavor, h) {
// easiest implementation is just to use the power of innerHTML
var d = my$doc();
var p = d.createElement("p");
p.innerHTML = h; // the magic
var s, parent = this.parentNode;
switch(flavor) {
case "beforebegin":
while(s = p.firstChild)
parent.insertBefore(s, this);
break;
case "afterbegin":
while(s = p.lastChild)
this.insertBefore(s, this.firstChild);
break;
case "beforeend":
while(s = p.firstChild)
this.appendChild(s);
break;
case "afterend":
while(s = p.lastChild)
parent.insertBefore(s, this.nextSibling);
break;
}
}

function htmlString(t) {
if(t.nodeType == 3) return t.data;
if(t.nodeType != 1) return "";
var s = "<" + (t.nodeName ? t.nodeName : "x");
if(t.class) s += ' class="' + t.class + '"';
if(t.id) s += ' id="' + t.id + '"';
s += '>';
if(t.childNodes)
for(var i=0; i<t.childNodes.length; ++i)
s += htmlString(t.childNodes[i]);
s += "</";
s += (t.nodeName ? t.nodeName : "x");
s += '>';
return s;
}

function outer$1(t, h) {
var p = t.parentNode;
if(!p) return;
t.innerHTML = h;
while(t.lastChild) p.insertBefore(t.lastChild, t.nextSibling);
p.removeChild(t);
}

// There are subtle differences between contentText and textContent, which I don't grok.
function textUnder(top, flavor) {
var t = top.getElementsByTagName("#text");
var answer = "", part;
for(var i=0; i<t.length; ++i) {
var u = t[i];
if(u.parentNode && u.parentNode.nodeName == "OPTION") continue;
// any other texts we should skip?
part = u.data.trim();
if(!part) continue;
if(answer) answer += '\n';
answer += part;
}
return answer;
}

function newTextUnder(top, s, flavor) {
var l = top.childNodes.length;
for(var i=l-1; i>=0; --i)
top.removeChild(top.childNodes[i]);
top.appendChild(my$doc().createTextNode(s));
}

function clickfn() {
var w = my$win();
var nn = this.nodeName, t = this.type;
// as though the user had clicked on this
if(nn == "button" || (nn == "INPUT" &&
(t == "button" || t == "reset" || t == "submit" || t == "checkbox" || t == "radio"))) {
var e = new w.Event;
e.initEvent("click", true, true);
if(!this.dispatchEvent(e)) return;
// do what the tag says to do
if(this.form) {
if(t == "submit") {
e.initEvent("submit", true, true);
if(this.dispatchEvent(e))
this.form.submit();
}
if(t == "reset") {
e.initEvent("reset", true, true);
if(this.dispatchEvent(e))
this.form.reset();
}
}
if(t != "checkbox" && t != "radio") return;
this.checked$2 = (this.checked$2 ? false : true);
// if it's radio and checked we need to uncheck the others.
if(this.form && this.checked$2 && t == "radio" &&
(nn = this.name) && (e = this.form[nn]) && Array.isArray(e)) {
for(var i=0; i<e.length; ++i)
if(e[i] != this) e[i].checked$2 = false;
} else // try it another way
if(this.checked$2 && t == "radio" && this.parentNode && (e = this.parentNode.childNodes) && (nn = this.name)) {
for(var i=0; i<e.length; ++i)
if(e[i].nodeName == "INPUT" && e[i].type == t && e[i].name == nn &&e[i] != this) e[i].checked$2 = false;
}
}
}

function checkset(n) {
if(typeof n !== "boolean") n = false;
this.checked$2 = n;
var nn = this.nodeName, t = this.type, e;
// if it's radio and checked we need to uncheck the others.
if(this.form && this.checked$2 && t == "radio" &&
(nn = this.name) && (e = this.form[nn]) && Array.isArray(e)) {
for(var i=0; i<e.length; ++i)
if(e[i] != this) e[i].checked$2 = false;
} else // try it another way
if(this.checked$2 && t == "radio" && this.parentNode && (e = this.parentNode.childNodes) && (nn = this.name)) {
for(var i=0; i<e.length; ++i)
if(e[i].nodeName == "INPUT" && e[i].type == t && e[i].name == nn &&e[i] != this) e[i].checked$2 = false;
}
}

// jtfn0 injects trace(blah) into the code.
// It should only be applied to deminimized code.
// jtfn1 puts a name on the anonymous function, for debugging.

jtfn0 = function (all, a, b) {
// if code is not deminimized, this will inject
// trace on every blank line, which is not good.
if(b == "\n" && a.match(/\n/)) return a+b;
// I don't want to match on function(){var either.
if(b != "\n" && !a.match(/\n/)) return a+b;
var w = my$win();
var c = w.$jt$c;
var sn = w.$jt$sn;
w.$jt$sn = ++sn;
return a + "trace" + "@(" + c + sn + ")" + b;
}

jtfn1 = function (all, a, b) {
var w = my$win();
var c = w.$jt$c;
var sn = w.$jt$sn;
w.$jt$sn = ++sn;
return a + " " + c + "__" + sn + b;
}

// Deminimize javascript for debugging purposes.
// Then the line numbers in the error messages actually mean something.
// This is only called when debugging is on. Users won't invoke this machinery.
// Argument is the script object.
// escodegen.generate and esprima.parse are found in demin.js.
function deminimize(s) {
if( s.dom$class != "Script") return;
if(s.demin) return; // already expanded
s.demin = true;
s.expanded = false;
if(! s.text) return;

// Don't deminimize if short, or if average line length is less than 120.
if(s.text.length < 1000) return;
var i, linecount = 1;
for(i=0; i<s.text.length; ++i)
if(s.text.substr(i,1) === '\n') ++linecount;
if(s.text.length / linecount <= 120) return;

/*********************************************************************
You're not gonna believe this.
paypal.com, and perhaps other websites, use an obfuscator, that hangs forever
if you're javascript engine doesn't do exactly what it's suppose to.
As I write this, edbrowse + quickjs works, however, it fails if you deminimize
the code for debugging. And it fails even more if you add trace points.
They deliberately set it up to fail if the js code is deminimized.
They don't want you to understand it.
There is a deceptive function called removeCookie, that has nothing to do
with cookies. Another function tests removeCookie.toString(),
and expects it to be  a simple compact return statement.
If it spreads across multiple lines (as happens with deminimization),
or if it includes tracing software, then it all blows up.
https://www.paypal.com/auth/createchallenge/381145a4bcdc015f/recaptchav3.js
I can put it back the way it was, or just not deminimize that particular script.
There are pros and cons either way.
For now I'm taking the simpler approach, and leaving the script alone.
Watch for the compact removeCookie function, that is my flag.
There may be other obfuscators out there, and other cleanup
procedures that we have to endure, and maintain, as the obfuscators evolve.
*********************************************************************/

if(s.text.indexOf("removeCookie':function(){return'dev'") > 0) {
alert("deminimization skipped due to removeCookie test");
return;
}
/* If you wanna fix this one after the fact:
var trouble = /'removeCookie': *function *\(\)\ *{\n *return *'dev';\n *}/;
if(trouble.test(s.text))
s.text = s.text.replace(trouble, "'removeCookie':function(){return'dev';}");
*/

// Ok, run it through the deminimizer.
if(self.escodegen) {
alert3("deminimizing");
s.original = s.text;
s.text = escodegen.generate(esprima.parse(s.text));
s.expanded = true;
} else {
alert("deminimization not available");
}
}

// Trace with possible breakpoints.
function addTrace(s) {
if( s.dom$class != "Script") return;
if(! s.text) return;
if(s.text.indexOf("trace"+"@(") >= 0) // already traced
return;
var w = my$win();
if(w.$jt$c == 'z') w.$jt$c = 'a';
else w.$jt$c = String.fromCharCode(w.$jt$c.charCodeAt(0) + 1);
w.$jt$sn = 0;
alert3("adding trace under " + w.$jt$c);
// Watch out, tools/uncomment will muck with this regexp if we're not careful!
// I escape some spaces with \ so they don't get crunched away.
// First name the anonymous functions; then put in the trace points.
s.text = s.text.replace(/(\bfunction *)(\([\w ,]*\)\ *{\n)/g, jtfn1);
s.text = s.text.replace(/(\bdo \{|\bwhile \([^{}\n]*\)\ *{|\bfor \([^{}\n]*\)\ *{|\bif \([^{}\n]*\)\ *{|\bcatch \(\w*\)\ *{|\belse \{|\btry \{|\bfunction *\w*\([\w ,]*\)\ *{|[^\n)]\n *)(var |\n)/g, jtfn0);
return;
}

// copy of the Event class, because Blob needs it.
Event = function(etype){
    // event state is kept read-only by forcing
    // a new object for each event.  This may not
    // be appropriate in the long run and we'll
    // have to decide if we simply dont adhere to
    // the read-only restriction of the specification
    this.bubbles =     this.cancelable = true;
    this.cancelled = this.defaultPrevented = false;
    this.currentTarget =     this.target = null;
    this.eventPhase = 0;
    this.timeStamp = new Date().getTime();
if(typeof etype == "string") this.type = etype;
};

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

/**!
 * url-search-params-polyfill
 *
 * @author Jerry Bendy (https://github.com/jerrybendy)
 * @licence MIT
 */

(function(self) {
    'use strict';

    var nativeURLSearchParams = (function() {
            // #41 Fix issue in RN
            try {
                if (self.URLSearchParams && (new self.URLSearchParams('foo=bar')).get('foo') === 'bar') {
                    return self.URLSearchParams;
                }
            } catch (e) {}
            return null;
        })(),
        isSupportObjectConstructor = nativeURLSearchParams && (new nativeURLSearchParams({a: 1})).toString() === 'a=1',
        // There is a bug in safari 10.1 (and earlier) that incorrectly decodes `%2B` as an empty space and not a plus.
        decodesPlusesCorrectly = nativeURLSearchParams && (new nativeURLSearchParams('s=%2B').get('s') === '+'),
        __URLSearchParams__ = "__URLSearchParams__",
        // Fix bug in Edge which cannot encode ' &' correctly
        encodesAmpersandsCorrectly = nativeURLSearchParams ? (function() {
            var ampersandTest = new nativeURLSearchParams();
            ampersandTest.append('s', ' &');
            return ampersandTest.toString() === 's=+%26';
        })() : true,
        prototype = URLSearchParamsPolyfill.prototype,
        iterable = !!(self.Symbol && self.Symbol.iterator);

    if (nativeURLSearchParams && isSupportObjectConstructor && decodesPlusesCorrectly && encodesAmpersandsCorrectly) {
        return;
    }


    /**
     * Make a URLSearchParams instance
     *
     * @param {object|string|URLSearchParams} search
     * @constructor
     */
    function URLSearchParamsPolyfill(search) {
        search = search || "";

        // support construct object with another URLSearchParams instance
        if (search instanceof URLSearchParams || search instanceof URLSearchParamsPolyfill) {
            search = search.toString();
        }
        this [__URLSearchParams__] = parseToDict(search);
    }


    /**
     * Appends a specified key/value pair as a new search parameter.
     *
     * @param {string} name
     * @param {string} value
     */
    prototype.append = function(name, value) {
        appendTo(this [__URLSearchParams__], name, value);
    };

    /**
     * Deletes the given search parameter, and its associated value,
     * from the list of all search parameters.
     *
     * @param {string} name
     */
    prototype['delete'] = function(name) {
        delete this [__URLSearchParams__] [name];
    };

    /**
     * Returns the first value associated to the given search parameter.
     *
     * @param {string} name
     * @returns {string|null}
     */
    prototype.get = function(name) {
        var dict = this [__URLSearchParams__];
        return this.has(name) ? dict[name][0] : null;
    };

    /**
     * Returns all the values association with a given search parameter.
     *
     * @param {string} name
     * @returns {Array}
     */
    prototype.getAll = function(name) {
        var dict = this [__URLSearchParams__];
        return this.has(name) ? dict [name].slice(0) : [];
    };

    /**
     * Returns a Boolean indicating if such a search parameter exists.
     *
     * @param {string} name
     * @returns {boolean}
     */
    prototype.has = function(name) {
        return hasOwnProperty(this [__URLSearchParams__], name);
    };

    /**
     * Sets the value associated to a given search parameter to
     * the given value. If there were several values, delete the
     * others.
     *
     * @param {string} name
     * @param {string} value
     */
    prototype.set = function set(name, value) {
        this [__URLSearchParams__][name] = ['' + value];
    };

    /**
     * Returns a string containg a query string suitable for use in a URL.
     *
     * @returns {string}
     */
    prototype.toString = function() {
        var dict = this[__URLSearchParams__], query = [], i, key, name, value;
        for (key in dict) {
            name = encode(key);
            for (i = 0, value = dict[key]; i < value.length; i++) {
                query.push(name + '=' + encode(value[i]));
            }
        }
        return query.join('&');
    };

    // There is a bug in Safari 10.1 and `Proxy`ing it is not enough.
    var forSureUsePolyfill = !decodesPlusesCorrectly;
    var useProxy = (!forSureUsePolyfill && nativeURLSearchParams && !isSupportObjectConstructor && self.Proxy);
    var propValue; 
    if (useProxy) {
        // Safari 10.0 doesn't support Proxy, so it won't extend URLSearchParams on safari 10.0
        propValue = new Proxy(nativeURLSearchParams, {
            construct: function (target, args) {
                return new target((new URLSearchParamsPolyfill(args[0]).toString()));
            }
        })
        // Chrome <=60 .toString() on a function proxy got error "Function.prototype.toString is not generic"
        propValue.toString = Function.prototype.toString.bind(URLSearchParamsPolyfill);
    } else {
        propValue = URLSearchParamsPolyfill;
    }
    /*
     * Apply polifill to global object and append other prototype into it
     */
    Object.defineProperty(self, 'URLSearchParams', {
        value: propValue
    });

    var USPProto = self.URLSearchParams.prototype;

    USPProto.polyfill = true;

    /**
     *
     * @param {function} callback
     * @param {object} thisArg
     */
    USPProto.forEach = USPProto.forEach || function(callback, thisArg) {
        var dict = parseToDict(this.toString());
        Object.getOwnPropertyNames(dict).forEach(function(name) {
            dict[name].forEach(function(value) {
                callback.call(thisArg, value, name, this);
            }, this);
        }, this);
    };

    /**
     * Sort all name-value pairs
     */
    USPProto.sort = USPProto.sort || function() {
        var dict = parseToDict(this.toString()), keys = [], k, i, j;
        for (k in dict) {
            keys.push(k);
        }
        keys.sort();

        for (i = 0; i < keys.length; i++) {
            this['delete'](keys[i]);
        }
        for (i = 0; i < keys.length; i++) {
            var key = keys[i], values = dict[key];
            for (j = 0; j < values.length; j++) {
                this.append(key, values[j]);
            }
        }
    };

    /**
     * Returns an iterator allowing to go through all keys of
     * the key/value pairs contained in this object.
     *
     * @returns {function}
     */
    USPProto.keys = USPProto.keys || function() {
        var items = [];
        this.forEach(function(item, name) {
            items.push(name);
        });
        return makeIterator(items);
    };

    /**
     * Returns an iterator allowing to go through all values of
     * the key/value pairs contained in this object.
     *
     * @returns {function}
     */
    USPProto.values = USPProto.values || function() {
        var items = [];
        this.forEach(function(item) {
            items.push(item);
        });
        return makeIterator(items);
    };

    /**
     * Returns an iterator allowing to go through all key/value
     * pairs contained in this object.
     *
     * @returns {function}
     */
    USPProto.entries = USPProto.entries || function() {
        var items = [];
        this.forEach(function(item, name) {
            items.push([name, item]);
        });
        return makeIterator(items);
    };


    if (iterable) {
        USPProto[self.Symbol.iterator] = USPProto[self.Symbol.iterator] || USPProto.entries;
    }


    function encode(str) {
        var replace = {
            '!': '%21',
            "'": '%27',
            '(': '%28',
            ')': '%29',
            '~': '%7E',
            '%20': '+',
            '%00': '\x00'
        };
        return encodeURIComponent(str).replace(/[!'\(\)~]|%20|%00/g, function(match) {
            return replace[match];
        });
    }

    function decode(str) {
        return str
            .replace(/[ +]/g, '%20')
            .replace(/(%[a-f0-9]{2})+/ig, function(match) {
                return decodeURIComponent(match);
            });
    }

    function makeIterator(arr) {
        var iterator = {
            next: function() {
                var value = arr.shift();
                return {done: value === undefined, value: value};
            }
        };

        if (iterable) {
            iterator[self.Symbol.iterator] = function() {
                return iterator;
            };
        }

        return iterator;
    }

    function parseToDict(search) {
        var dict = {};

        if (typeof search === "object") {
            // if `search` is an array, treat it as a sequence
            if (isArray(search)) {
                for (var i = 0; i < search.length; i++) {
                    var item = search[i];
                    if (isArray(item) && item.length === 2) {
                        appendTo(dict, item[0], item[1]);
                    } else {
                        throw new TypeError("Failed to construct 'URLSearchParams': Sequence initializer must only contain pair elements");
                    }
                }

            } else {
                for (var key in search) {
                    if (search.hasOwnProperty(key)) {
                        appendTo(dict, key, search[key]);
                    }
                }
            }

        } else {
            // remove first '?'
            if (search.indexOf("?") === 0) {
                search = search.slice(1);
            }

            var pairs = search.split("&");
            for (var j = 0; j < pairs.length; j++) {
                var value = pairs [j],
                    index = value.indexOf('=');

                if (-1 < index) {
                    appendTo(dict, decode(value.slice(0, index)), decode(value.slice(index + 1)));

                } else {
                    if (value) {
                        appendTo(dict, decode(value), '');
                    }
                }
            }
        }

        return dict;
    }

    function appendTo(dict, name, value) {
        var val = typeof value === 'string' ? value : (
            value !== null && value !== undefined && typeof value.toString === 'function' ? value.toString() : JSON.stringify(value)
        );

        // #47 Prevent using `hasOwnProperty` as a property name
        if (hasOwnProperty(dict, name)) {
            dict[name].push(val);
        } else {
            dict[name] = [val];
        }
    }

    function isArray(val) {
        return !!val && '[object Array]' === Object.prototype.toString.call(val);
    }

    function hasOwnProperty(obj, prop) {
        return Object.prototype.hasOwnProperty.call(obj, prop);
    }

})(typeof global !== 'undefined' ? global : (typeof window !== 'undefined' ? window : this));

// end third party code.

// lock down, for security.

for(var k in URLSearchParams.prototype)
Object.defineProperty(URLSearchParams.prototype, k,{writable:false,configurable:false});
Object.defineProperty(Object.prototype, "toString",{enumerable:false,writable:false,configurable:false});

var flist = ["Math", "Date", "Promise", "eval", "Array", "Uint8Array",
"Error", "String", "parseInt", "Event",
"alert","alert3","alert4","dumptree","uptrace",
"showscripts", "showframes", "searchscripts", "snapshot", "aloop",
"eb$base$snapshot", "set_location_hash",
"eb$newLocation","eb$logElement",
"getElement", "getHead", "setHead", "getBody", "setBody",
"getElementsByTagName", "getElementsByClassName", "getElementsByName", "getElementById","nodeContains",
"eb$gebtn","eb$gebn","eb$gebcn","eb$gebid","eb$cont",
"dispatchEvent","addEventListener","removeEventListener","attachOn",
"attachEvent","detachEvent","eb$listen","eb$unlisten",
"NodeFilter","createNodeIterator","createTreeWalker",
"logtime","defport","setDefaultPort","camelCase","dataCamel","isabove",
"classList","classListAdd","classListRemove","classListReplace","classListToggle","classListContains",
"mutFixup", "mrList","mrKids", "rowReindex", "insertRow", "deleteRow",
"insertCell", "deleteCell",
"appendFragment", "insertFragment",
"appendChild", "prependChild", "insertBefore", "replaceChild", "hasChildNodes",
"eb$getSibling", "eb$getElementSibling", "insertAdjacentElement",
"append", "prepend", "before", "after", "replaceWith",
"formname", "formAppendChild", "formInsertBefore", "formRemoveChild",
"implicitMember",
"getAttribute", "getAttributeNames", "getAttributeNS",
"hasAttribute", "hasAttributeNS",
"setAttribute", "markAttribute", "setAttributeNS",
"removeAttribute", "removeAttributeNS", "getAttributeNode",
"clone1", "findObject", "correspondingObject",
"compareDocumentPosition",
"cssGather", "getComputedStyle", "computeStyleInline", "cssTextGet",
"injectSetup", "eb$visible",
"insertAdjacentHTML", "htmlString", "outer$1", "textUnder", "newTextUnder",
"URL", "File", "FileReader", "Blob",
"MessagePortPolyfill", "MessageChannelPolyfill",
"clickfn", "checkset",
"jtfn0", "jtfn1", "deminimize", "addTrace",
];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(this, flist[i], {writable:false,configurable:false});

// some class prototypes
var flist = [Date, Promise, Array, Uint8Array, Error, String, URL, URLSearchParams];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(flist[i], "prototype", {writable:false,configurable:false});

Object.defineProperty(URL, "createObjectURL", {writable:false,configurable:false});
Object.defineProperty(URL, "revokeObjectURL", {writable:false,configurable:false});
Object.defineProperty(Blob, "prototype", {writable:false,configurable:false});
Object.defineProperty(Blob.prototype, "text", {writable:false,configurable:false});
Object.defineProperty(Blob.prototype, "slice", {writable:false,configurable:false});
Object.defineProperty(Blob.prototype, "stream", {writable:false,configurable:false});
Object.defineProperty(Blob.prototype, "arrayBuffer", {writable:false,configurable:false});
