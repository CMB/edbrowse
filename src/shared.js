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

// so we can run this stand alone
if(typeof puts === "undefined") {
function puts(s){}
}

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
// I overwrite toString in some cases, so can't nail this down until later
// Object.defineProperty(Function.prototype, "toString",{enumerable:false,writable:false,configurable:false});
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
var nn = top.nodeName;
var r = "";
var extra = "";
if(nn === "#text" && top.data) {
extra = top.data;
extra = extra.replace(/^[ \t\n]*/, "");
var l = extra.indexOf('\n');
if(l >= 0) extra = extra.substr(0,l);
if(extra.length > 120) extra = extra.substr(0,120);
}
if(nn === "OPTION" && top.value)
extra = top.value;
if(nn === "OPTION" && top.text) {
if(extra.length) extra += ' ';
extra += top.text;
}
if(nn === "A" && top.href)
extra = top.href.toString();
if(nn === "BASE" && top.href)
extra = top.href.toString();
if(extra.length) extra = ' ' + extra;
// some tags should never have anything below them so skip the parentheses notation for these.
if((nn == "BASE" || nn == "META" || nn == "LINK" ||nn == "#text" || nn == "IMAGE" || nn == "OPTION" || nn == "INPUT" || nn == "SCRIPT") &&
(!top.childNodes || top.childNodes.length == 0)) {
r += nn + extra + '\n';
return r;
}
r += nn + "{" + extra + '\n';
if(top.is$frame) {
if(top.eb$expf) r += top.contentWindow.dumptree(top.contentDocument);
} else if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
r += dumptree(c);
}
}
r += '}\n';
return r;
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

function by_esn(n) {
if(typeof n != "number") { alert("numeric argument expected"); return; }
var a = my$doc().getElementsByTagName("*");
for(var i = 0; i < a.length; ++i)
if(a[i].eb$seqno === n) return a[i];
return null;
}

/*********************************************************************
Show the scripts, where they come from, type, length, whether deminimized.
This uses document.scripts and getElementsByTagname() so you see
all the scripts, hopefully, not just those that were in the original html.
The list is left in $ss for convenient access.
my$win() is used to get the window of the running context, where you are,
rather than this window, which is often not what you want.
*********************************************************************/

function showscripts() {
var i, s, m;
var w = my$win(), d = my$doc();
var slist = [];
for(i=0; i<d.scripts.length; ++i) {
s = d.scripts[i];
s.from$html = true;
slist.push(s);
}
var getlist = d.getElementsByTagName("script");
for(i=0; i<getlist.length; ++i) {
s = getlist[i];
if(!s.from$html) slist.push(s);
}
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
var search = ss.indexOf('?');
if(search > 0) ss = ss.substr(0,search);
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
alert("bye   ub   ci+   /<head/r from   w base   q");
}

function set_location_hash(h) { h = '#'+h;
var w = my$win(), d = my$doc(), body = d.body;
var loc = w.location$2;
// save the old url, but I don't know if it's a URL or a string
var oldURL = loc.toString();
loc.hash$val = h;
loc.href$val = loc.href$val.replace(/#.*/, "") + h; 
var newURL = loc.toString();
loc = d.location$2;
loc.hash$val = h;
loc.href$val = loc.href$val.replace(/#.*/, "") + h;
// call the onhashchange handlers
// this code comes from dispatchEvent, but is simpler.
var e = new (w.HashChangeEvent);
e.eventPhase = 2, e.target = e.currentTarget = w;
e.oldURL = oldURL, e.newURL = newURL;
var fn1 = "on" + e.type;
var fn2 = fn1 + "$$fn";
if(typeof w[fn2] == "function") {
if(db$flags(1)) alert3("current Window hashchange");
w[fn2](e);
} else if(typeof w[fn1] == "function") {
if(db$flags(1)) alert3("current Window hashchange\nfire assigned");
w[fn1](e);
if(db$flags(1)) alert3("endfire assigned");
}
if(typeof body[fn2] == "function") {
if(db$flags(1)) alert3("current Body hashchange");
body[fn2](e);
} else if(typeof body[fn1] == "function") {
if(db$flags(1)) alert3("current Body hashchange\nfire assigned");
body[fn1](e);
if(db$flags(1)) alert3("endfire assigned");
}
}

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

function showarg(x) {
var l, w = my$win ? my$win() : window;
// null comes out as an object
if(x === null) return "null";
switch(typeof x) {
case "undefined": return "undefined";
case "number": case "boolean": return x.toString();
case "function": return x.name;
case "string":
l = x.length;
if(l > 60) x = x.substr(0,60) + "...";
return x.replace(/\n/g, "\\n");
case "object":
if(Array.isArray(x)) {
l = x.length;
var i, r = "array[" + x.length + "]{";
if(l > 20) l = 20;
for(i=0; i<l; ++i)
r += showarg(x[i]) + ',';
if(l < x.length) r += "...";
r += '}';
return r;
}
if(x.dom$class === "URL") return "URL(" + x.toString() + ")";
if(x.nodeType == 1 && x.childNodes && x.nodeName) { // html element
var s = "<" + x.nodeName + ">";
var y = x.getAttribute("id");
if(y) s += " id=" + y;
y = x.getAttribute("class");
if(y) s += " class=" + y;
return s;
}
if(typeof x.HTMLDivElement == "function" && typeof x.HTMLTableElement == "function") {
var r = "window";
if(x.location && x.location.href) r += " " + x.location.href;
return r;
}
return "object";
default: return "?";
}
}

function showarglist(a) {
if(typeof a != "object" ||
typeof a.length != "number")
return "not an array";
var s = "";
for(var i = 0; i < a.length; ++i) {
if(i) s += ", ";
s += showarg(a[i]);
}
return s;
}

// document.head, document.body; shortcuts to head and body.
function getElement() {
  var e = this.lastChild;
if(!e) { alert3("missing documentElement node"); return null; }
if(e.nodeName.toUpperCase() != "HTML") alert3("html node name " + e.nodeName);
return e
}

function getHead() {
 var e = this.documentElement;
if(!e) return null;
// In case somebody adds extra nodes under <html>, I search for head and body.
// But it should always be head, body.
for(var i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName.toUpperCase() == "HEAD") return e.childNodes[i];
alert3("missing head node"); return null;
}

function setHead(h) {
 var i, e = this.documentElement;
if(!e) return;
for(i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName.toUpperCase() == "HEAD") break;
if(i < e.childNodes.length) e.removeChild(e.childNodes[i]); else i=0;
if(h) {
if(h.nodeName.toUpperCase() != "HEAD") { alert3("head replaced with node " + h.nodeName); h.nodeName = "HEAD"; }
if(i == e.childNodes.length) e.appendChild(h);
else e.insertBefore(h, e.childNodes[i]);
}
}

function getBody() {
 var e = this.documentElement;
if(!e) return null;
for(var i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName.toUpperCase() == "BODY") return e.childNodes[i];
alert3("missing body node"); return null;
}

function setBody(b) {
 var i, e = this.documentElement;
if(!e) return;
for(i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName.toUpperCase() == "BODY") break;
if(i < e.childNodes.length) e.removeChild(e.childNodes[i]);
if(b) {
if(b.nodeName.toUpperCase() != "BODY") { alert3("body replaced with node " + b.nodeName); b.nodeName = "BODY"; }
if(i == e.childNodes.length) e.appendChild(b);
else e.insertBefore(b, e.childNodes[i]);
}
}

function getRootNode(o) {
var composed = false;
if(typeof o == "object" && o.composed)
composed = true;
var t = this;
while(t) {
if(t.nodeName == "#document") return t;
if(!composed && t.nodeName == "SHADOWROOT") return t;
t = t.parentNode;
}
alert3("getRootNode no root found");
return null;
}

// wrapper to turn function blah{ my js code } into function blah{ [native code] }
// This is required by sanity tests in jquery and other libraries.
function wrapString() {
return Object.toString.bind(this)().replace(/\([\u0000-\uffff]*/, "() {\n    [native code]\n}");
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
return gebtn(this, s, true);
}

function gebtn(top, s, first) {
var a = new (my$win().Array);
if(!first && (s === '*' || (top.nodeName && top.nodeName.toLowerCase() === s)))
a.push(top);
if(top.childNodes) {
// don't descend into another frame.
// The frame has no children through childNodes, so we don't really need this line.
if(!top.is$frame)
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(gebtn(c, s, false));
}
}
return a;
}

function getElementsByName(s) {
if(!s) { // missing or null argument
alert3("getElementsByName(type " + typeof s + ")");
return new (my$win().Array);
}
return gebn(this, s, true);
}

function gebn(top, s, first) {
var a = new (my$win().Array);
if(!first && (s === '*' || top.name === s))
a.push(top);
if(top.childNodes) {
if(!top.is$frame)
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(gebn(c, s, false));
}
}
return a;
}

function getElementById(s) {
if(!s) { // missing or null argument
alert3("getElementById(type " + typeof s + ")");
return null;
}
gebi_hash = this.id$hash;
if(gebi_hash) { // should always be there
// efficiency, see if we have hashed this id
var t = gebi_hash[s];
if(t) {
// is it still rooted?
for(var u = t.parentNode; u; u = u.parentNode)
if(u == this) return t;
delete gebi_hash[s];
}}
if(!gebi_hash) {
// look the traditional way
return gebi(this, s);
}
// look for nonsense to build up the hash
gebi(this, "*@%impossible`[]")
return gebi_hash[s] ? gebi_hash[s] : null;
}

function gebi(top, s) {
if(top.id) {
if(gebi_hash) gebi_hash[top.id] = top;
if(top.id == s) return top;
}
if(top.childNodes) {
// don't descend into another frame.
// The frame has no children through childNodes, so we don't really need this line.
if(top.is$frame) return null;
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
var res = gebi(c, s);
if(res) return res;
}
}
return null;
}

function getElementsByClassName(s) {
if(!s) { // missing or null argument
alert3("getElementsByTagName(type " + typeof s + ")");
return new (my$win().Array);
}
s = s . replace (/^\s+/, '') . replace (/\s+$/, '');
if(s === "") return new (my$win().Array);
var sa = s.split(/\s+/);
return gebcn(this, sa, true);
}

function gebcn(top, sa, first) {
var a = new (my$win().Array);
if(!first && top.cl$present) {
var ok = true;
for(var i=0; i<sa.length; ++i) {
var w = sa[i];
if(w === '*') { ok = true; break; }
if(!top.classList.contains(w)) { ok = false; break; }
}
if(ok) a.push(top);
}
if(top.childNodes) {
if(!top.is$frame)
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(gebcn(c, sa, false));
}
}
return a;
}

function nodeContains(n) {  return cont(this, n); }

function cont(top, n) {
if(top === n) return true;
if(!top.childNodes) return false;
if(top.is$frame) return false;
for(var i=0; i<top.childNodes.length; ++i)
if(cont(top.childNodes[i], n)) return true;
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
However, attachEvent is deprecated, and not implemented here.
This is frickin complicated, so set eventDebug to debug it.
*********************************************************************/

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
return;
}

for(var j=0; j<this[evarray].length; ++j) {
if(this[evarray][j] == handler) {
if(db$flags(1)) alert3("handler is duplicate, do not add");
return;
}
/*********************************************************************
And now for a problem we found on
https://github.com/validator/validator/releases/tag/20.6.30
The handler is an anonymous function inside another function a().
in other words, a() adds this handler to window.onload.
the handler then calls a(), which adds the handler again,
a duplicate, which I tried to check for in the previous block.
It's the exact same code, already compiled, but a new
object is created for this handler, so it can be a local variable in the
running instance of a().
It is illustrated by this code fragment, which you can run through any js engine.
look = [];
function a(n) { look[n] = function(){return77}}
a(0),a(1);
print(look[0]==look[1])
print(look[0].toString()==look[1].toString())
So I'm going to run a toString() test here, and hope it doesn't produce
any false positives. When in doubt run db3 and dbev
so you can see what is going on.
Bad news, acid3 test 31 presents this very yfalse positive.
Two separate handlers with exactly the same code.
So I also check for fileName and lineNumber.
This makes false positives less likely, but not impossible.
Remember those minimized javascript files that are all on one line.
fileName and lineNumber will always agree.   Ugh!
*********************************************************************/
if(this[evarray][j].toString() == handler.toString() &&
this[evarray][j].fileName == handler.fileName &&
this[evarray][j].lineNumber == handler.lineNumber) {
if(db$flags(1)) alert3("handler is duplicate by toString(), do not add");
return;
}
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
o.list = gebtn(root, "*");
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
o.list = gebtn(root, "*");
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
a = incr > 0 ? a.nextSibling : a.previousSibling;
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
a = incr > 0 ? a.nextSibling : a.previousSibling;
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
this.node.setAttribute("class", this.join(' '));
}

function classListAdd() {
for(var i=0; i<arguments.length; ++i) {
for(var j=0; j<this.length; ++j)
if(arguments[i] == this[j]) break;
if(j == this.length) this.push(arguments[i]);
}
this.node.setAttribute("class", this.join(' '));
}

function classListReplace(o, n) {
if(!o) return;
if(!n) { this.remove(o); return; }
for(var j=0; j<this.length; ++j)
if(o == this[j]) { this[j] = n; break; }
this.node.setAttribute("class", this.join(' '));
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
var c = node.getAttribute("class");
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
var w2; // might not be the same window as w
var list = w.mutList;
// frames is a live array of windows.
// Test: a change to the tree, and the base node is rooted,
// and the thing added or removed is a frame or an array or it has frames below.
if(!isattr && (w2 = isRooted(b))) {
var j = typeof y == "object" ? y : z;
if(Array.isArray(j) || j.is$frame || (j.childNodes&&j.getElementsByTagName("iframe").length))
frames$rebuild(w2);
}
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

// The return is completely undocumented. I have determined it is not null.
// I assume it is the appended fragment.
function appendFragment(p,  frag) { var c; while(c = frag.firstChild) p.appendChild(c); return frag; }
function insertFragment(p, frag, l) { var c; while(c = frag.firstChild) p.insertBefore(c, l); return frag; }

// if t is linkd into the tree, return the containing window
function isRooted(t) {
while(t) {
if(t.nodeName == "HTML") return t.eb$win;
t = t.parentNode;
}
return undefined;
}

function frames$rebuild(w) {
var i, f, l, f2, l2;
// unlink the name references
for(i=0; i<(l=w.frames$2.length); ++i) {
f = w.frames$2[i];
if(f.name) delete w.frames[f.name];
}
f2 = w.document.getElementsByTagName("iframe");
l2 = f2.length;
alert3("rebuild frames in context " + w.eb$ctx + " lengths " + l + " and " + l2);
if(l2 < l) for(i=l2; i<l; ++i) delete w.frames[i];
if(l2 > l) for(i=l; i<l2; ++i)
w.eval('Object.defineProperty(frames,"'+i+'",{get:function(){return frames$2['+i+'].contentWindow},configurable:true})')
// and relink the names
for(i=0; i<l2; ++i) {
f = f2[i];
if(f.name)
w.eval('Object.defineProperty(frames,"'+f.name+'",{get:function(){return frames$2['+i+'].contentWindow},configurable:true})')
}
w.frames$2 = f2;
}

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
if(r) mutFixup(this, false, c, null);
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
if(r) mutFixup(this, false, r, null);
return r;
}

function removeChild(c) {
if(!c) return null;
var r = this.eb$rmch2(c);
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

function getSibling (obj,direction) {
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

function getElementSibling (obj,direction) {
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
while(t.dom$class != "HTMLTableElement") {
if(t.is$frame) return;
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
if(s.dom$class == "HTMLTableRowElement")
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
// Should this be ownerDocument, the context that created the table,
// or my$doc(), the running context. I think the latter is safer.
var r = my$doc().createElement("tr");
if(t.dom$class != "HTMLTableElement") {
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
if(r.dom$class != "HTMLTableRowElement") return;
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

function deleteCell(n) {
var l = this.cells.length;
if(typeof n != "number") n = -1;
if(n == -1) n = 0;
if(n >= 0 && n < l)
this.removeChild(this.cells[n]);
}

/*********************************************************************
This is a workaround, when setAttribute is doing something it shouldn't,
like form.setAttribute("elements") or some such.
I call these implicit members, we shouldn't mess with them.
*********************************************************************/

function implicitMember(o, name) {
return name === "elements" && o.dom$class == "HTMLFormElement" ||
name === "rows" && (o.dom$class == "HTMLTableElement" || o.dom$class == "tBody" || o.dom$class == "tHead" || o.dom$class == "tFoot") ||
name === "tBodies" && o.dom$class == "HTMLTableElement" ||
(name === "cells" || name === "rowIndex" || name === "sectionRowIndex") && o.dom$class == "HTMLTableRowElement" ||
name === "className" ||
// no clue what getAttribute("style") is suppose to do
name === "style" ||
name === "htmlFor" && o.dom$class == "HTMLLabelElement" ||
name === "options" && o.dom$class == "HTMLSelectElement" ||
name === "selectedOptions" && o.dom$class == "HTMLSelectElement";
}

function spilldown(name) {
// Ideally I should have a list of all the on functions, but I'm gonna say
// any word that starts with on spills down.
if(name.match(/^on[a-zA-Z]*$/)) return true;
// I'm not sure value should spill down, setAttribute("value","blah")
return name == "value" ||
// class shouldn't spill down. But if we don't, the class == last$class system fails.
// We don't detect a change in class and rerun the css rules.
// We could push it down to new$class and compare new$class and last$class.
name == "class";
}

// This stuff has to agree with the tables in startwindow.js starting at "src"
function spilldownResolveURL(t, name) {
if(!t.nodeName) return false;
var nn = t.nodeName.toLowerCase();
return name == "src" && (nn == "frame" || nn == "iframe") ||
name == "href" && (nn == "a" || nn == "area");
}

function spilldownResolve(t, name) {
if(!t.nodeName) return false;
var nn = t.nodeName.toLowerCase();
return name == "action" && nn == "form" ||
name == "src" && (nn == "img" || nn == "script" || nn == "audio" || nn == "video") ||
name == "href" && (nn == "link" || nn == "base");
}

function spilldownBool(t, name) {
if(!t.nodeName) return false;
var nn = t.nodeName.toLowerCase();
return name == "aria-hidden" ||
name == "selected" && nn == "option" ||
name == "checked" && nn == "input";
}

/*********************************************************************
Set and clear attributes. This is done in 3 different ways,
the third using attributes as a NamedNodeMap.
This may be overkill - I don't know.
*********************************************************************/

function getAttribute(name) {
var a, w = my$win();
if(!this.eb$xml) name = name.toLowerCase();
if(implicitMember(this, name)) return null;
// has to be a real attribute
if(!this.attributes$2) return null;
a = null;
for(var i=0; i<this.attributes.length; ++i)
if(this.attributes[i].name == name) { a = this.attributes[i]; break; }
if(!a) return null;
var v = a.value;
var t = typeof v;
if(t == "undefined" || v == null) return null;
// I stringify URL objects, should we do that to other objects?
if(t == 'object' && (v.dom$class == "URL" || v instanceof w.URL)) return v.toString();
// number, boolean, object; it goes back as it was put in.
return v; }
function hasAttribute(name) { return this.getAttribute(name) !== null; }

function getAttributeNames(name) {
var w = my$win();
var a = new w.Array;
if(!this.attributes$2) return a;
for(var l = 0; l < this.attributes$2.length; ++l) {
var z = this.attributes$2[l].name;
a.push(z);
}
return a;
}

function getAttributeNS(space, name) {
if(space && !name.match(/:/)) name = space + ":" + name;
return this.getAttribute(name);
}
function hasAttributeNS(space, name) { return this.getAttributeNS(space, name) !== null;}

function setAttribute(name, v) {
var a, w = my$win();
if(!this.eb$xml) name = name.toLowerCase();
// special code for style
if(name == "style" && this.style.dom$class == "CSSStyleDeclaration") {
this.style.cssText = v;
return;
}
if(implicitMember(this, name)) return;
var oldv = null;
// referencing attributes should create it on demand, but if it doesn't...
if(!this.attributes) this.attributes$2 = new w.NamedNodeMap;
a = null;
for(var i=0; i<this.attributes.length; ++i)
if(this.attributes[i].name == name) { a = this.attributes[i]; break; }
if(!a) {
a = new w.Attr();
a.owner = this;
a.name = name;
this.attributes.push(a);
} else {
oldv = a.value;
}
a.value = v;
if(name.substr(0,5) == "data-") {
// referencing dataset should create it on demand, but if it doesn't...
if(!this.dataset) this.dataset$2 = {};
this.dataset[dataCamel(name)] = v;
}
// names that spill down into the actual property
// should we be doing any of this for xml nodes?
if(spilldown(name)) this[name] = v;
if(spilldownResolve(this, name)) this.href$2 = resolveURL(w.eb$base, v);
if(spilldownResolveURL(this, name)) this.href$2 = new (w.URL)(resolveURL(w.eb$base, v));
if(spilldownBool(this, name)) {
// This one is required by acid test 43, I don't understand it at all.
if(name == "checked" && v == "checked")
this.defaultChecked = true;
else {
// is a nonsense string like blah, true or false? I don't know.
// For now I'll assume it's true.
v = (v === "false" ? false : true);
this[name] = v;
}
}
mutFixup(this, true, name, oldv);
}
function setAttributeNS(space, name, v) {
if(space && !name.match(/:/)) name = space + ":" + name;
this.setAttribute(name, v);
}

function removeAttribute(name) {
if(!this.attributes$2) return;
if(!this.eb$xml)     name = name.toLowerCase();
// special code for style
if(name == "style" && this.style.dom$class == "CSSStyleDeclaration") {
// wow I have no clue what this means but it happens, https://www.maersk.com
return;
}
if(name.substr(0,5) == "data-") {
var n = dataCamel(name);
if(this.dataset$2 && this.dataset$2[n]) delete this.dataset$2[n];
}
// should we be doing any of this for xml nodes?
if(spilldown(name)) delete this[name];
if(spilldownResolve(this, name)) delete this[name];
if(spilldownResolveURL(this, name)) delete this[name];
if(spilldownBool(this, name)) delete this[name];
// acid test 48 removes class before we can check its visibility.
// class is undefined and last$class is undefined, so getComputedStyle is never called.
if(name === "class" && !this.last$class) this.last$class = "@@";
if(name === "id" && !this.last$id) this.last$id = "@@";
var a = null;
for(var i=0; i<this.attributes.length; ++i)
if(this.attributes[i].name == name) { a = this.attributes[i]; break; }
if(!a) return;
// Have to roll our own splice.
var i, found = false;
for(i=0; i<this.attributes.length-1; ++i) {
if(!found && this.attributes[i] == a) found = true;
if(found) this.attributes[i] = this.attributes[i+1];
}
this.attributes.length = i;
delete this.attributes[i];
mutFixup(this, true, name, a.value);
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
var a = null;
for(var i=0; i<this.attributes.length; ++i)
if(this.attributes[i].name == name) { a = this.attributes[i]; break; }
return a;
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
else if(node1.dom$class == "CSSStyleDeclaration") {
if(debug) alert3("skipping style object");
return;
} else
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

// live arrays
if((item == "options" || item == "selectedOptions") && node1.dom$class == "HTMLSelectElement") continue;

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
if(node1.dom$class == "HTMLFormElement" && node1[item].length &&
node1[item][0].dom$class == "HTMLInputElement" && node1[item][0].name == item) {
var a1 = node1[item];
var a2 = node2[item];
if(debug) alert3("linking form.radio " + item + " with " + a1.length + " buttons");
a2.type = a1.type;
a2.nodeName = a1.nodeName;
if(a1.class) a2.setAttribute("class", a1.class);
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
!Array.isArray(node1) && !(node1.dom$class == "HTMLOptionElement"))
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
Object.defineProperty(this,"DOCUMENT_POSITION_DISCONNECTED",{value:1});
Object.defineProperty(this,"DOCUMENT_POSITION_PRECEDING",{value:2});
Object.defineProperty(this,"DOCUMENT_POSITION_FOLLOWING",{value:4});
Object.defineProperty(this,"DOCUMENT_POSITION_CONTAINS",{value:8});
Object.defineProperty(this,"DOCUMENT_POSITION_CONTAINED_BY",{value:16});

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

// for toolbar menubar etc
generalbar = {}
Object.defineProperty(generalbar, "visible", {value:true})

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
if(t.dom$class == "HTMLLinkElement") {
if(t.css$data && (
t.type && t.type.toLowerCase() == "text/css" ||
t.rel && t.rel.toLowerCase() == "stylesheet")) {
w.cssSource.push({data: t.css$data, src:t.href});
css_all += "@ebdelim0" + t.href + "{}\n";
css_all += t.css$data;
}
}
if(t.dom$class == "HTMLStyleElement") {
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
Object.defineProperty(w, "last$css", {enumerable:false});
w.css$ver++;
cssDocLoad(w.eb$ctx, css_all, pageload);
}

function makeSheets(all) {
var w = my$win();
var d = my$doc();
var ss = d.styleSheets;
ss.length = 0; // should already be 0
var a = all.split('\n');
// last rule ends in newline of course, but then split leaves
// an extra line after that.
if(a.length) a.pop();
var nss = null; // new style sheet
var stack = [];
for(var i = 0; i < a.length; ++i) {
var line = a[i];
if(line.substr(0,8) != "@ebdelim") {
if(nss) {
var r = new w.CSSRule;
r.cssText = line;
nss.cssRules.push(r);
}
continue;
}
var which = line.substr(8,1);
switch(which) {
case '0':
stack.length = 0; // should already be 0
nss = new w.CSSStyleSheet;
stack.push(nss);
ss.push(nss);
nss.src = line.substr(9).replace(/ *{}/,"");
break;
case '1':
nss = new w.CSSStyleSheet;
stack.push(nss);
ss.push(nss);
nss.src = line.substr(9).replace(/ *{}/,"");
break;
case '2':
stack.pop();
nss = stack.length ? stack[stack.length-1] : null;
break;
}
}
}

// e is the node and pe is the pseudoelement
function getComputedStyle(e,pe) {
var s, w = my$win();

if(!pe) pe = 0;
else if(pe == ":before") pe = 1;
else if(pe == ":after") pe = 2;
else { alert3("getComputedStyle pseudoelement " + pe + " is invalid"); pe = 0; }

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
cssApply(this.eb$ctx, e, pe);
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
if(e.dom$class == "CSSStyleDeclaration" || e.dom$class == "HTMLStyleElement") return;
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
cssApply(w.eb$ctx, e, 0);
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
if(t.dom$class == "XMLCdata") return "<![Cdata[" + t.text + "]]>";
if(t.nodeType != 1) return "";
var s = "<" + (t.nodeName ? t.nodeName : "x");
/* defer to the setAttribute system
if(t.class) s += ' class="' + t.class + '"';
if(t.id) s += ' id="' + t.id + '"';
*/
if(t.attributes$2) {
for(var l = 0; l < t.attributes$2.length; ++l) {
var a = t.attributes$2[l];
// we need to html escape certain characters, which I do a few of.
s += ' ' + a.name + "='" + a.value.toString().replace(/['<>&]/g,function(a){return "&#"+a.charCodeAt(0)+";"}) + "'";
}
}
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
var nn = top.nodeName;
if(nn == "#text") return top.data.trim();
if(nn == "SCRIPT" || nn == "#cdata-section") return top.text;
var pre = (nn=="PRE");
// we should be more general here; this doesn't handle
// <pre>hello<i>multi lined text in italics</i>world</pre>
var answer = "", part, delim = "";
var t = top.querySelectorAll("cdata,text");
for(var i=0; i<t.length; ++i) {
var u = t[i];
if(u.parentNode && u.parentNode.nodeName == "OPTION") continue;
// any other texts we should skip?
part = u.nodeName == "#text" ? u.data : u.text;
if(!pre) part = part.trim(); // should we be doing this?
if(!part) continue;
if(answer) answer += delim;
answer += part;
}
return answer;
}

function newTextUnder(top, s, flavor) {
var l = top.childNodes.length;
for(var i=l-1; i>=0; --i)
top.removeChild(top.childNodes[i]);
// do nothing if s is undefined, or null, or the empty string
if(!s) return;
top.appendChild(my$doc().createTextNode(s));
}

function clickfn() {
var w = my$win();
var nn = this.nodeName, t = this.type;
// as though the user had clicked on this
if(nn == "BUTTON" || (nn == "INPUT" &&
(t == "button" || t == "reset" || t == "submit" || t == "checkbox" || t == "radio"))) {
var e = new w.Event;
e.initEvent("click", true, true);
if(!this.dispatchEvent(e)) return;
// do what the tag says to do
if(this.form && this.form.dom$class == "HTMLFormElement") {
if(t == "submit") {
e.initEvent("submit", true, true);
if(this.dispatchEvent(e) && this.form.submit)
this.form.submit();
}
if(t == "reset") {
e.initEvent("reset", true, true);
if(this.dispatchEvent(e) && this.form.reset)
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

// define a custom element
function cel_define(name, c, options) {
var w = my$win();
var cr = w.cel$registry;
var ext = "";
if(typeof options == "object" && options.extends) ext = options.extends;
if(ext)
alert3("define custom element " + name + " extends " + ext);
else
alert3("define custom element " + name);
if(typeof name != "string" || !name.match(/.-./)) throw new Error("SyntaxError");
if(cr[name]) throw new Error("NotSupportedError");
if(typeof c != "function") throw new Error("DOMException");
var o = {construct:c};
// what other stuff should we remember in o?
cr[name] = o;
}

function cel_get(name) {
var w = my$win();
var cr = w.cel$registry;
if(typeof name != "string" || !name.match(/.-./)) throw new Error("SyntaxError");
var o = cr[name];
return o ? o.construct : undefined;
}

// jtfn0 injects trace(blah) into the code.
// It should only be applied to deminimized code.
// jtfn1 puts a name on the anonymous function, for debugging.
// jtfn2 injects code after catch(e) {, for detection by dberr
// jtfn3 injects trace at the end of a return statement, in a tricky way.

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
var fn = c + "__" + sn; // function name
return a + " " + fn + b +
"if(step$l>=1)alert('" + fn + "(' + showarglist(arguments) + ')');\n";
}

jtfn2 = function (all, a) {
return '}catch(' + a + '){if(db$flags(3)) alert(' + a + '.toString()),alert(' + a + '.stack),step$l=2;';
}

jtfn3 = function (all, a, b) {
var w = my$win();
var c = w.$jt$c;
var sn = w.$jt$sn;
w.$jt$sn = ++sn;
// a is just whitespace, to preserve indenting
// b is the expression to return
return a + "{let x$rv=(" + b + ");trace" + "@(" + c + sn + ");return x$rv;}\n";
}

// Deminimize javascript for debugging purposes.
// Then the line numbers in the error messages actually mean something.
// This is only called when debugging is on. Users won't invoke this machinery.
// Argument is the script object.
// escodegen.generate and esprima.parse are found in demin.js.
function deminimize(s) {
alert3("deminimizing");
if( s.dom$class != "HTMLScriptElement") {alert3("wrong class " + s.dom$class); return; }
// it might not be javascript.
// This should agree with the criteria in html.c
if(s.language && !s.language.match(/^javascript\b/i)) { alert3("wrong language " + s.language); return; }
if(s.type && !s.type.match(/(\bjavascript|\/javascript)$/i)) { alert3("wrong type " + s.type); return; }
if(s.demin) { alert3("already deminimized"); return; }
s.demin = true;
s.expanded = false;
if(! s.text) { alert3("empty"); return; }

// Don't deminimize if short, or if average line length is less than 120.
if(s.text.length < 1000) { alert3("short"); return; }
var i, linecount = 1;
for(i=0; i<s.text.length; ++i)
if(s.text.substr(i,1) === '\n') ++linecount;
if(s.text.length / linecount <= 120) { alert3("short lines"); return; }

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
I use to watch for the compact removeCookie function,
but they changed that, no doubt change the code from time to time,
so nobody can figure it out.
That leaves me to check the filename, which isn't great either cause
some other website could use the same code under a different filename.
*********************************************************************/

if(s.src.indexOf("/recaptcha") > 0) {
alert("deminimization skipped due to /recaptcha in filename");
return;
}

// Ok, run it through the deminimizer.
if(self.escodegen) {
s.original = s.text;
s.text = escodegen.generate(esprima.parse(s.text));
// This is a crude workaround because codegen doesn't understand the syntax of extending a class.
// There is a patch in demin.js that inserts 18392748934
// We need to remove it here.
// buildsourcestring will remove the space after colon if I'm not careful.
s.text = s.text.replace(/:\s18392748934\n/g, "\n");
s.expanded = true;
alert3("expanded");
} else {
alert("deminimization not available");
}
}

// Trace with possible breakpoints.
function addTrace(s) {
if( s.dom$class != "HTMLScriptElement") return;
if(! s.text) return;
if(s.src.indexOf("/recaptcha") > 0) return;
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
s.text = s.text.replace(/}\ *catch\ *\((\w+)\)\ *{/g, jtfn2);
s.text = s.text.replace(/}\ *catch\ *\(\)\ *{/g, '} catch() { if(db$flags(3)) alert("catch with no argument"),step$l=2;');
s.text = s.text.replace(/(\n\ *)return\ +([^ ;\n][^;\n]*);\ *\n/g, jtfn3);
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

// placeholder for URL class, we can't share the actual class here,
// but this has to be here for the Blob code.
// See startwindow for an explanation of why this class can't be shared.
URL = {};

/*********************************************************************
Some URL methods we can define here however, and reuse elsewhere,
like the table methods etc.
The first is the rebuild method, to build the url string
when any of its components is updated.
All components are strings, except for port,
and all should be defined, even if they are empty.
*********************************************************************/

function url_rebuild() {
var h = "";
if(this.protocol$val) {
// protocol includes the colon
h = this.protocol$val;
var plc = h.toLowerCase();
if(plc != "mailto:" && plc != "telnet:" && plc != "javascript:")
h += "//";
}
if(this.host$val) {
h += this.host$val;
} else if(this.hostname$val) {
h += this.hostname$val;
if(this.port$val) h += ":" + this.port$val;
}
if(this.pathname$val) {
// pathname should always begin with /, should we check for that?
if(!this.pathname$val.match(/^\//))
h += "/";
h += this.pathname$val;
}
if(this.search$val) {
// search should always begin with ?, should we check for that?
h += this.search$val;
}
if(this.hash$val) {
// hash should always begin with #, should we check for that?
h += this.hash$val;
}
this.href$val = h;
}

function url_hrefset(v) {
var w = my$win(), inconstruct = true, firstassign = false;
// if passed a url, turn it back into a string
if(v === null || v === undefined) v = "";
if(v.dom$class == "URL" || v instanceof w.URL) v = v.toString();
if(typeof v != "string") return;
if(v.substr(0,7) == "Wp`Set@") v = v.substr(7), firstassign = true;
v = resolveURL(w.eb$base, v);
// return or blow up if v is not a url; not yet implemented
if(typeof this.href$val == "string") inconstruct = false;
if(inconstruct) {
Object.defineProperty(this, "href$val", {enumerable:false, writable:true, value:v});
Object.defineProperty(this, "protocol$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "hostname$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "host$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "port$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "pathname$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "search$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "hash$val", {enumerable:false, writable:true, value:""});
} else {
this.href$val = v;
this.port$val = this.protocol$val = this.host$val = this.hostname$val = this.pathname$val = this.search$val = this.hash$val = "";
}
if(v.match(/^[a-zA-Z]*:/)) {
this.protocol$val = v.replace(/:.*/, "");
this.protocol$val += ":";
v = v.replace(/^[a-zA-z]*:\/*/, "");
}
if(v.match(/[/#?]/)) {
/* contains / ? or # */
this.host$val = v.replace(/[/#?].*/, "");
v = v.replace(/^[^/#?]*/, "");
} else {
/* no / ? or #, the whole thing is the host, www.foo.bar */
this.host$val = v;
v = "";
}
// Watch out, ipv6 has : in the middle.
if(this.host$val.substr(0,1) == '[') { // I'll assume this is ipv6
if(this.host$val.match(/]:/)) {
this.hostname$val = this.host$val.replace(/]:.*/, "]");
this.port$val = this.host$val.replace(/^.*]:/, "");
} else {
this.hostname$val = this.host$val;
//this.port$val = setDefaultPort(this.protocol$val);
}
} else {
if(this.host$val.match(/:/)) {
this.hostname$val = this.host$val.replace(/:.*/, "");
this.port$val = this.host$val.replace(/^.*:/, "");
} else {
this.hostname$val = this.host$val;
//this.port$val = setDefaultPort(this.protocol$val);
}
}
// perhaps set protocol to http if it looks like a url?
// as in edbrowse foo.bar.com
// Ends in standard tld, or looks like an ip4 address, or starts with www.
if(this.protocol$val == "" &&
(this.hostname$val.match(/\.(com|org|net|info|biz|gov|edu|us|uk|ca|au)$/) ||
this.hostname$val.match(/^\d+\.\d+\.\d+\.\d+$/) ||
this.hostname$val.match(/^\[[\da-fA-F:]+]$/) ||
this.hostname$val.match(/^www\..*\.[a-zA-Z]{2,}$/))) {
this.protocol$val = "http:";
}
if(v.match(/[#?]/)) {
this.pathname$val = v.replace(/[#?].*/, "");
v = v.replace(/^[^#?]*/, "");
} else {
this.pathname$val = v;
v = "";
}
if(this.pathname$val == "")
this.pathname$val = "/";
if(v.match(/#/)) {
this.search$val = v.replace(/#.*/, "");
this.hash$val = v.replace(/^[^#]*/, "");
} else {
this.search$val = v;
}
if(!firstassign && this.eb$ctx) {
// replace the web page
eb$newLocation('r' + this.eb$ctx + this.href$val + '\n');
}
};

// sort some objects based on timestamp.
// There should only be a few, thus a bubble sort.
// If there are many, this will hang for a long time.
// Might have to write a native method to use qsort.
function sortTime(list) {
var l = list.length;
if(!l) return;
if(l > 20) alert3("sortTime with " + l + " objects");
var i, swap, change = true;
while(change) { change = false;
for(i=0; i<l-1; ++i)
if(list[i].timeStamp > list[i+1].timeStamp)
swap = list[i], list[i] = list[i+1], list[i+1] = swap, change = true;
}
}

DOMParser = function() {
return {
parseFromString: function(t,y) {
var d = my$doc();
if(y == "text/html" || y == "text/xml") {
var v = d.createElement("iframe");
if(t) {
if(typeof t == "string") {
if(y.match(/xml$/)) t = "`~*xml}@;" + t;
v.src = "data:" + y + "," + encodeURIComponent(t);
} else
alert3("DOMParser expects a string but gets " + typeof t);
}
// this expands the frame on demand
return v.contentDocument;
}
if(y == "text/plain") {
return d.createTextNode(t);
}
alert3("trying to use the DOMParser\n" + y + " <<< ");
alert4(t);
alert3(">>>");
return d.createTextNode("DOMParser not yet implemented");
}}}

// various XMLHttpRequest methods
function xml_open(method, url, async, user, password){
if(user || password) alert3("xml user and password ignored");
this.readyState = 1;
this.async = (async === false)?false:true;
this.method = method || "GET";
alert3("xhr " + (this.async ? "async " : "") + "open " + this.method + " " + url);
this.url = resolveURL(my$win().eb$base, url);
this.status = 0;
this.statusText = "";
// state = 1 and technically that's a change
// a website might use this to set something up, before send
// warning: if you don't call open, just set variables, this won't be called;
// but I think you're suppose to call open
if(typeof this.onreadystatechange == "function") this.onreadystatechange();
};

function xml_srh(header, value){
this.headers[header] = value;
};

function xml_grh(header){
var rHeader, returnedHeaders;
if (this.readyState < 3){
throw new Error("INVALID_STATE_ERR");
} else {
returnedHeaders = [];
for (rHeader in this.responseHeaders) {
if (rHeader.match(new RegExp(header, "i"))) {
returnedHeaders.push(this.responseHeaders[rHeader]);
}
}
if (returnedHeaders.length) return returnedHeaders.join(", ");
}
return null;
};

function xml_garh(){
var header, returnedHeaders = [];
if (this.readyState < 3){
throw new Error("INVALID_STATE_ERR");
} else {
for (header in this.responseHeaders)
returnedHeaders.push( header + ": " + this.responseHeaders[header] );
}
return returnedHeaders.join("\r\n");
};

function xml_send(data, parsedoc){
if(parsedoc) alert3("xml parsedoc ignored");
var w = my$win();
var headerstring = "";
for (var item in this.headers) {
var v1=item;
var v2=this.headers[item];
headerstring+=v1+': '+v2+'\n';
}
if(headerstring) alert3("xhr headers " + headerstring.replace(/\n$/,''));
var urlcopy = this.url;
if(urlcopy.match(/[*'";\[\]$\u0000-\u0020\u007f-\uffff]/)) {
alert3("xhr url does not look encoded");
// but assume it was anyways, cause it should be
//urlcopy = encodeURI(urlcopy);
}
if(data) {
alert3("xhr data " + data);
// no idea if data is already encoded or not.
/*
if(data.match(/[!*'";\[\]$\u0000-\u0020\u007f-\uffff]/)) {
alert3("xhr data was not encoded");
data = encodeURI(data);
}
*/
}
// check the sanity of data
if(data === null || data === undefined) data = "";
var td = typeof data;
var pd = 0; // how to process the data
if(td == "object" && data instanceof w.Uint8Array) {
pd = 1;
// Turn the byte array into utf8.
// code 0 becomes code 256, so we don't have a problem with null bytes.
var s="";
for(var i=0; i<data.length; ++i)
s += String.fromCharCode(data[i]?data[i]:256);
td = typeof (data = s);
}
// what do we do about Uint16Array and Uint32Array?
if(td != "string") {
alert3("payload data has improper type " + td);
}
this.$entire =  eb$fetchHTTP.call(this, urlcopy,this.method,headerstring,data, pd);
if(this.$entire != "async") this.parseResponse();
};

function xml_parse(){
var responsebody_array = this.$entire.split("\r\n\r\n");
var success = parseInt(responsebody_array[0]);
var code = parseInt(responsebody_array[1]);
var url2 = responsebody_array[2];
var http_headers = responsebody_array[3];
responsebody_array[0] = responsebody_array[1] = responsebody_array[2] = responsebody_array[3] = "";
this.responseText = responsebody_array.join("\r\n\r\n").trim();
// some want responseText, some just want response
this.response = this.responseText;
var hhc = http_headers.split(/\r?\n/);
for(var i=0; i<hhc.length; ++i) {
var value1 = hhc[i];
if(!value1.match(/:/)) continue;
var value2 = value1.split(":")[0];
var value3 = value1.split(":")[1];
this.responseHeaders[value2] = value3.trim();
}

this.readyState = 4;
this.responseURL = url2.replace(/#.*/,"");
if(success) {
this.status = code;
// need a real statusText for the codes
this.statusText = (code == 200 ? "OK" : "http error " + code);

// Should we run the xml parser if the status was not 200?
// And should we run it before the onreadystatechange function?
var ct = this.getResponseHeader("^content-type$");
if(!ct) ct = "text/xml"; // default
// if overrideMimeType called, should we replace it in headers, or just here?
if(this.eb$mt) ct = this.eb$mt;
if(ct) ct = ct.toLowerCase().replace(/;.*/,'');
if(code >= 200 && code < 300 && ct && (ct == "text/xml" || ct == "application/xml")) {
alert3("parsing the response as xml");
this.responseXML = (new (my$win().DOMParser)()).parseFromString(this.responseText, "text/xml");
}

// I'll do the load events, not loadstart or progress or loadend etc.
var w = my$win();
var e = new w.Event;
e.initEvent("load", true, true);
e.loaded = this.response.length;
this.dispatchEvent(e);
// I don't understand the upload object at all
this.upload.dispatchEvent(e);

// does anyone call addEventListener for readystatechange? Hope not.
if(typeof this.onreadystatechange == "function") this.onreadystatechange();
} else {
this.status = 0;
this.statusText = "network error";
}
};

// this is a minimal EventTarget class. It has the listeners but doesn't
// inherit all the stuff from Node, like it should.
// It is here so XMLHttpRequest can inherit its listeners.
function EventTarget(){}
EventTarget.prototype.eb$listen = eb$listen;
EventTarget.prototype.eb$unlisten = eb$unlisten;
EventTarget.prototype.addEventListener = function(ev, handler, iscapture) { this.eb$listen(ev,handler, iscapture, true); }
EventTarget.prototype.removeEventListener = function(ev, handler, iscapture) { this.eb$unlisten(ev,handler, iscapture, true); }
EventTarget.prototype.dispatchEvent = dispatchEvent;

function XMLHttpRequestEventTarget(){}
XMLHttpRequestEventTarget.prototype = new EventTarget;

function XMLHttpRequestUpload(){}
XMLHttpRequestUpload.prototype = new XMLHttpRequestEventTarget;

// Originally implemented by Yehuda Katz
// And since then, from envjs, by Thatcher et al
function XMLHttpRequest() {
    this.headers = {};
    this.responseHeaders = {};
    this.aborted = false;//non-standard
    this.withCredentials = true;
this.upload = new XMLHttpRequestUpload;
}
XMLHttpRequest.prototype = new EventTarget;
// defined by the standard: http://www.w3.org/TR/XMLHttpRequest/#xmlhttprequest
// but not provided by Firefox.  Safari and others do define it.
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPEN = 1;
XMLHttpRequest.HEADERS_RECEIVED = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;
XMLHttpRequest.prototype.toString = function(){return "[object XMLHttpRequest]"}
XMLHttpRequest.prototype.open = xml_open;
// FormData takes over this function, and sets _hasContentType
// if we are setting "Content-Type"
XMLHttpRequest.prototype.setRequestHeader = xml_srh;
XMLHttpRequest.prototype.getResponseHeader = xml_grh;
XMLHttpRequest.prototype.getAllResponseHeaders = xml_garh;
// FormData takes over this function and sends it a different way
//if the data is an instance of FormDataPolyfill
XMLHttpRequest.prototype.send = xml_send;
XMLHttpRequest.prototype.parseResponse = xml_parse;
XMLHttpRequest.prototype.overrideMimeType = function(t) {
if(typeof t == "string") this.eb$mt = t;
}
XMLHttpRequest.prototype.eb$mt = null;
XMLHttpRequest.prototype.async = false;
XMLHttpRequest.prototype.readyState = 0;
XMLHttpRequest.prototype.responseText = "";
XMLHttpRequest.prototype.response = "";
XMLHttpRequest.prototype.responseXML = null;
XMLHttpRequest.prototype.status = 0;
XMLHttpRequest.prototype.statusText = "";

CSS = {
supports:function(w){ alert3("CSS.supports("+w+")"); return false},
escape:function(s) {
if(typeof s == "number") s = s.toString();
if(typeof s != "string") return null;
return s.replace(/([\\()\[\]{}.#])/g, function(a,b){ return "\\"+b})
}}

// Internationalization method, this is a stub, English only.
// Only date and numbers, the most common.

function Intl_dt(w) {
alert3("Int_datetime("+w+")");
this.locale = w;
}
Object.defineProperty(Intl_dt.prototype, "format", {value:function(d) {
if(typeof d != "object") return ""
if(typeof d.getYear != "function") return ""
return `${d.getMonth()+1}/${d.getDate()}/${d.getYear()+1900}`;
}})

function Intl_num(w) {
alert3("Int_number("+w+")");
this.locale = w;
}
Object.defineProperty(Intl_num.prototype, "format", {value:function(n) {
if(typeof n != "number") return "NaN";
var sign = '';
if(n < 0) sign = '-', n = -n;
var m = Math.floor(n), r = n - m, s = "";
if(r) {
s = r + "", s = s.substr(1);
// lots of possible round off errors here
// 37.40000000000238 becomes 37.4
s = s.replace(/\.*000000.*/, "")
// 54.99999999373 becomes 55
if(s.match(/^\.999999/))
s = "", ++m;
// 37.39999999378 becomes 37.4, not so easy to do
if(s.match(/999999/)) {
s = s.replace(/999999.*/, "")
var l = s.length - 1;
s = s.substr(0,l) + (1 + parseInt(s.substr(l)));
}
}
while(m >= 1000) {
r = m % 1000;
r = r + "";
while(r.length < 3) r = '0' + r;
s = ',' + r + s;
m = Math.floor(m/1000)
}
return sign + m + s;
}})

function Intl() {}
Object.defineProperty(Intl, "DateTimeFormat", {value:Intl_dt})
Object.defineProperty(Intl, "NumberFormat", {value:Intl_num})

// Code beyond this point is third party, but necessary for the operation of the browser.

// NextSection
// TextDecoder TextEncoder   https://github.com/anonyco/FastestSmallestTextEncoderDecoder
// There is a minimized version, which I don't use here.

/** @define {boolean} */
var ENCODEINTO_BUILD = false;

(function(window){
	"use strict";
	//var log = Math.log;
	//var LN2 = Math.LN2;
	//var clz32 = Math.clz32 || function(x) {return 31 - log(x >> 0) / LN2 | 0};
	var fromCharCode = String.fromCharCode;
	var Object_prototype_toString = ({}).toString;
	var sharedArrayBufferString = Object_prototype_toString.call(window["SharedArrayBuffer"]);
	var undefinedObjectString = Object_prototype_toString();
	var NativeUint8Array = window.Uint8Array;
	var patchedU8Array = NativeUint8Array || Array;
	var nativeArrayBuffer = NativeUint8Array ? ArrayBuffer : patchedU8Array;
	var arrayBuffer_isView = nativeArrayBuffer.isView || function(x) {return x && "length" in x};
	var arrayBufferString = Object_prototype_toString.call(nativeArrayBuffer.prototype);
	var window_encodeURIComponent = encodeURIComponent;
	var window_parseInt = parseInt;
	var TextEncoderPrototype = TextEncoder["prototype"];
	var GlobalTextEncoder = window["TextEncoder"];
	var decoderRegexp = /[\xc0-\xff][\x80-\xbf]+|[\x80-\xff]/g;
	var encoderRegexp = /[\x80-\uD7ff\uDC00-\uFFFF]|[\uD800-\uDBFF][\uDC00-\uDFFF]?/g;
	var tmpBufferU16 = new (NativeUint8Array ? Uint16Array : patchedU8Array)(32);
	var globalTextEncoderPrototype;
	var globalTextEncoderInstance;
	
	/*function decoderReplacer(encoded) {
		var cp0 = encoded.charCodeAt(0), codePoint=0x110000, i=0, stringLen=encoded.length|0, result="";
		switch(cp0 >> 4) {
			// no 1 byte sequences
			case 12:
			case 13:
				codePoint = ((cp0 & 0x1F) << 6) | (encoded.charCodeAt(1) & 0x3F);
				i = codePoint < 0x80 ? 0 : 2;
				break;
			case 14:
				codePoint = ((cp0 & 0x0F) << 12) | ((encoded.charCodeAt(1) & 0x3F) << 6) | (encoded.charCodeAt(2) & 0x3F);
				i = codePoint < 0x800 ? 0 : 3;
				break;
			case 15:
				if ((cp0 >> 3) === 30) {
					codePoint = ((cp0 & 0x07) << 18) | ((encoded.charCodeAt(1) & 0x3F) << 12) | ((encoded.charCodeAt(2) & 0x3F) << 6) | (encoded.charCodeAt(3) & 0x3F);
					i = codePoint < 0x10000 ? 0 : 4;
				}
		}
		if (i) {
		    if (stringLen < i) {
		    	i = 0;
		    } else if (codePoint < 0x10000) { // BMP code point
				result = fromCharCode(codePoint);
			} else if (codePoint < 0x110000) {
				codePoint = codePoint - 0x10080|0;//- 0x10000|0;
				result = fromCharCode(
					(codePoint >> 10) + 0xD800|0,  // highSurrogate
					(codePoint & 0x3ff) + 0xDC00|0 // lowSurrogate
				);
			} else i = 0; // to fill it in with INVALIDs
		}
		
		for (; i < stringLen; i=i+1|0) result += "\ufffd"; // fill rest with replacement character
		
		return result;
	}*/
	function TextDecoder(){};
	TextDecoder["prototype"]["decode"] = function(inputArrayOrBuffer){
		var inputAs8 = inputArrayOrBuffer, asObjectString;
		if (!arrayBuffer_isView(inputAs8)) {
			asObjectString = Object_prototype_toString.call(inputAs8);
			if (asObjectString !== arrayBufferString && asObjectString !== sharedArrayBufferString && asObjectString !== undefinedObjectString)
				throw TypeError("Failed to execute 'decode' on 'TextDecoder': The provided value is not of type '(ArrayBuffer or ArrayBufferView)'");
			inputAs8 = NativeUint8Array ? new patchedU8Array(inputAs8) : inputAs8 || [];
		}
		
		var resultingString="", tmpStr="", index=0, len=inputAs8.length|0, lenMinus32=len-32|0, nextEnd=0, nextStop=0, cp0=0, codePoint=0, minBits=0, cp1=0, pos=0, tmp=-1;
		// Note that tmp represents the 2nd half of a surrogate pair incase a surrogate gets divided between blocks
		for (; index < len; ) {
			nextEnd = index <= lenMinus32 ? 32 : len - index|0;
			for (; pos < nextEnd; index=index+1|0, pos=pos+1|0) {
				cp0 = inputAs8[index] & 0xff;
				switch(cp0 >> 4) {
					case 15:
						cp1 = inputAs8[index=index+1|0] & 0xff;
						if ((cp1 >> 6) !== 0b10 || 0b11110111 < cp0) {
							index = index - 1|0;
							break;
						}
						codePoint = ((cp0 & 0b111) << 6) | (cp1 & 0b00111111);
						minBits = 5; // 20 ensures it never passes -> all invalid replacements
						cp0 = 0x100; //  keep track of th bit size
					case 14:
						cp1 = inputAs8[index=index+1|0] & 0xff;
						codePoint <<= 6;
						codePoint |= ((cp0 & 0b1111) << 6) | (cp1 & 0b00111111);
						minBits = (cp1 >> 6) === 0b10 ? minBits + 4|0 : 24; // 24 ensures it never passes -> all invalid replacements
						cp0 = (cp0 + 0x100) & 0x300; // keep track of th bit size
					case 13:
					case 12:
						cp1 = inputAs8[index=index+1|0] & 0xff;
						codePoint <<= 6;
						codePoint |= ((cp0 & 0b11111) << 6) | cp1 & 0b00111111;
						minBits = minBits + 7|0;
						
						// Now, process the code point
						if (index < len && (cp1 >> 6) === 0b10 && (codePoint >> minBits) && codePoint < 0x110000) {
							cp0 = codePoint;
							codePoint = codePoint - 0x10000|0;
							if (0 <= codePoint/*0xffff < codePoint*/) { // BMP code point
								//nextEnd = nextEnd - 1|0;
								
								tmp = (codePoint >> 10) + 0xD800|0;   // highSurrogate
								cp0 = (codePoint & 0x3ff) + 0xDC00|0; // lowSurrogate (will be inserted later in the switch-statement)
								
								if (pos < 31) { // notice 31 instead of 32
									tmpBufferU16[pos] = tmp;
									pos = pos + 1|0;
									tmp = -1;
								}  else {// else, we are at the end of the inputAs8 and let tmp0 be filled in later on
									// NOTE that cp1 is being used as a temporary variable for the swapping of tmp with cp0
									cp1 = tmp;
									tmp = cp0;
									cp0 = cp1;
								}
							} else nextEnd = nextEnd + 1|0; // because we are advancing i without advancing pos
						} else {
							// invalid code point means replacing the whole thing with null replacement characters
							cp0 >>= 8;
							index = index - cp0 - 1|0; // reset index  back to what it was before
							cp0 = 0xfffd;
						}
						
						
						// Finally, reset the variables for the next go-around
						minBits = 0;
						codePoint = 0;
						nextEnd = index <= lenMinus32 ? 32 : len - index|0;
					/*case 11:
					case 10:
					case 9:
					case 8:
						codePoint ? codePoint = 0 : cp0 = 0xfffd; // fill with invalid replacement character
					case 7:
					case 6:
					case 5:
					case 4:
					case 3:
					case 2:
					case 1:
					case 0:
						tmpBufferU16[pos] = cp0;
						continue;*/
					default:
						tmpBufferU16[pos] = cp0; // fill with invalid replacement character
						continue;
					case 11:
					case 10:
					case 9:
					case 8:
				}
				tmpBufferU16[pos] = 0xfffd; // fill with invalid replacement character
			}
			tmpStr += fromCharCode(
				tmpBufferU16[ 0], tmpBufferU16[ 1], tmpBufferU16[ 2], tmpBufferU16[ 3], tmpBufferU16[ 4], tmpBufferU16[ 5], tmpBufferU16[ 6], tmpBufferU16[ 7],
				tmpBufferU16[ 8], tmpBufferU16[ 9], tmpBufferU16[10], tmpBufferU16[11], tmpBufferU16[12], tmpBufferU16[13], tmpBufferU16[14], tmpBufferU16[15],
				tmpBufferU16[16], tmpBufferU16[17], tmpBufferU16[18], tmpBufferU16[19], tmpBufferU16[20], tmpBufferU16[21], tmpBufferU16[22], tmpBufferU16[23],
				tmpBufferU16[24], tmpBufferU16[25], tmpBufferU16[26], tmpBufferU16[27], tmpBufferU16[28], tmpBufferU16[29], tmpBufferU16[30], tmpBufferU16[31]
			);
			if (pos < 32) tmpStr = tmpStr.slice(0, pos-32|0);//-(32-pos));
			if (index < len) {
				//fromCharCode.apply(0, tmpBufferU16 : NativeUint8Array ?  tmpBufferU16.subarray(0,pos) : tmpBufferU16.slice(0,pos));
				tmpBufferU16[0] = tmp;
				pos = (~tmp) >>> 31;//tmp !== -1 ? 1 : 0;
				tmp = -1;
				
				if (tmpStr.length < resultingString.length) continue;
			} else if (tmp !== -1) {
				tmpStr += fromCharCode(tmp);
			}
			
			resultingString += tmpStr;
			tmpStr = "";
		}

		return resultingString;
	}
	//////////////////////////////////////////////////////////////////////////////////////
	function encoderReplacer(nonAsciiChars){
		// make the UTF string into a binary UTF-8 encoded string
		var point = nonAsciiChars.charCodeAt(0)|0;
		if (0xD800 <= point) {
			if (point <= 0xDBFF) {
				var nextcode = nonAsciiChars.charCodeAt(1)|0; // defaults to 0 when NaN, causing null replacement character
				
				if (0xDC00 <= nextcode && nextcode <= 0xDFFF) {
					//point = ((point - 0xD800)<<10) + nextcode - 0xDC00 + 0x10000|0;
					point = (point<<10) + nextcode - 0x35fdc00|0;
					if (point > 0xffff)
						return fromCharCode(
							(0x1e/*0b11110*/<<3) | (point>>18),
							(0x2/*0b10*/<<6) | ((point>>12)&0x3f/*0b00111111*/),
							(0x2/*0b10*/<<6) | ((point>>6)&0x3f/*0b00111111*/),
							(0x2/*0b10*/<<6) | (point&0x3f/*0b00111111*/)
						);
				} else point = 65533/*0b1111111111111101*/;//return '\xEF\xBF\xBD';//fromCharCode(0xef, 0xbf, 0xbd);
			} else if (point <= 0xDFFF) {
				point = 65533/*0b1111111111111101*/;//return '\xEF\xBF\xBD';//fromCharCode(0xef, 0xbf, 0xbd);
			}
		}
		/*if (point <= 0x007f) return nonAsciiChars;
		else */if (point <= 0x07ff) {
			return fromCharCode((0x6<<5)|(point>>6), (0x2<<6)|(point&0x3f));
		} else return fromCharCode(
			(0xe/*0b1110*/<<4) | (point>>12),
			(0x2/*0b10*/<<6) | ((point>>6)&0x3f/*0b00111111*/),
			(0x2/*0b10*/<<6) | (point&0x3f/*0b00111111*/)
		);
	}
	function TextEncoder(){};
	TextEncoderPrototype["encode"] = function(inputString){
		// 0xc0 => 0b11000000; 0xff => 0b11111111; 0xc0-0xff => 0b11xxxxxx
		// 0x80 => 0b10000000; 0xbf => 0b10111111; 0x80-0xbf => 0b10xxxxxx
		var encodedString = inputString === void 0 ? "" : ("" + inputString), len=encodedString.length|0;
		var result=new patchedU8Array((len << 1) + 8|0), tmpResult;
		var i=0, pos=0, point=0, nextcode=0;
		var upgradededArraySize=!NativeUint8Array; // normal arrays are auto-expanding
		for (i=0; i<len; i=i+1|0, pos=pos+1|0) {
			point = encodedString.charCodeAt(i)|0;
			if (point <= 0x007f) {
				result[pos] = point;
			} else if (point <= 0x07ff) {
				result[pos] = (0x6<<5)|(point>>6);
				result[pos=pos+1|0] = (0x2<<6)|(point&0x3f);
			} else {
				widenCheck: {
					if (0xD800 <= point) {
						if (point <= 0xDBFF) {
							nextcode = encodedString.charCodeAt(i=i+1|0)|0; // defaults to 0 when NaN, causing null replacement character
							
							if (0xDC00 <= nextcode && nextcode <= 0xDFFF) {
								//point = ((point - 0xD800)<<10) + nextcode - 0xDC00 + 0x10000|0;
								point = (point<<10) + nextcode - 0x35fdc00|0;
								if (point > 0xffff) {
									result[pos] = (0x1e/*0b11110*/<<3) | (point>>18);
									result[pos=pos+1|0] = (0x2/*0b10*/<<6) | ((point>>12)&0x3f/*0b00111111*/);
									result[pos=pos+1|0] = (0x2/*0b10*/<<6) | ((point>>6)&0x3f/*0b00111111*/);
									result[pos=pos+1|0] = (0x2/*0b10*/<<6) | (point&0x3f/*0b00111111*/);
									continue;
								}
								break widenCheck;
							}
							point = 65533/*0b1111111111111101*/;//return '\xEF\xBF\xBD';//fromCharCode(0xef, 0xbf, 0xbd);
						} else if (point <= 0xDFFF) {
							point = 65533/*0b1111111111111101*/;//return '\xEF\xBF\xBD';//fromCharCode(0xef, 0xbf, 0xbd);
						}
					}
					if (!upgradededArraySize && (i << 1) < pos && (i << 1) < (pos - 7|0)) {
						upgradededArraySize = true;
						tmpResult = new patchedU8Array(len * 3);
						tmpResult.set( result );
						result = tmpResult;
					}
				}
				result[pos] = (0xe/*0b1110*/<<4) | (point>>12);
				result[pos=pos+1|0] =(0x2/*0b10*/<<6) | ((point>>6)&0x3f/*0b00111111*/);
				result[pos=pos+1|0] =(0x2/*0b10*/<<6) | (point&0x3f/*0b00111111*/);
			}
		}
		return NativeUint8Array ? result.subarray(0, pos) : result.slice(0, pos);
	};
	function polyfill_encodeInto(inputString, u8Arr) {
		var encodedString = inputString === void 0 ?  "" : ("" + inputString).replace(encoderRegexp, encoderReplacer);
		var len=encodedString.length|0, i=0, char=0, read=0, u8ArrLen = u8Arr.length|0, inputLength=inputString.length|0;
		if (u8ArrLen < len) len=u8ArrLen;
		putChars: {
			for (; i<len; i=i+1|0) {
				char = encodedString.charCodeAt(i) |0;
				switch(char >> 4) {
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
						read = read + 1|0;
						// extension points:
					case 8:
					case 9:
					case 10:
					case 11:
						break;
					case 12:
					case 13:
						if ((i+1|0) < u8ArrLen) {
							read = read + 1|0;
							break;
						}
					case 14:
						if ((i+2|0) < u8ArrLen) {
							//if (!(char === 0xEF && encodedString.substr(i+1|0,2) === "\xBF\xBD"))
							read = read + 1|0;
							break;
						}
					case 15:
						if ((i+3|0) < u8ArrLen) {
							read = read + 1|0;
							break;
						}
					default:
						break putChars;
				}
				//read = read + ((char >> 6) !== 2) |0;
				u8Arr[i] = char;
			}
		}
		return {"written": i, "read": inputLength < read ? inputLength : read};
		// 0xc0 => 0b11000000; 0xff => 0b11111111; 0xc0-0xff => 0b11xxxxxx
		// 0x80 => 0b10000000; 0xbf => 0b10111111; 0x80-0xbf => 0b10xxxxxx
		/*var encodedString = typeof inputString == "string" ? inputString : inputString === void 0 ?  "" : "" + inputString;
		var encodedLen = encodedString.length|0, u8LenLeft=u8Arr.length|0;
		var i=-1, read=-1, code=0, point=0, nextcode=0;
		tryFast: if (2 < encodedLen && encodedLen < (u8LenLeft >> 1)) {
			// Skip the normal checks because we can almost certainly fit the string inside the existing buffer
			while (1) {		// make the UTF string into a binary UTF-8 encoded string
				point = encodedString.charCodeAt(read = read + 1|0)|0;
				
				if (point <= 0x007f) {
					if (point === 0 && encodedLen <= read) {
						read = read - 1|0;
						break; // we have reached the end of the string
					}
					u8Arr[i=i+1|0] = point;
				} else if (point <= 0x07ff) {
					u8Arr[i=i+1|0] = (0x6<<5)|(point>>6);
					u8Arr[i=i+1|0] = (0x2<<6)|(point&0x3f);
				} else {
					if (0xD800 <= point && point <= 0xDBFF) {
						nextcode = encodedString.charCodeAt(read)|0; // defaults to 0 when NaN, causing null replacement character
						
						if (0xDC00 <= nextcode && nextcode <= 0xDFFF) {
							read = read + 1|0;
							//point = ((point - 0xD800)<<10) + nextcode - 0xDC00 + 0x10000|0;
							point = (point<<10) + nextcode - 0x35fdc00|0;
							if (point > 0xffff) {
								u8Arr[i=i+1|0] = (0x1e<<3) | (point>>18);
								u8Arr[i=i+1|0] = (0x2<<6) | ((point>>12)&0x3f);
								u8Arr[i=i+1|0] = (0x2<<6) | ((point>>6)&0x3f);
								u8Arr[i=i+1|0] = (0x2<<6) | (point&0x3f);
								continue;
							}
						} else if (nextcode === 0 && encodedLen <= read) {
							break; // we have reached the end of the string
						} else {
							point = 65533;//0b1111111111111101; // invalid replacement character
						}
					}
					u8Arr[i=i+1|0] = (0xe<<4) | (point>>12);
					u8Arr[i=i+1|0] = (0x2<<6) | ((point>>6)&0x3f);
					u8Arr[i=i+1|0] = (0x2<<6) | (point&0x3f);
					if (u8LenLeft < (i + ((encodedLen - read) << 1)|0)) {
						// These 3x chars are the only way to inflate the size to 3x
						u8LenLeft = u8LenLeft - i|0;
						break tryFast;
					}
				}
			}
			u8LenLeft = 0; // skip the next for-loop 
		}
		
		
		for (; 0 < u8LenLeft; ) {		// make the UTF string into a binary UTF-8 encoded string
			point = encodedString.charCodeAt(read = read + 1|0)|0;
			
			if (point <= 0x007f) {
				if (point === 0 && encodedLen <= read) {
					read = read - 1|0;
					break; // we have reached the end of the string
				}
				u8LenLeft = u8LenLeft - 1|0;
				u8Arr[i=i+1|0] = point;
			} else if (point <= 0x07ff) {
				u8LenLeft = u8LenLeft - 2|0;
				if (0 <= u8LenLeft) {
					u8Arr[i=i+1|0] = (0x6<<5)|(point>>6);
					u8Arr[i=i+1|0] = (0x2<<6)|(point&0x3f);
				}
			} else {
				if (0xD800 <= point && point <= 0xDBFF) {
					nextcode = encodedString.charCodeAt(read = read + 1|0)|0; // defaults to 0 when NaN, causing null replacement character
					
					if (0xDC00 <= nextcode) {
						if (nextcode <= 0xDFFF) {
							read = read + 1|0;
							//point = ((point - 0xD800)<<10) + nextcode - 0xDC00 + 0x10000|0;
							point = (point<<10) + nextcode - 0x35fdc00|0;
							if (point > 0xffff) {
								u8LenLeft = u8LenLeft - 4|0;
								if (0 <= u8LenLeft) {
									u8Arr[i=i+1|0] = (0x1e<<3) | (point>>18);
									u8Arr[i=i+1|0] = (0x2<<6) | ((point>>12)&0x3f);
									u8Arr[i=i+1|0] = (0x2<<6) | ((point>>6)&0x3f);
									u8Arr[i=i+1|0] = (0x2<<6) | (point&0x3f);
								}
								continue;
							}
						} else if (point <= 0xDFFF) {
							point = 65533/*0b1111111111111101*\/;//return '\xEF\xBF\xBD';//fromCharCode(0xef, 0xbf, 0xbd);
						}
					} else if (nextcode === 0 && encodedLen <= read) {
						break; // we have reached the end of the string
					} else {
						point = 65533;//0b1111111111111101; // invalid replacement character
					}
				}
				u8LenLeft = u8LenLeft - 3|0;
				if (0 <= u8LenLeft) {
					u8Arr[i=i+1|0] = (0xe<<<4) | (point>>12);
					u8Arr[i=i+1|0] = (0x2<<6) | ((point>>6)&0x3f);
					u8Arr[i=i+1|0] = (0x2<<6) | (point&0x3f);
				}
			}
		} 
		return {"read": read < 0 ? 0 : u8LenLeft < 0 ? read : read+1|0, "written": i < 0 ? 0 : i+1|0};*/
	};
	if (ENCODEINTO_BUILD) {
		TextEncoderPrototype["encodeInto"] = polyfill_encodeInto;
	}
	
	if (!GlobalTextEncoder) {
		window["TextDecoder"] = TextDecoder;
		window["TextEncoder"] = TextEncoder;
	} else if (ENCODEINTO_BUILD && !(globalTextEncoderPrototype = GlobalTextEncoder["prototype"])["encodeInto"]) {
		globalTextEncoderInstance = new GlobalTextEncoder;
		globalTextEncoderPrototype["encodeInto"] = function(string, u8arr) {
			// Unfortunately, there's no way I can think of to quickly extract the number of bits written and the number of bytes read and such
			var strLen = string.length|0, u8Len = u8arr.length|0;
			if (strLen < (u8Len >> 1)) { // in most circumstances, this means its safe. there are still edge-cases which are possible
				// in many circumstances, we can use the faster native TextEncoder
				var res8 = globalTextEncoderInstance["encode"](string);
				var res8Len = res8.length|0;
				if (res8Len < u8Len) { // if we dont have to worry about read/written
					u8arr.set( res8 ); // every browser that supports TextEncoder also supports typedarray.prototype.set
					return {
						"read": strLen,
						"written": res8.length|0
					};
				}
			}
			return polyfill_encodeInto(string, u8arr);
		};
	}
})(typeof global == "" + void 0 ? typeof self == "" + void 0 ? this : self : global);

// NextSection
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
      var i = chunks.length, l
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
      return my$win().Promise.resolve(this._buffer)
    }

    Blob.prototype.text = function () {
      return my$win().Promise.resolve(textDecode(this._buffer))
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
    return new my$win().Promise(function(resolve, reject) {
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

// NextSection
// Ok this is my function, but it blends with the message port functions below.
function onmessage$$running() { 
if(this.eb$pause && !this.onmessage) return;
if(this.onmessage || (this.onmessage$$array && this.onmessage$$array.length)) { // handlers are ready
while(this.onmessage$$queue.length) {
// better run messages fifo
var me = this.onmessage$$queue[0];
this.onmessage$$queue.splice(0, 1);
// if you then add another handler, it won't run on this message.
// I assume you add all the handlers you wish in one go,
// then they process each message in the queue, and each message going forward.
var datashow = me.data;
var datalength = 0;
if(typeof me.data == "string") datalength = me.data.length;
if(datalength >= 200) datashow = "long";
alert3(this.nodeName + " context " + this.eb$ctx + " processes message of length " + datalength + " ↑" +
datashow + "↑");
if(this.onmessage)
this.onmessage(me);
else
this.onmessage$$fn(me);
alert3("process message complete");
}
}
}

// NextSection
/*********************************************************************
MessagePort and MessageChannel
https://github.com/rocwind/message-port-polyfill
MIT license.
These are considerably modified for our purposes.
*********************************************************************/

MessagePort = /** @class */ (function () {
function MessagePort() {
var w = my$win();
this.onmessage = null;
this.onmessageerror = null;
this.otherPort = null;
this.onmessage$$queue = [];
this.eb$ctx = w.eb$ctx;
this.eb$pause = true;
w.mp$registry.push(this);
}
var p = MessagePort.prototype;
p.nodeName = "PORT";
p.onmessage$$running = onmessage$$running;
p.dispatchEvent = function (me) {
me.name = me.type = "message";
// me.data is already set
this.onmessage$$queue.push(me);
var datashow = me.data;
var datalength = 0;
if(typeof me.data == "string") datalength = me.data.length;
if(datalength >= 200) datashow = "long";
alert3("posting message of length " + datalength + " to port context " + this.eb$ctx + " ↑" +
datashow + "↑");
return true;
};
p.postMessage = function (message) {
if (this.otherPort) this.otherPort.dispatchEvent({ data: message });
};
p.eb$listen = eb$listen;
p.eb$unlisten = eb$unlisten;
p.addEventListener = function(ev, handler, iscapture) { this.eb$listen(ev,handler, iscapture, true); }
p.removeEventListener = function(ev, handler, iscapture) { this.eb$unlisten(ev,handler, iscapture, true); }
p.start = function () {
this.eb$pause = false;
alert3("MessagePort start for context " + this.eb$ctx);
};
p.close = function () {
this.eb$pause = true;
alert3("MessagePort start for context " + this.eb$ctx);
};
return MessagePort;
}());
MessageChannel = /** @class */ (function () {
function MessageChannel() {
this.port1 = new MessagePort();
this.port2 = new MessagePort();
this.port1.otherPort = this.port2;
this.port2.otherPort = this.port1;
}
return MessageChannel;
}());

// NextSection
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

// NextSection
// https://raw.githubusercontent.com/jimmywarting/FormData/master/FormData.js
// jimmy@warting.se

/* formdata-polyfill. MIT License. Jimmy Wärting <https://jimmy.warting.se/opensource> */

/* global FormData self Blob File */
/* eslint-disable no-inner-declarations */

if (typeof Blob !== 'undefined' && (typeof FormData === 'undefined' || !FormData.prototype.keys)) {
  const global = typeof globalThis === 'object'
    ? globalThis
    : typeof window === 'object'
      ? window
      : typeof self === 'object' ? self : this

  // keep a reference to native implementation
  const _FormData = global.FormData

  // To be monkey patched
  const _send = global.XMLHttpRequest && global.XMLHttpRequest.prototype.send
  const _fetch = global.Request && global.fetch
  const _sendBeacon = global.navigator && global.navigator.sendBeacon
  // Might be a worker thread...
  const _match = global.Element && global.Element.prototype

  // Unable to patch Request/Response constructor correctly #109
  // only way is to use ES6 class extend
  // https://github.com/babel/babel/issues/1966

  const stringTag = global.Symbol && Symbol.toStringTag

  // Add missing stringTags to blob and files
  if (stringTag) {
    if (!Blob.prototype[stringTag]) {
      Blob.prototype[stringTag] = 'Blob'
    }

    if ('File' in global && !File.prototype[stringTag]) {
      File.prototype[stringTag] = 'File'
    }
  }

  // Fix so you can construct your own File
  try {
    new File([], '') // eslint-disable-line
  } catch (a) {
    global.File = function File (b, d, c) {
      const blob = new Blob(b, c || {})
      const t = c && void 0 !== c.lastModified ? new Date(c.lastModified) : new Date()

      Object.defineProperties(blob, {
        name: {
          value: d
        },
        lastModified: {
          value: +t
        },
        toString: {
          value () {
            return '[object File]'
          }
        }
      })

      if (stringTag) {
        Object.defineProperty(blob, stringTag, {
          value: 'File'
        })
      }

      return blob
    }
  }

  function ensureArgs (args, expected) {
    if (args.length < expected) {
      throw new TypeError(`${expected} argument required, but only ${args.length} present.`)
    }
  }

  /**
   * @param {string} name
   * @param {string | undefined} filename
   * @returns {[string, File|string]}
   */
  function normalizeArgs (name, value, filename) {
    if (value instanceof Blob) {
      filename = filename !== undefined
      ? String(filename + '')
      : typeof value.name === 'string'
      ? value.name
      : 'blob'

      if (value.name !== filename || Object.prototype.toString.call(value) === '[object Blob]') {
        value = new File([value], filename)
      }
      return [String(name), value]
    }
    return [String(name), String(value)]
  }

  // normalize line feeds for textarea
  // https://html.spec.whatwg.org/multipage/form-elements.html#textarea-line-break-normalisation-transformation
  function normalizeLinefeeds (value) {
    return value.replace(/\r?\n|\r/g, '\r\n')
  }

  /**
   * @template T
   * @param {ArrayLike<T>} arr
   * @param {{ (elm: T): void; }} cb
   */
  function each (arr, cb) {
    for (let i = 0; i < arr.length; i++) {
      cb(arr[i])
    }
  }

  const escape = str => str.replace(/\n/g, '%0A').replace(/\r/g, '%0D').replace(/"/g, '%22')

  /**
   * @implements {Iterable}
   */
  class FormDataPolyfill {
    /**
     * FormData class
     *
     * @param {HTMLFormElement=} form
     */
    constructor (form) {
      /** @type {[string, string|File][]} */
      this._data = []

      const self = this
      form && each(form.elements, (/** @type {HTMLInputElement} */ elm) => {
        if (
          !elm.name ||
          elm.disabled ||
          elm.type === 'submit' ||
          elm.type === 'button' ||
          elm.matches('form fieldset[disabled] *')
        ) return

        if (elm.type === 'file') {
          const files = elm.files && elm.files.length
            ? elm.files
            : [new File([], '', { type: 'application/octet-stream' })] // #78

          each(files, file => {
            self.append(elm.name, file)
          })
        } else if (elm.type === 'select-multiple' || elm.type === 'select-one') {
          each(elm.options, opt => {
            !opt.disabled && opt.selected && self.append(elm.name, opt.value)
          })
        } else if (elm.type === 'checkbox' || elm.type === 'radio') {
          if (elm.checked) self.append(elm.name, elm.value)
        } else {
          const value = elm.type === 'textarea' ? normalizeLinefeeds(elm.value) : elm.value
          self.append(elm.name, value)
        }
      })
    }

    /**
     * Append a field
     *
     * @param   {string}           name      field name
     * @param   {string|Blob|File} value     string / blob / file
     * @param   {string=}          filename  filename to use with blob
     * @return  {undefined}
     */
    append (name, value, filename) {
      ensureArgs(arguments, 2)
      this._data.push(normalizeArgs(name, value, filename))
    }

    /**
     * Delete all fields values given name
     *
     * @param   {string}  name  Field name
     * @return  {undefined}
     */
    delete (name) {
      ensureArgs(arguments, 1)
      const result = []
      name = String(name)

      each(this._data, entry => {
        entry[0] !== name && result.push(entry)
      })

      this._data = result
    }

    /**
     * Iterate over all fields as [name, value]
     *
     * @return {Iterator}
     */
    * entries () {
      for (var i = 0; i < this._data.length; i++) {
        yield this._data[i]
      }
    }

    /**
     * Iterate over all fields
     *
     * @param   {Function}  callback  Executed for each item with parameters (value, name, thisArg)
     * @param   {Object=}   thisArg   `this` context for callback function
     */
    forEach (callback, thisArg) {
      ensureArgs(arguments, 1)
      for (const [name, value] of this) {
        callback.call(thisArg, value, name, this)
      }
    }

    /**
     * Return first field value given name
     * or null if non existent
     *
     * @param   {string}  name      Field name
     * @return  {string|File|null}  value Fields value
     */
    get (name) {
      ensureArgs(arguments, 1)
      const entries = this._data
      name = String(name)
      for (let i = 0; i < entries.length; i++) {
        if (entries[i][0] === name) {
          return entries[i][1]
        }
      }
      return null
    }

    /**
     * Return all fields values given name
     *
     * @param   {string}  name  Fields name
     * @return  {Array}         [{String|File}]
     */
    getAll (name) {
      ensureArgs(arguments, 1)
      const result = []
      name = String(name)
      each(this._data, data => {
        data[0] === name && result.push(data[1])
      })

      return result
    }

    /**
     * Check for field name existence
     *
     * @param   {string}   name  Field name
     * @return  {boolean}
     */
    has (name) {
      ensureArgs(arguments, 1)
      name = String(name)
      for (let i = 0; i < this._data.length; i++) {
        if (this._data[i][0] === name) {
          return true
        }
      }
      return false
    }

    /**
     * Iterate over all fields name
     *
     * @return {Iterator}
     */
    * keys () {
      for (const [name] of this) {
        yield name
      }
    }

    /**
     * Overwrite all values given name
     *
     * @param   {string}    name      Filed name
     * @param   {string}    value     Field value
     * @param   {string=}   filename  Filename (optional)
     */
    set (name, value, filename) {
      ensureArgs(arguments, 2)
      name = String(name)
      /** @type {[string, string|File][]} */
      const result = []
      const args = normalizeArgs(name, value, filename)
      let replace = true

      // - replace the first occurrence with same name
      // - discards the remaining with same name
      // - while keeping the same order items where added
      each(this._data, data => {
        data[0] === name
          ? replace && (replace = !result.push(args))
          : result.push(data)
      })

      replace && result.push(args)

      this._data = result
    }

    /**
     * Iterate over all fields
     *
     * @return {Iterator}
     */
    * values () {
      for (const [, value] of this) {
        yield value
      }
    }

    /**
     * Return a native (perhaps degraded) FormData with only a `append` method
     * Can throw if it's not supported
     *
     * @return {FormData}
     */
    ['_asNative'] () {
      const fd = new _FormData()

      for (const [name, value] of this) {
        fd.append(name, value)
      }

      return fd
    }

    /**
     * [_blob description]
     *
     * @return {Blob} [description]
     */
    ['_blob'] () {
        const boundary = '----formdata-polyfill-' + Math.random(),
          chunks = [],
          p = `--${boundary}\r\nContent-Disposition: form-data; name="`
        this.forEach((value, name) => typeof value == 'string'
          ? chunks.push(p + escape(normalizeLinefeeds(name)) + `"\r\n\r\n${normalizeLinefeeds(value)}\r\n`)
          : chunks.push(p + escape(normalizeLinefeeds(name)) + `"; filename="${escape(value.name)}"\r\nContent-Type: ${value.type||"application/octet-stream"}\r\n\r\n`, value, `\r\n`))
        chunks.push(`--${boundary}--`)
        return new Blob(chunks, {
          type: "multipart/form-data; boundary=" + boundary
        })
    }

    /**
     * The class itself is iterable
     * alias for formdata.entries()
     *
     * @return {Iterator}
     */
    [Symbol.iterator] () {
      return this.entries()
    }

    /**
     * Create the default string description.
     *
     * @return  {string} [object FormData]
     */
    toString () {
      return '[object FormData]'
    }
  }

  if (_match && !_match.matches) {
    _match.matches =
      _match.matchesSelector ||
      _match.mozMatchesSelector ||
      _match.msMatchesSelector ||
      _match.oMatchesSelector ||
      _match.webkitMatchesSelector ||
      function (s) {
        var matches = (this.document || this.ownerDocument).querySelectorAll(s)
        var i = matches.length
        while (--i >= 0 && matches.item(i) !== this) {}
        return i > -1
      }
  }

  if (stringTag) {
    /**
     * Create the default string description.
     * It is accessed internally by the Object.prototype.toString().
     */
    FormDataPolyfill.prototype[stringTag] = 'FormData'
  }

  // Patch xhr's send method to call _blob transparently
  if (_send) {
    const setRequestHeader = global.XMLHttpRequest.prototype.setRequestHeader

    global.XMLHttpRequest.prototype.setRequestHeader = function (name, value) {
      setRequestHeader.call(this, name, value)
      if (name.toLowerCase() === 'content-type') this._hasContentType = true
    }

    global.XMLHttpRequest.prototype.send = function (data) {
      // need to patch send b/c old IE don't send blob's type (#44)
      if (data instanceof FormDataPolyfill) {
        const blob = data['_blob']()
        if (!this._hasContentType) this.setRequestHeader('Content-Type', blob.type)
        _send.call(this, blob)
      } else {
        _send.call(this, data)
      }
    }
  }

  // Patch fetch's function to call _blob transparently
  if (_fetch) {
    global.fetch = function (input, init) {
      if (init && init.body && init.body instanceof FormDataPolyfill) {
        init.body = init.body['_blob']()
      }

      return _fetch.call(this, input, init)
    }
  }

  // Patch navigator.sendBeacon to use native FormData
  if (_sendBeacon) {
    global.navigator.sendBeacon = function (url, data) {
      if (data instanceof FormDataPolyfill) {
        data = data['_asNative']()
      }
      return _sendBeacon.call(this, url, data)
    }
  }

  global['FormData'] = FormDataPolyfill
}


//NextSection
/*
Third-party library: fetch, a fetch polyfill written in JS

1. Link for fetch:
https://github.com/JakeChampion/fetch.git

2. License for fetch

Copyright (c) 2014-2023 GitHub, Inc.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

3. Step-by-step guide to including fetch
3.1 Get the code using https://github.com/JakeChampion/fetch.git
3.2 Remove all instances of the keyword 'export' from the file fetch.js
3.3 Replace the existing fetch code with the contents of fetch.js

Last update for fetch: 20240215

I got rid of the setTimeout(blah, 0), to do something in 0 seconds.
Now it just does it.
I can't call set"Timeout form the master window; there is no frame.

Within fetch, I changed xhr.open to async = false.
I can't run an asynchronous xhr from the master window, it does it
by a timer, and we can't do that.
This isn't an issue with jsbg-, but just incase you have jsbg+
*/

/* eslint-disable no-prototype-builtins */
var g =
  (typeof globalThis !== 'undefined' && globalThis) ||
  (typeof self !== 'undefined' && self) ||
  // eslint-disable-next-line no-undef
  (typeof global !== 'undefined' && global) ||
  {}

var support = {
  searchParams: 'URLSearchParams' in g,
  iterable: 'Symbol' in g && 'iterator' in Symbol,
  blob:
    'FileReader' in g &&
    'Blob' in g &&
    (function() {
      try {
        new Blob()
        return true
      } catch (e) {
        return false
      }
    })(),
  formData: 'FormData' in g,
  arrayBuffer: 'ArrayBuffer' in g
}

function isDataView(obj) {
  return obj && DataView.prototype.isPrototypeOf(obj)
}

if (support.arrayBuffer) {
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

  var isArrayBufferView =
    ArrayBuffer.isView ||
    function(obj) {
      return obj && viewClasses.indexOf(Object.prototype.toString.call(obj)) > -1
    }
}

function normalizeName(name) {
  if (typeof name !== 'string') {
    name = String(name)
  }
  if (/[^a-z0-9\-#$%&'*+.^_`|~!]/i.test(name) || name === '') {
    throw new TypeError('Invalid character in header field name: "' + name + '"')
  }
  return name.toLowerCase()
}

function normalizeValue(value) {
  if (typeof value !== 'string') {
    value = String(value)
  }
  return value
}

// Build a destructive iterator for the value list
function iteratorFor(items) {
  var iterator = {
    next: function() {
      var value = items.shift()
      return {done: value === undefined, value: value}
    }
  }

  if (support.iterable) {
    iterator[Symbol.iterator] = function() {
      return iterator
    }
  }

  return iterator
}

function Headers(headers) {
  this.map = {}

  if (headers instanceof Headers) {
    headers.forEach(function(value, name) {
      this.append(name, value)
    }, this)
  } else if (Array.isArray(headers)) {
    headers.forEach(function(header) {
      if (header.length != 2) {
        throw new TypeError('Headers constructor: expected name/value pair to be length 2, found' + header.length)
      }
      this.append(header[0], header[1])
    }, this)
  } else if (headers) {
    Object.getOwnPropertyNames(headers).forEach(function(name) {
      this.append(name, headers[name])
    }, this)
  }
}

Headers.prototype.append = function(name, value) {
  name = normalizeName(name)
  value = normalizeValue(value)
  var oldValue = this.map[name]
  this.map[name] = oldValue ? oldValue + ', ' + value : value
}

Headers.prototype['delete'] = function(name) {
  delete this.map[normalizeName(name)]
}

Headers.prototype.get = function(name) {
  name = normalizeName(name)
  return this.has(name) ? this.map[name] : null
}

Headers.prototype.has = function(name) {
  return this.map.hasOwnProperty(normalizeName(name))
}

Headers.prototype.set = function(name, value) {
  this.map[normalizeName(name)] = normalizeValue(value)
}

Headers.prototype.forEach = function(callback, thisArg) {
  for (var name in this.map) {
    if (this.map.hasOwnProperty(name)) {
      callback.call(thisArg, this.map[name], name, this)
    }
  }
}

Headers.prototype.keys = function() {
  var items = []
  this.forEach(function(value, name) {
    items.push(name)
  })
  return iteratorFor(items)
}

Headers.prototype.values = function() {
  var items = []
  this.forEach(function(value) {
    items.push(value)
  })
  return iteratorFor(items)
}

Headers.prototype.entries = function() {
  var items = []
  this.forEach(function(value, name) {
    items.push([name, value])
  })
  return iteratorFor(items)
}

if (support.iterable) {
  Headers.prototype[Symbol.iterator] = Headers.prototype.entries
}

function consumed(body) {
  if (body._noBody) return
  if (body.bodyUsed) {
    return Promise.reject(new TypeError('Already read'))
  }
  body.bodyUsed = true
}

function fileReaderReady(reader) {
  return new Promise(function(resolve, reject) {
    reader.onload = function() {
      resolve(reader.result)
    }
    reader.onerror = function() {
      reject(reader.error)
    }
  })
}

function readBlobAsArrayBuffer(blob) {
  var reader = new FileReader()
  var promise = fileReaderReady(reader)
  reader.readAsArrayBuffer(blob)
  return promise
}

function readBlobAsText(blob) {
  var reader = new FileReader()
  var promise = fileReaderReady(reader)
  var match = /charset=([A-Za-z0-9_-]+)/.exec(blob.type)
  var encoding = match ? match[1] : 'utf-8'
  reader.readAsText(blob, encoding)
  return promise
}

function readArrayBufferAsText(buf) {
  var view = new Uint8Array(buf)
  var chars = new Array(view.length)

  for (var i = 0; i < view.length; i++) {
    chars[i] = String.fromCharCode(view[i])
  }
  return chars.join('')
}

function bufferClone(buf) {
  if (buf.slice) {
    return buf.slice(0)
  } else {
    var view = new Uint8Array(buf.byteLength)
    view.set(new Uint8Array(buf))
    return view.buffer
  }
}

function Body() {
  this.bodyUsed = false

  this._initBody = function(body) {
    /*
      fetch-mock wraps the Response object in an ES6 Proxy to
      provide useful test harness features such as flush. However, on
      ES5 browsers without fetch or Proxy support pollyfills must be used;
      the proxy-pollyfill is unable to proxy an attribute unless it exists
      on the object before the Proxy is created. This change ensures
      Response.bodyUsed exists on the instance, while maintaining the
      semantic of setting Request.bodyUsed in the constructor before
      _initBody is called.
    */
    // eslint-disable-next-line no-self-assign
    this.bodyUsed = this.bodyUsed
    this._bodyInit = body
    if (!body) {
      this._noBody = true;
      this._bodyText = ''
    } else if (typeof body === 'string') {
      this._bodyText = body
    } else if (support.blob && Blob.prototype.isPrototypeOf(body)) {
      this._bodyBlob = body
    } else if (support.formData && FormData.prototype.isPrototypeOf(body)) {
      this._bodyFormData = body
    } else if (support.searchParams && URLSearchParams.prototype.isPrototypeOf(body)) {
      this._bodyText = body.toString()
    } else if (support.arrayBuffer && support.blob && isDataView(body)) {
      this._bodyArrayBuffer = bufferClone(body.buffer)
      // IE 10-11 can't handle a DataView body.
      this._bodyInit = new Blob([this._bodyArrayBuffer])
    } else if (support.arrayBuffer && (ArrayBuffer.prototype.isPrototypeOf(body) || isArrayBufferView(body))) {
      this._bodyArrayBuffer = bufferClone(body)
    } else {
      this._bodyText = body = Object.prototype.toString.call(body)
    }

    if (!this.headers.get('content-type')) {
      if (typeof body === 'string') {
        this.headers.set('content-type', 'text/plain;charset=UTF-8')
      } else if (this._bodyBlob && this._bodyBlob.type) {
        this.headers.set('content-type', this._bodyBlob.type)
      } else if (support.searchParams && URLSearchParams.prototype.isPrototypeOf(body)) {
        this.headers.set('content-type', 'application/x-www-form-urlencoded;charset=UTF-8')
      }
    }
  }

  if (support.blob) {
    this.blob = function() {
      var rejected = consumed(this)
      if (rejected) {
        return rejected
      }

      if (this._bodyBlob) {
        return Promise.resolve(this._bodyBlob)
      } else if (this._bodyArrayBuffer) {
        return Promise.resolve(new Blob([this._bodyArrayBuffer]))
      } else if (this._bodyFormData) {
        throw new Error('could not read FormData body as blob')
      } else {
        return Promise.resolve(new Blob([this._bodyText]))
      }
    }
  }

  this.arrayBuffer = function() {
    if (this._bodyArrayBuffer) {
      var isConsumed = consumed(this)
      if (isConsumed) {
        return isConsumed
      } else if (ArrayBuffer.isView(this._bodyArrayBuffer)) {
        return Promise.resolve(
          this._bodyArrayBuffer.buffer.slice(
            this._bodyArrayBuffer.byteOffset,
            this._bodyArrayBuffer.byteOffset + this._bodyArrayBuffer.byteLength
          )
        )
      } else {
        return Promise.resolve(this._bodyArrayBuffer)
      }
    } else if (support.blob) {
      return this.blob().then(readBlobAsArrayBuffer)
    } else {
      throw new Error('could not read as ArrayBuffer')
    }
  }

  this.text = function() {
    var rejected = consumed(this)
    if (rejected) {
      return rejected
    }

    if (this._bodyBlob) {
      return readBlobAsText(this._bodyBlob)
    } else if (this._bodyArrayBuffer) {
      return Promise.resolve(readArrayBufferAsText(this._bodyArrayBuffer))
    } else if (this._bodyFormData) {
      throw new Error('could not read FormData body as text')
    } else {
      return Promise.resolve(this._bodyText)
    }
  }

  if (support.formData) {
    this.formData = function() {
      return this.text().then(decode)
    }
  }

  this.json = function() {
    return this.text().then(JSON.parse)
  }

  return this
}

// HTTP methods whose capitalization should be normalized
var methods = ['CONNECT', 'DELETE', 'GET', 'HEAD', 'OPTIONS', 'PATCH', 'POST', 'PUT', 'TRACE']

function normalizeMethod(method) {
  var upcased = method.toUpperCase()
  return methods.indexOf(upcased) > -1 ? upcased : method
}

function Request(input, options) {
  if (!(this instanceof Request)) {
    throw new TypeError('Please use the "new" operator, this DOM object constructor cannot be called as a function.')
  }

  options = options || {}
  var body = options.body

  if (input instanceof Request) {
    if (input.bodyUsed) {
      throw new TypeError('Already read')
    }
    this.url = input.url
    this.credentials = input.credentials
    if (!options.headers) {
      this.headers = new Headers(input.headers)
    }
    this.method = input.method
    this.mode = input.mode
    this.signal = input.signal
    if (!body && input._bodyInit != null) {
      body = input._bodyInit
      input.bodyUsed = true
    }
  } else {
    this.url = String(input)
  }

  this.credentials = options.credentials || this.credentials || 'same-origin'
  if (options.headers || !this.headers) {
    this.headers = new Headers(options.headers)
  }
  this.method = normalizeMethod(options.method || this.method || 'GET')
  this.mode = options.mode || this.mode || null
  this.signal = options.signal || this.signal || (function () {
    if ('AbortController' in g) {
      var ctrl = new AbortController();
      return ctrl.signal;
    }
  }());
  this.referrer = null

  if ((this.method === 'GET' || this.method === 'HEAD') && body) {
    throw new TypeError('Body not allowed for GET or HEAD requests')
  }
  this._initBody(body)

  if (this.method === 'GET' || this.method === 'HEAD') {
    if (options.cache === 'no-store' || options.cache === 'no-cache') {
      // Search for a '_' parameter in the query string
      var reParamSearch = /([?&])_=[^&]*/
      if (reParamSearch.test(this.url)) {
        // If it already exists then set the value with the current time
        this.url = this.url.replace(reParamSearch, '$1_=' + new Date().getTime())
      } else {
        // Otherwise add a new '_' parameter to the end with the current time
        var reQueryString = /\?/
        this.url += (reQueryString.test(this.url) ? '&' : '?') + '_=' + new Date().getTime()
      }
    }
  }
}

Request.prototype.clone = function() {
  return new Request(this, {body: this._bodyInit})
}

function decode(body) {
  var form = new FormData()
  body
    .trim()
    .split('&')
    .forEach(function(bytes) {
      if (bytes) {
        var split = bytes.split('=')
        var name = split.shift().replace(/\+/g, ' ')
        var value = split.join('=').replace(/\+/g, ' ')
        form.append(decodeURIComponent(name), decodeURIComponent(value))
      }
    })
  return form
}

function parseHeaders(rawHeaders) {
  var headers = new Headers()
  // Replace instances of \r\n and \n followed by at least one space or horizontal tab with a space
  // https://tools.ietf.org/html/rfc7230#section-3.2
  var preProcessedHeaders = rawHeaders.replace(/\r?\n[\t ]+/g, ' ')
  // Avoiding split via regex to work around a common IE11 bug with the core-js 3.6.0 regex polyfill
  // https://github.com/github/fetch/issues/748
  // https://github.com/zloirock/core-js/issues/751
  preProcessedHeaders
    .split('\r')
    .map(function(header) {
      return header.indexOf('\n') === 0 ? header.substr(1, header.length) : header
    })
    .forEach(function(line) {
      var parts = line.split(':')
      var key = parts.shift().trim()
      if (key) {
        var value = parts.join(':').trim()
        try {
          headers.append(key, value)
        } catch (error) {
          console.warn('Response ' + error.message)
        }
      }
    })
  return headers
}

Body.call(Request.prototype)

function Response(bodyInit, options) {
  if (!(this instanceof Response)) {
    throw new TypeError('Please use the "new" operator, this DOM object constructor cannot be called as a function.')
  }
  if (!options) {
    options = {}
  }

  this.type = 'default'
  this.status = options.status === undefined ? 200 : options.status
  if (this.status < 200 || this.status > 599) {
    throw new RangeError("Failed to construct 'Response': The status provided (0) is outside the range [200, 599].")
  }
  this.ok = this.status >= 200 && this.status < 300
  this.statusText = options.statusText === undefined ? '' : '' + options.statusText
  this.headers = new Headers(options.headers)
  this.url = options.url || ''
  this._initBody(bodyInit)
}

Body.call(Response.prototype)

Response.prototype.clone = function() {
  return new Response(this._bodyInit, {
    status: this.status,
    statusText: this.statusText,
    headers: new Headers(this.headers),
    url: this.url
  })
}

Response.error = function() {
  var response = new Response(null, {status: 200, statusText: ''})
  response.ok = false
  response.status = 0
  response.type = 'error'
  return response
}

var redirectStatuses = [301, 302, 303, 307, 308]

Response.redirect = function(url, status) {
  if (redirectStatuses.indexOf(status) === -1) {
    throw new RangeError('Invalid status code')
  }

  return new Response(null, {status: status, headers: {location: url}})
}

var DOMException = g.DOMException
try {
  new DOMException()
} catch (err) {
  DOMException = function(message, name) {
    this.message = message
    this.name = name
    var error = Error(message)
    this.stack = error.stack
  }
  DOMException.prototype = Object.create(Error.prototype)
  DOMException.prototype.constructor = DOMException
}

function fetch(input, init) {
  return new Promise(function(resolve, reject) {
    var request = new Request(input, init)

    if (request.signal && request.signal.aborted) {
      return reject(new DOMException('Aborted', 'AbortError'))
    }

    var xhr = new XMLHttpRequest()

    function abortXhr() {
      xhr.abort()
    }

    xhr.onload = function() {
      var options = {
        statusText: xhr.statusText,
        headers: parseHeaders(xhr.getAllResponseHeaders() || '')
      }
      // This check if specifically for when a user fetches a file locally from the file system
      // Only if the status is out of a normal range
      if (request.url.indexOf('file://') === 0 && (xhr.status < 200 || xhr.status > 599)) {
        options.status = 200;
      } else {
        options.status = xhr.status;
      }
      options.url = 'responseURL' in xhr ? xhr.responseURL : options.headers.get('X-Request-URL')
      var body = 'response' in xhr ? xhr.response : xhr.responseText
        resolve(new Response(body, options))
    }

    xhr.onerror = function() {
        reject(new TypeError('Network request failed'))
    }

    xhr.ontimeout = function() {
        reject(new TypeError('Network request timed out'))
    }

    xhr.onabort = function() {
        reject(new DOMException('Aborted', 'AbortError'))
    }

    function fixUrl(url) {
      try {
        return url === '' && g.location.href ? g.location.href : url
      } catch (e) {
        return url
      }
    }

    xhr.open(request.method, fixUrl(request.url), false)

    if (request.credentials === 'include') {
      xhr.withCredentials = true
    } else if (request.credentials === 'omit') {
      xhr.withCredentials = false
    }

    if ('responseType' in xhr) {
      if (support.blob) {
        xhr.responseType = 'blob'
      } else if (
        support.arrayBuffer
      ) {
        xhr.responseType = 'arraybuffer'
      }
    }

    if (init && typeof init.headers === 'object' && !(init.headers instanceof Headers || (g.Headers && init.headers instanceof g.Headers))) {
      var names = [];
      Object.getOwnPropertyNames(init.headers).forEach(function(name) {
        names.push(normalizeName(name))
        xhr.setRequestHeader(name, normalizeValue(init.headers[name]))
      })
      request.headers.forEach(function(value, name) {
        if (names.indexOf(name) === -1) {
          xhr.setRequestHeader(name, value)
        }
      })
    } else {
      request.headers.forEach(function(value, name) {
        xhr.setRequestHeader(name, value)
      })
    }

    if (request.signal) {
      request.signal.addEventListener('abort', abortXhr)

      xhr.onreadystatechange = function() {
        // DONE (success or failure)
        if (xhr.readyState === 4) {
          request.signal.removeEventListener('abort', abortXhr)
        }
      }
    }

    xhr.send(typeof request._bodyInit === 'undefined' ? null : request._bodyInit)
  })
}

fetch.polyfill = true

if (!g.fetch) {
  g.fetch = fetch
  g.Headers = Headers
  g.Request = Request
  g.Response = Response
}

// end third party code.

// lock down for security

for(var k in URLSearchParams.prototype)
Object.defineProperty(URLSearchParams.prototype, k,{writable:false,configurable:false});

var flist = [
getElementsByTagName, getElementsByClassName, getElementsByName, getElementById,nodeContains,
dispatchEvent,
NodeFilter,createNodeIterator,createTreeWalker,
appendChild, prependChild, insertBefore, removeChild, replaceChild, hasChildNodes,
insertAdjacentElement,append, prepend, before, after, replaceWith,
getAttribute, getAttributeNames, getAttributeNS,
hasAttribute, hasAttributeNS,
setAttribute, setAttributeNS,
removeAttribute, removeAttributeNS, getAttributeNode,
compareDocumentPosition,
getComputedStyle,
insertAdjacentHTML,URL,
TextEncoder, TextDecoder,
];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(flist[i], "toString",{value:wrapString});

// I told you I wouldn't forget these
Object.defineProperty(Object.prototype, "toString",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "toString",{enumerable:false,writable:false,configurable:false});

flist = ["Math", "Date", "Promise", "eval", "Array", "Uint8Array",
"Error", "String", "parseInt", "Event",
"alert","alert3","alert4","dumptree","uptrace","by_esn",
"showscripts", "showframes", "searchscripts", "snapshot", "aloop",
"showarg", "showarglist",
"set_location_hash",
"eb$newLocation","eb$logElement",
"resolveURL", "eb$fetchHTTP",
"setTimeout", "clearTimeout", "setInterval", "clearInterval",
"getElement", "getHead", "setHead", "getBody", "setBody",
"getRootNode","wrapString",
"getElementsByTagName", "getElementsByClassName", "getElementsByName", "getElementById","nodeContains",
"gebi", "gebtn","gebn","gebcn","cont",
"dispatchEvent","eb$listen","eb$unlisten",
"NodeFilter","createNodeIterator","createTreeWalker",
"logtime","defport","setDefaultPort","camelCase","dataCamel","isabove",
"classList","classListAdd","classListRemove","classListReplace","classListToggle","classListContains",
"mutFixup", "mrList","mrKids", "rowReindex", "insertRow", "deleteRow",
"insertCell", "deleteCell",
"appendFragment", "insertFragment",
"isRooted", "frames$rebuild",
"appendChild", "prependChild", "insertBefore", "removeChild", "replaceChild", "hasChildNodes",
"getSibling", "getElementSibling", "insertAdjacentElement",
"append", "prepend", "before", "after", "replaceWith",
"formname", "formAppendChild", "formInsertBefore", "formRemoveChild",
"implicitMember", "spilldown","spilldownResolve","spilldownResolveURL","spilldownBool",
"getAttribute", "getAttributeNames", "getAttributeNS",
"hasAttribute", "hasAttributeNS",
"setAttribute",  "setAttributeNS",
"removeAttribute", "removeAttributeNS", "getAttributeNode",
"clone1", "findObject", "correspondingObject",
"compareDocumentPosition", "generalbar", "CSS",
"Intl", "Intl_dt", "Intl_num",
"cssGather", "cssApply", "cssDocLoad",
"makeSheets", "getComputedStyle", "computeStyleInline", "cssTextGet",
"injectSetup", "eb$visible",
"insertAdjacentHTML", "htmlString", "outer$1", "textUnder", "newTextUnder",
"EventTarget", "XMLHttpRequestEventTarget", "XMLHttpRequestUpload", "XMLHttpRequest",
"URL", "File", "FileReader", "Blob", "FormData",
"Headers", "Request", "Response", "fetch",
"TextEncoder", "TextDecoder",
"MessagePortPolyfill", "MessageChannelPolyfill",
"clickfn", "checkset", "cel_define", "cel_get",
"jtfn0", "jtfn1", "jtfn2", "jtfn3", "deminimize", "addTrace",
"url_rebuild", "url_hrefset", "sortTime",
"DOMParser",
"xml_open", "xml_srh", "xml_grh", "xml_garh", "xml_send", "xml_parse",
"onmessage$$running",
];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(this, flist[i], {writable:false,configurable:false});

// some class prototypes
flist = [Date, Promise, Array, Uint8Array, Error, String, URL, URLSearchParams,
Intl_dt, Intl_num,
EventTarget, XMLHttpRequestEventTarget, XMLHttpRequestUpload, XMLHttpRequest,
Blob, FormData, Request, Response, Headers];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(flist[i], "prototype", {writable:false,configurable:false});

flist = ["eb$listen", "eb$unlisten", "addEventListener", "removeEventListener", "dispatchEvent"];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(EventTarget.prototype, flist[i], {writable:false,configurable:false});
flist = ["toString", "open", "setRequestHeader", "getResponseHeader", "getAllResponseHeaders", "send", "parseResponse", "overrideMimeType"]
for(var i=0; i<flist.length; ++i)
Object.defineProperty(XMLHttpRequest.prototype, flist[i], {writable:false,configurable:false});
Object.defineProperty(URL, "createObjectURL", {writable:false,configurable:false});
Object.defineProperty(URL, "revokeObjectURL", {writable:false,configurable:false});
flist = ["text", "slice", "stream", "arrayBuffer"];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(Blob.prototype, flist[i], {writable:false,configurable:false});
flist = ["append", "delete", "entries", "foreach", "get", "getAll", "has", "keys", "set", "values", "_asNative", "_blob", "toString"];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(FormData.prototype, flist[i], {writable:false,configurable:false});
flist = ["delete", "append", "get", "has", "set", "foreach", "keys", "values", "entries"];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(Headers.prototype, flist[i], {writable:false,configurable:false});
flist = ["bodyUsed", "_initBody", "blob", "arrayBuffer", "text", "formData", "json", "clone"];
for(var i=0; i<flist.length; ++i) {
Object.defineProperty(Request.prototype, flist[i], {writable:false,configurable:false});
Object.defineProperty(Response.prototype, flist[i], {writable:false,configurable:false});
}
Object.defineProperty(Math, "max", {writable:false,configurable:false});
Object.defineProperty(Math, "random", {writable:false,configurable:false});
Object.defineProperty(String, "prototype", {writable:false,configurable:false});
Object.defineProperty(String, "fromCharCode", {writable:false,configurable:false});
