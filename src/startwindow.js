/*********************************************************************
This file contains support javascript functions used by a browser.
They are easier to write here in javascript, then in C using the js api.
And it is portable amongst all js engines.
This file is converted into a C string and compiled and run
at the start of each javascript window.
Please take advantage of this machinery and put functions here,
including prototypes and getter / setter support functions,
whenever it makes sense to do so.

edbrowse support functions and native methods often start with eb$,
hoping they will not accidentally collide with js functions in the wild.
Example: eb$newLocation, a native method that redirects this web page to another.

It would be nice to run this file stand-alone, outside of edbrowse,
even if the functionality is limited.
To this end, I create the window object if it isn't already there,
using the obvious window = this.
*********************************************************************/

if(typeof window === "undefined") {
window = this;
mw$ = {compiled: false};
document = {};
// Stubs for native methods that are normally provided by edbrowse.
// Example: eb$puts, which we can replace with print,
// or console.log, or anything present in the command line js interpreter.
if(!window.print) print = console.log;
eb$puts = print;
eb$logputs = function(a,b) { print(b); }
eb$newLocation = function (s) { print("new location " + s); }
eb$logElement = function(o, tag) { print("pass tag " + tag + " to edbrowse"); }
eb$getcook = function(member) { return "cookies"; }
eb$setcook = function(value, member) { print(" new cookie " + value); }
eb$formSubmit = function() { print("submit"); }
eb$formReset = function() { print("reset"); }
my$win = function() { return window; }
my$doc = function() { return document; }
document.eb$apch2 = function(c) { alert("append " + c.nodeName  + " to " + this.nodeName); this.childNodes.push(c); }
querySelectorAll = function() { return [] ; }
querySelector = function() { return {} ; }
querySelector0 = function() { return false; }
eb$cssText = function(){}
}

// other names for window
self = top = parent = window;
frameElement = null;
// parent and top and frameElement could be changed
// if this is a frame in a larger frameset.

/* An ok (object keys) function for javascript/dom debugging.
 * This is in concert with the jdb command in edbrowse.
 * I know, it doesn't start with eb$ but I wanted an easy,
 * mnemonic command that I could type in quickly.
 * If a web page creates an ok function it will override this one.
And that does happen, e.g. the react system, so $ok is an alias for this. */
ok = $ok = Object.keys;

// print an error inline, at debug level 3 or higher.
function alert3(s) { eb$logputs(3, s); }
function alert4(s) { eb$logputs(4, s); }

// Dump the tree below a node, this is for debugging.
document.nodeName = "DOCUMENT"; // in case you want to start at the top.
document.tagName = "document";
window.nodeName = "WINDOW";

// Print the first line of text for a text node, and no braces
// because nothing should be below a text node.
// You can make this more elaborate and informative if you wish.
dumptree = function(top) {
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
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
dumptree(c);
}
}
alert("}");
}

uptrace = function(t) {
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
*********************************************************************/

showscripts = function() {
var i, s, m;
var slist = document.getElementsByTagName("script");
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
$ss = slist;
}

searchscripts = function(t) {
if(!$ss) showscripts();
for(var i=0; i<$ss.length; ++i)
if(w.$ss[i].text && w.$ss[i].text.indexOf(t) >= 0) alert(i);
}

snapshot = function() {
// eb$wlf is native to support the snapshot functionality
eb$wlf('<base href="' + w.eb$base + '">\n', "from");
var jslocal = "";
var idx = 0;
if(!$ss) showscripts();
for(var i=0; i<$ss.length; ++i) {
var s = $ss[i];
if(typeof s.text === "string" &&
(s.src && s.src.length || s.expanded)) {
var ss = "inline";
if(s.src && s.src.length) ss = s.src.toString();
if(ss.match(/^data:/)) continue;
// assumes the search piece of the url is spurious and unreliable
ss = ss.replace(/\?.*/, "");
++idx;
eb$wlf(s.text, "f" + idx + ".js");
jslocal += "f" + idx + ".js:" + ss + "\n";
}
}
idx = 0;
for(var i=0; i<cssSource.length; ++i) {
var s = cssSource[i];
if(typeof s.data === "string" && s.data.length &&
s.src && s.src.length) {
var ss = s.src.toString();
// assumes the search piece of the url is spurious and unreliable
ss = ss.replace(/\?.*/, "");
++idx;
eb$wlf(s.data, "f" + idx + ".css");
jslocal += "f" + idx + ".css:" + ss + "\n";
}
}
eb$wlf(jslocal, "jslocal");
alert(".   ub   ci+   /<head/r from   w base   qt");
}

// run an expression in a loop.
aloop = function(s$$, t$$, exp$$) {
if(Array.isArray(s$$)) {
aloop(0, s$$.length, t$$);
return;
}
if(typeof s$$ !== "number" || typeof t$$ !== "number" || typeof exp$$ !== "string") {
alert("aloop(array, expression) or aloop(start, end, expression)");
return;
}
exp$$ = "for(var i=" + s$$ +"; i<" + t$$ +"; ++i){" + exp$$ + "}";
eval(exp$$);
}

// produce a stack for debugging purposes
step$stack = function(){
var s = "you shouldn't see this";
try { 'use strict'; eval("yyz$"); } catch(e) { s = e.stack; }
// Lop off some leading lines that don't mean anything.
for(var i = 0; i<5; ++i)
s = s.replace(/^.*\n/, "");
return s;
}

step$l = 0;
step$go = "";
// First line of js in the base file of your snapshot might be
// step$l = 0, step$go = "c275";
// to start tracing at c275

// This is our bailout function, it references a variable that does not exist.
function eb$stopexec() { return javascript$interrupt; }

eb$nullfunction = function() { return null; }
eb$voidfunction = function() { }
eb$truefunction = function() { return true; }
eb$falsefunction = function() { return false; }

scroll = scrollTo = scrollBy = scrollByLines = scrollByPages = eb$voidfunction;
focus = blur = scroll = eb$voidfunction;
document.focus = document.blur = document.close = eb$voidfunction;
document.open = function() { return this }

/* Some visual attributes of the window.
 * These are simulations as edbrowse has no screen.
 * Better to have something than nothing at all. */
height = 768;
width = 1024;
devicePixelRatio = 1.0;
// document.status is removed because it creates a conflict with
// the status property of the XMLHttpRequest implementation
defaultStatus = 0;
returnValue = true;
menubar = true;
scrollbars = true;
toolbar = true;
resizable = true;
directories = false;
name = "unspecifiedFrame";

eb$base = "";
function eb$base$snapshot() {
document.URL = eb$base;
var u = new URL(eb$base);
// changing things behind the scenes, so as not to trigger redirection
location$2.href$val = document.location$2.href$val = u.href$val;
location$2.protocol$val = document.location$2.protocol$val = u.protocol$val;
location$2.hostname$val = document.location$2.hostname$val = u.hostname$val;
location$2.host$val = document.location$2.host$val = u.host$val;
location$2.port$val = document.location$2.port$val = u.port$val;
location$2.pathname$val = document.location$2.pathname$val = u.pathname$val;
location$2.search$val = document.location$2.search$val = u.search$val;
location$2.hash$val = document.location$2.hash$val = u.hash$val;
}

document.bgcolor = "white";
document.readyState = "loading";
document.nodeType = 9;

screen = {
height: 768, width: 1024,
availHeight: 768, availWidth: 1024, availTop: 0, availLeft: 0};

// window.alert is a simple wrapper around native puts.
// Some web pages overwrite alert.
alert = eb$puts;

// Build a lot of the DOM in dom$,
// so all those support functions aren't cluttering up window.

dom$ = {}

// The web console, one argument, print based on debugLevel.
// First a helper function, then the console object.
dom$.eb$logtime = function(debug, level, obj) {
var today=new Date;
var h=today.getHours();
var m=today.getMinutes();
var s=today.getSeconds();
// add a zero in front of numbers<10
if(h < 10) h = "0" + h;
if(m < 10) m = "0" + m;
if(s < 10) s = "0" + s;
eb$logputs(debug, "console " + level + " [" + h + ":" + m + ":" + s + "] " + obj);
}

console = {
log: function(obj) { dom$.eb$logtime(3, "log", obj); },
info: function(obj) { dom$.eb$logtime(3, "info", obj); },
warn: function(obj) { dom$.eb$logtime(3, "warn", obj); },
error: function(obj) { dom$.eb$logtime(3, "error", obj); },
timeStamp: function(label) { if(label === undefined) label = "x"; return label.toString() + (new Date).getTime(); }
};

Object.defineProperty(document, "cookie", {
get: eb$getcook, set: eb$setcook});

Object.defineProperty(document, "documentElement", {
get: function() { var e = this.lastChild;
if(!e) { alert3("missing html node"); return null; }
if(e.nodeName != "HTML") alert3("html node name " + e.nodeName);
return e; }});
Object.defineProperty(document, "head", {
get: function() { var e = this.documentElement;
if(!e) return null;
// In case somebody adds extra nodes under <html>, I search for head and body.
// But it should always be head, body.
for(var i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "HEAD") return e.childNodes[i];
alert3("missing head node"); return null;},
set: function(h) { var e = this.documentElement;
if(!e) return;
var i;
for(i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "HEAD") break;
if(i < e.childNodes.length) e.removeChild(e.childNodes[i]); else i=0;
if(h) {
if(h.nodeName != "HEAD") { alert3("head replaced with node " + h.nodeName); h.nodeName = "HEAD"; }
if(i == e.childNodes.length) e.appendChild(h);
else e.insertBefore(h, e.childNodes[i]);
}
}});
Object.defineProperty(document, "body", {
get: function() { var e = this.documentElement;
if(!e) return null;
for(var i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "BODY") return e.childNodes[i];
alert3("missing body node"); return null;},
set: function(b) { var e = this.documentElement;
if(!e) return;
var i;
for(i=0; i<e.childNodes.length; ++i)
if(e.childNodes[i].nodeName == "BODY") break;
if(i < e.childNodes.length) e.removeChild(e.childNodes[i]);
if(b) {
if(b.nodeName != "BODY") { alert3("body replaced with node " + b.nodeName); b.nodeName = "BODY"; }
if(i == e.childNodes.length) e.appendChild(b);
else e.insertBefore(b, e.childNodes[i]);
}
}});

navigator = {};
navigator.appName = "edbrowse";
navigator["appCode Name"] = "edbrowse C/duktape";
/* not sure what product is about */
navigator.product = "edbrowse";
navigator.productSub = "3.7";
navigator.vendor = "Karl Dahlke";
navigator.javaEnabled = eb$falsefunction;
navigator.taintEnabled = eb$falsefunction;
navigator.cookieEnabled = true;
navigator.onLine = true;
navigator.mimeTypes = [];
navigator.plugins = [];
// the rest of navigator, and of course the plugins,
// must be filled in at run time based on the config file.
// This is overwritten at startup by edbrowse.
navigator.userAgent = "edbrowse/3.0.0";
// might be useful to pretend like we have low bandwidth,
// so the website doesn't send down all sorts of visual crap.
navigator.connection = {
downlink: 50,
downlinkMax: 100,
effectiveType: "2g",
rtt: 8,
saveData: false,
type: "unknown",
addEventListener: eb$voidfunction,
removeEventListener: eb$voidfunction,
};

/* There's no history in edbrowse. */
/* Only the current file is known, hence length is 1. */
history = {
length: 1,
next: "",
previous: "",
back: eb$voidfunction,
forward: eb$voidfunction,
go: eb$voidfunction,
pushState: eb$voidfunction,
replaceState: eb$voidfunction,
toString: function() {  return "Sorry, edbrowse does not maintain a browsing history."; } 
}

/* some base arrays - lists of things we'll probably need */
document.heads = [];
document.bases = [];
document.links = [];
document.metas = [];
document.styles = [];
document.bodies = [];
document.forms = [];
document.elements = [];
document.divs = [];
document.labels = [];
document.htmlobjs = [];
document.scripts = [];
document.paragraphs = [];
document.headers = [];
document.footers = [];
document.tables = [];
document.spans = [];
document.images = [];
// styleSheets is a placeholder for now; I don't know what to do with it.
document.styleSheets = [];

frames = [];
// to debug a.href = object or other weird things.
hrefset$p = []; hrefset$a = [];

// symbolic constants for compareDocumentPosition
DOCUMENT_POSITION_DISCONNECTED = 1;
DOCUMENT_POSITION_PRECEDING = 2;
DOCUMENT_POSITION_FOLLOWING = 4;
DOCUMENT_POSITION_CONTAINS = 8;
DOCUMENT_POSITION_CONTAINED_BY = 16;

/*********************************************************************
The URL class is head-spinning in its complexity and its side effects.
Almost all of these can be handled in JS,
except for setting window.location or document.location to a new url,
which replaces the web page you are looking at.
This side effect does not take place in the constructor, which establishes the initial url.
*********************************************************************/

URL = function() {
var h = "";
if(arguments.length > 0) h= arguments[0];
this.href = h;
}
URL.prototype.dom$class = "URL";

/* rebuild the href string from its components.
 * Call this when a component changes.
 * All components are strings, except for port,
 * and all should be defined, even if they are empty. */
URL.prototype.rebuild = function() {
var h = "";
if(this.protocol$val.length) {
// protocol includes the colon
h = this.protocol$val;
var plc = h.toLowerCase();
if(plc != "mailto:" && plc != "telnet:" && plc != "javascript:")
h += "//";
}
if(this.host$val.length) {
h += this.host$val;
} else if(this.hostname$val.length) {
h += this.hostname$val;
if(this.port$val != 0)
h += ":" + this.port$val;
}
if(this.pathname$val.length) {
// pathname should always begin with /, should we check for that?
if(!this.pathname$val.match(/^\//))
h += "/";
h += this.pathname$val;
}
if(this.search$val.length) {
// search should always begin with ?, should we check for that?
h += this.search$val;
}
if(this.hash$val.length) {
// hash should always begin with #, should we check for that?
h += this.hash$val;
}
this.href$val = h;
};
Object.defineProperty(URL.prototype, "rebuild", {enumerable:false});

// No idea why we can't just assign the property directly.
// URL.prototype.protocol = { ... };
Object.defineProperty(URL.prototype, "protocol", {
  get: function() {return this.protocol$val; },
  set: function(v) { this.protocol$val = v; this.rebuild(); },
enumerable:true
});

Object.defineProperty(URL.prototype, "pathname", {
  get: function() {return this.pathname$val; },
  set: function(v) { this.pathname$val = v; this.rebuild(); },
enumerable:true
});

Object.defineProperty(URL.prototype, "search", {
  get: function() {return this.search$val; },
  set: function(v) { this.search$val = v; this.rebuild(); },
enumerable:true
});

Object.defineProperty(URL.prototype, "hash", {
  get: function() {return this.hash$val; },
  set: function(v) { this.hash$val = v; this.rebuild(); },
enumerable:true
});

Object.defineProperty(URL.prototype, "port", {
  get: function() {return this.port$val; },
  set: function(v) { this.port$val = v;
if(this.hostname$val.length)
this.host$val = this.hostname$val + ":" + v;
this.rebuild(); },
enumerable:true
});

Object.defineProperty(URL.prototype, "hostname", {
  get: function() {return this.hostname$val; },
  set: function(v) { this.hostname$val = v;
if(this.port$val)
this.host$val = v + ":" +  this.port$val;
this.rebuild(); },
enumerable:true
});

Object.defineProperty(URL.prototype, "host", {
  get: function() {return this.host$val; },
  set: function(v) { this.host$val = v;
if(v.match(/:/)) {
this.hostname$val = v.replace(/:.*/, "");
this.port$val = v.replace(/^.*:/, "");
/* port has to be an integer */
this.port$val = parseInt(this.port$val);
} else {
this.hostname$val = v;
this.port$val = 0;
}
this.rebuild(); },
enumerable:true
});

dom$.eb$defport = {
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

/* returns default port as an integer, based on protocol */
dom$.eb$setDefaultPort = function(p) {
var port = 0;
p = p.toLowerCase().replace(/:/, "");
if(dom$.eb$defport.hasOwnProperty(p))
port = dom$.eb$defport[p];
return port;
}

Object.defineProperty(URL.prototype, "href", {
  get: function() {return this.href$val; },
  set: function(v) {
var inconstruct = true;
if(v.dom$class == "URL") v = v.toString();
if(v === null || v === undefined) v = "";
if(typeof v != "string") return;
if(typeof this.href$val == "string") {
// Ok, we already had a url, and here's nother one.
// I think we're suppose to resolve it against what was already there,
// so that /foo against www.xyz.com becomes www.xyz.com/foobar
if(v) v = eb$resolveURL(this.href$val, v);
inconstruct = false;
}
if(inconstruct) {
Object.defineProperty(this, "href$val", {enumerable:false, writable:true, value:v});
Object.defineProperty(this, "protocol$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "hostname$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "host$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "port$val", {enumerable:false, writable:true, value:0});
Object.defineProperty(this, "pathname$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "search$val", {enumerable:false, writable:true, value:""});
Object.defineProperty(this, "hash$val", {enumerable:false, writable:true, value:""});
} else {
this.href$val = v;
this.port$val = 0;
this.protocol$val = this.host$val = this.hostname$val = this.pathname$val = this.search$val = this.hash$val = "";
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
if(this.host$val.match(/:/)) {
this.hostname$val = this.host$val.replace(/:.*/, "");
this.port$val = this.host$val.replace(/^.*:/, "");
/* port has to be an integer */
this.port$val = parseInt(this.port$val);
} else {
this.hostname$val = this.host$val;
// should we be filling in a default port here?
this.port$val = dom$.eb$setDefaultPort(this.protocol$val);
}
// perhaps set protocol to http if it looks like a url?
// as in edbrowse foo.bar.com
// Ends in standard tld, or looks like an ip4 address, or starts with www.
if(this.protocol$val == "" &&
(this.hostname$val.match(/\.(com|org|net|info|biz|gov|edu|us|uk|ca|au)$/) ||
this.hostname$val.match(/^\d+\.\d+\.\d+\.\d+$/) ||
this.hostname$val.match(/^www\..*\.[a-zA-Z]{2,}$/))) {
this.protocol$val = "http:";
if(this.port$val == 0)
this.port$val = 80;
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
if(!inconstruct && (this == my$win().location || this == my$doc().location)) {
// replace the web page
eb$newLocation('r' + this.href$val + '\n');
}
},
enumerable:true
});

URL.prototype.toString = function() {  return this.href$val; }
Object.defineProperty(URL.prototype, "toString", {enumerable:false});

Object.defineProperty(URL.prototype, "length", { get: function() { return this.toString().length; }});

URL.prototype.concat = function(s) {  return this.toString().concat(s); }
Object.defineProperty(URL.prototype, "concat", {enumerable:false});

URL.prototype.startsWith = function(s) {  return this.toString().startsWith(s); }
Object.defineProperty(URL.prototype, "startsWith", {enumerable:false});

URL.prototype.endsWith = function(s) {  return this.toString().endsWith(s); }
Object.defineProperty(URL.prototype, "endsWith", {enumerable:false});

URL.prototype.includes = function(s) {  return this.toString().includes(s); }
Object.defineProperty(URL.prototype, "includes", {enumerable:false});

/*
Can't turn URL.search into String.search, because search is already a property
of URL, that is, the search portion of the URL.
URL.prototype.search = function(s) { 
return this.toString().search(s);
}
*/

URL.prototype.indexOf = function(s) {  return this.toString().indexOf(s); }
Object.defineProperty(URL.prototype, "indexOf", {enumerable:false});

URL.prototype.lastIndexOf = function(s) {  return this.toString().lastIndexOf(s); }
Object.defineProperty(URL.prototype, "lastIndexOf", {enumerable:false});

URL.prototype.substring = function(from, to) {  return this.toString().substring(from, to); }
Object.defineProperty(URL.prototype, "substring", {enumerable:false});

URL.prototype.substr = function(from, to) {return this.toString().substr(from, to);}
Object.defineProperty(URL.prototype, "substr", {enumerable:false});

URL.prototype.toLowerCase = function() {  return this.toString().toLowerCase(); }
Object.defineProperty(URL.prototype, "toLowerCase", {enumerable:false});

URL.prototype.toUpperCase = function() {  return this.toString().toUpperCase(); }
Object.defineProperty(URL.prototype, "toUpperCase", {enumerable:false});

URL.prototype.match = function(s) {  return this.toString().match(s); }
Object.defineProperty(URL.prototype, "match", {enumerable:false});

URL.prototype.replace = function(s, t) {  return this.toString().replace(s, t); }
Object.defineProperty(URL.prototype, "replace", {enumerable:false});

URL.prototype.split = function(s) { return this.toString().split(s); }
Object.defineProperty(URL.prototype, "split", {enumerable:false});

URL.prototype.slice = function(from, to) { return this.toString().slice(from, to); }
Object.defineProperty(URL.prototype, "slice", {enumerable:false});

URL.prototype.charAt = function(n) { return this.toString().charAt(n); }
Object.defineProperty(URL.prototype, "charAt", {enumerable:false});

URL.prototype.charCodeAt = function(n) { return this.toString().charCodeAt(n); }
Object.defineProperty(URL.prototype, "charCodeAt", {enumerable:false});

URL.prototype.trim = function() { return this.toString().trim(); }
Object.defineProperty(URL.prototype, "trim", {enumerable:false});

/*********************************************************************
Here are the DOM classes with generic constructors.
But first, the Node class, which is suppose to be the parent class
of all the others. Javascript can't inherit like that, which is a bummer.
Still, I include Node because some javascript will interrogate Node to see
which methods all the nodes possess?
Do we support appendchild?   etc.
*********************************************************************/

Node = function(){};
Node.prototype.dom$class = "Node";

HTML = function(){};
// Some screen attributes that are suppose to be there.
HTML.prototype = {
dom$class: "HTML",
doScroll: eb$voidfunction,
clientHeight: 768, clientWidth: 1024,
offsetHeight: 768, offsetWidth: 1024,
 scrollHeight: 768, scrollWidth: 1024,
scrollTop: 0, scrollLeft: 0
};

// is there a difference between DocType ad DocumentType?
DocType = function(){ this.nodeType = 10, this.nodeName = "DOCTYPE";};
DocType.prototype.dom$class = "DocType";
DocumentType = function(){};
DocumentType.prototype.dom$class = "DocumentType";
CharacterData = function(){};
CharacterData.prototype.dom$class = "CharacterData";
Head = function(){};
Head.prototype.dom$class = "Head";
Meta = function(){};
Meta.prototype.dom$class = "Meta";
Title = function(){};
Title.prototype.dom$class = "Title";
Object.defineProperty(Title.prototype, "text", {
get: function(){ return this.firstChild && this.firstChild.nodeName == "#text" && this.firstChild.data || "";}
// setter should change the title of the document, not yet implemented
});
Link = function(){};
Link.prototype.dom$class = "Link";
// It's a list but why would it ever be more than one?
Object.defineProperty(Link.prototype, "relList", {
get: function() { var a = this.rel ? [this.rel] : [];
// edbrowse only supports stylesheet
a.supports = function(s) { return s === "stylesheet"; }
return a;
}});

Body = function(){};
Body.prototype = {
dom$class: "Body",
doScroll: eb$voidfunction,
clientHeight: 768, clientWidth: 1024,
offsetHeight: 768, offsetWidth: 1024,
 scrollHeight: 768, scrollWidth: 1024,
scrollTop: 0, scrollLeft: 0
}

Base = function(){};
Base.prototype.dom$class = "Base";
Form = function(){ this.elements = [];};
Form.prototype.dom$class = "Form";
Form.prototype.submit = eb$formSubmit;
Form.prototype.reset = eb$formReset;
Object.defineProperty(Form.prototype, "length", { get: function() { return this.elements.length;}});

Validity = function(){};
Validity.prototype.dom$class = "Validity";
/*********************************************************************
All these should be getters, or should they?
Consider the tooLong attribute.
tooLong could compare the length of the input with the maxlength attribute,
that's what the gettter would do, but edbrowse already does that at entry time.
In general, shouldn't edbrowse check for most r all of these on entry,
so then most of these wouldn't have to be getters?
patternMismatch on email and url, etc.
One thing that always has to be a getter is valueMissing,
cause it starts out empty of course, and is a required field.
And valid is a getter, true if everything else is false.
*********************************************************************/
Validity.prototype.badInput =
Validity.prototype.customError =
Validity.prototype.patternMismatch =
Validity.prototype.rangeOverflow =
Validity.prototype.rangeUnderflow =
Validity.prototype.stepMismatch =
Validity.prototype.tooLong =
Validity.prototype.tooShort =
Validity.prototype.typeMismatch = false;
Object.defineProperty(Validity.prototype, "valueMissing", {
get: function() {var o = this.owner;  return o.required && o.value == ""; }});
Object.defineProperty(Validity.prototype, "valid", {
get: function() { // only need to check items with getters
return !(this.valueMissing)}});

Element = function() { this.validity = new Validity, this.validity.owner = this};
Element.prototype.dom$class = "Element";
Element.prototype.selectionStart = 0;
Element.prototype.selectionEnd = -1;
Element.prototype.selectionDirection = "none";
// I really don't know what this function does, something visual I think.
Element.prototype.setSelectionRange = function(s, e, dir) {
if(typeof s == "number") this.selectionStart = s;
if(typeof e == "number") this.selectionEnd = e;
if(typeof dir == "string") this.selectionDirection = dir;
}

Element.prototype.click = function() {
var nn = this.nodeName, t = this.type;
// as though the user had clicked on this
if(nn == "button" || (nn == "INPUT" &&
(t == "button" || t == "reset" || t == "submit" || t == "checkbox" || t == "radio"))) {
var e = new Event;
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

// We only need this in the rare case of setting click and clearing
// the other radio buttons. acid test 43
Object.defineProperty(Element.prototype, "checked", {
get: function() { return this.checked$2 ? true : false; },
set: function(n) {
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
}});

Object.defineProperty(Element.prototype, "name", {
get: function() { return this.name$2; },
set: function(n) { var f; if(f = this.form) {
if(this.name$2 && f[this.name$2] == this) delete f[this.name$2];
if(this.name$2 && f.elements[this.name$2] == this) delete f.elements[this.name$2];
if(!f[n]) f[n] = this;
if(!f.elements[n]) f.elements[n] = this;
}
this.name$2 = n;
}});

// only meaningful for textarea
Object.defineProperty(Element.prototype, "innerText", {
get: function() { return this.type == "textarea" ? this.value : null },
set: function(t) { if(this.type == "textarea") this.value = t; }});

/*********************************************************************
This is a special routine for textarea.innerHTML = "some html text";
I assume, with very little data to go on, that the html is rendered
in some fashion, i.e. turned into text, then pushed into the text area.
This is just a first step. If there is a text node below then I
cross that over to textarea.value. If it's anything more complicated
than that, I throw up my hands and give up.
Yes, I found this in the real world when trying to unsubscribe from
	https://www.credomobile.com
I remove the textNode below, because it would be rendered by edbrowse,
and the text that was just put in the buffer would also be on the main page.
Note the chain of setters.
Javascript calls innerHTML, which is a setter written in C.
That calls this routine, which pushes the rendered string into value,
which is another setter, writtten in C.
If all this works I'll be amazed.
*********************************************************************/

dom$.textarea$html$crossover = function(t) {
if(!t || t.dom$class != "Element" || t.type != "textarea")
return;
t.value = "";
// It's a textarea - what is below?
if(t.childNodes.length == 0) return; // nothing below
var tn; // our textNode
if(t.childNodes.length == 1 && (tn = t.firstChild) &&
tn.dom$class == "TextNode") {
var d = (tn.data ? tn.data : "");
t.value = d;
t.removeChild(tn);
return;
}
alert3("textarea.innerHTML is too complicated for me to render");
}

HTMLElement = function(){};
HTMLElement.prototype.dom$class = "HTMLElement";
Select = function() { this.selectedIndex = -1; this.value = "";this.validity = new Validity, this.validity.owner = this};
Select.prototype.dom$class = "Select";
Object.defineProperty(Select.prototype, "value", {
get: function() {
var a = this.options;
var n = this.selectedIndex;
return (this.multiple || n < 0 || n >= a.length) ? "" : a[n].value;
}});
Image = function(){};
Image.prototype.dom$class = "Image";
Frame = function(){};
Frame.prototype.dom$class = "Frame";
// This is a placeholder for now. I don't know what HTMLIFrameElement is.
HTMLIFrameElement = Frame;
Anchor = function(){};
Anchor.prototype.dom$class = "Anchor";
HTMLAnchorElement = function(){};
HTMLAnchorElement.prototype.dom$class = "HTMLAnchorElement";
HTMLLinkElement = function(){};
HTMLLinkElement.prototype.dom$class = "HTMLLinkElement";
HTMLAreaElement = function(){};
HTMLAreaElement.prototype.dom$class = "HTMLAreaElement";
Lister = function(){};
Lister.prototype.dom$class = "Lister";
Listitem = function(){};
Listitem.prototype.dom$class = "Listitem";
tBody = function(){ this.rows = []};
tBody.prototype.dom$class = "tBody";
tHead = function(){ this.rows = []};
tHead.prototype.dom$class = "tHead";
tFoot = function(){ this.rows = []};
tFoot.prototype.dom$class = "tFoot";
tCap = function(){};
tCap.prototype.dom$class = "tCap";
Table = function(){ this.rows = []; this.tBodies = []};
Table.prototype.dom$class = "Table";
tRow = function(){ this.cells = []};
tRow.prototype.dom$class = "tRow";
Cell = function(){};
Cell.prototype.dom$class = "Cell";
Div = function(){};
Div.prototype.dom$class = "Div";
Div.prototype.doScroll = eb$voidfunction;
Div.prototype.click = function() {
// as though the user had clicked on this
var e = new Event;
e.initEvent("click", true, true);
this.dispatchEvent(e);
}
Label = function(){};
Label.prototype.dom$class = "Label";
Object.defineProperty(Label.prototype, "htmlFor", { get: function() { return this.getAttribute("for"); }, set: function(h) { this.setAttribute("for", h); }});
HtmlObj = function(){};
HtmlObj.prototype.dom$class = "HtmlObj";
Area = function(){};
Area.prototype.dom$class = "Area";
Span = function(){};
Span.prototype.dom$class = "Span";
Span.prototype.doScroll = eb$voidfunction;
P = function(){};
P.prototype.dom$class = "P";
Header = function(){};
Header.prototype.dom$class = "Header";
Footer = function(){};
Footer.prototype.dom$class = "Footer";
Script = function(){};
Script.prototype.dom$class = "Script";
Script.prototype.type = "";
Script.prototype.text = "";
HTMLScriptElement = Script; // alias for Script, I guess
Timer = function(){this.nodeName = "TIMER"};
Timer.prototype.dom$class = "Timer";
Audio = function(){};
Audio.prototype.dom$class = "Audio";
Audio.prototype.play = eb$voidfunction;

/*********************************************************************
If foo is an anchor, then foo.href = blah
builds the url object; there are a lot of side effects here.
Same for form.action, script.src, etc.
I believe that a new URL should be resolved against the base, that is,
/foobar becomes www.xyz.com/foobar, though I'm not sure.
We ought not do this in the generic URL class, but for these assignments, I think yes.
The URL class already resolves when updating a URL,
so this is just for a new url A.href = "/foobar";
There is no base when html is first processed, so start with an empty string,
so we don't seg fault. resolveURL does nothing in this case.
This is seen by eb$base = "" above.
When base is set, and more html is generated and parsed, the url is resolved
in html, and then again here in js.
The first time it becomes its own url, then remains so,
I don't think this is a problem, but not entirely sure.
There may be shortcuts associated with these url members.
Some websites refer to A.protocol, which has not explicitly been set.
I assume they mean A.href.protocol, the protocol of the url object.
Do we have to do this for every component of the URL object,
and for every class that has such an object?
I don't know, but here we go.
This is a loop over classes, then a loop over url components.
The leading ; averts a javascript parsing ambiguity.
don't take it out!
*********************************************************************/

; (function() {
var cnlist = ["Anchor", "HTMLAnchorElement", "Image", "Script", "Link", "Area", "Form", "Frame", "Audio"];
var ulist = ["href", "href", "src", "src", "href", "href", "action", "src", "src"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i]; // class name
var u = ulist[i]; // url name
eval('Object.defineProperty(' + cn + '.prototype, "' + u + '", { \
get: function() { if(!this.href$2) this.href$2 = new URL; return this.href$2; }, \
set: function(h) { if(h.dom$class == "URL") h = h.toString(); \
if(h === null || h === undefined) h = ""; \
var w = my$win(); \
if(typeof h !== "string") { alert3("hrefset " + typeof h); \
w.hrefset$p.push("' + cn + '"); \
w.hrefset$a.push(h); \
return; } \
var last_href = (this.href$2 ? this.href$2.href$val : null); \
if(!this.href$2) { this.href$2 = new URL(h ? eb$resolveURL(w.eb$base,h) : h) } else { if(!this.href$2.href$val && h) h =  eb$resolveURL(w.eb$base,h); \
this.href$2.href = h; }  \
var next_href = this.href$2.href$val; \
/* special code for setting frame.src, redirect to a new page. */ \
if(this.dom$class == "Frame" && this.content$Document && this.content$Document.lastChild && last_href != next_href && next_href) { \
/* There is a nasty corner case here, dont know if it ever happens. What if we are replacing the running frame? window.parent.src = new_url; See if we can get around it this way. */ \
if(w == this.content$Window) { w.location = next_href; return; } \
var d = new Document; d.childNodes = []; d.attributes = new NamedNodeMap; d.attributes.owner = d; \
d.nodeName = "DOCUMENT"; d.tagName = "document"; d.nodeType = 9; d.ownerDocument = my$doc(); \
delete this.eb$auto; this.content$Document = this.content$Window = d; \
eb$unframe(this, d); /* fix links on the edbrowse side */ \
this.childNodes[0] = d; d.parentNode = this; \
/* I can force the opening of this new frame, but should I? */ \
this.contentDocument; eb$unframe2(this); \
} }});');
var piecelist = ["protocol", "pathname", "host", "search", "hostname", "port", "hash"];
for(var j=0; j<piecelist.length; ++j) {
var piece = piecelist[j];
eval('Object.defineProperty(' + cn + '.prototype, "' + piece + '", {get: function() { return this.href$2 ? this.href$2.' + piece + ' : null},set: function(x) { if(this.href$2) this.href$2.' + piece + ' = x; }});');
}
}
})();

/*********************************************************************
When a script runs it may call document.write. But where to put those nodes?
I think they belong under the script object, I think that's intuitive,
but most browsers put them under body,
or at least under the parent of the script object,
but always in position, as though they were right here in place of the script.
This function lifts the nodes from the script object to its parent,
in position, just after the script.
*********************************************************************/

eb$uplift = function(s) {
var p = s.parentNode;
if(!p) return; // should never happen
var before = s.nextSibling;
while(s.firstChild)
if(before) p.insertBefore(s.firstChild, before);
else p.appendChild(s.firstChild);
}

// Canvas method draws a picture. That's meaningless for us,
// but it still has to be there.
Canvas = function() {};
Canvas.prototype.dom$class = "Canvas";
Canvas.prototype.getContext = function(x) { return { addHitRegion: eb$nullfunction,
arc: eb$nullfunction,
arcTo: eb$nullfunction,
beginPath: eb$nullfunction,
bezierCurveTo: eb$nullfunction,
clearHitRegions: eb$nullfunction,
clearRect: eb$nullfunction,
clip: eb$nullfunction,
closePath: eb$nullfunction,
createImageData: eb$nullfunction,
createLinearGradient: eb$nullfunction,
createPattern: eb$nullfunction,
createRadialGradient: eb$nullfunction,
drawFocusIfNeeded: eb$nullfunction,
drawImage: eb$nullfunction,
drawWidgetAsOnScreen: eb$nullfunction,
drawWindow: eb$nullfunction,
ellipse: eb$nullfunction,
fill: eb$nullfunction,
fillRect: eb$nullfunction,
fillText: eb$nullfunction,
getImageData: eb$nullfunction,
getLineDash: eb$nullfunction,
isPointInPath: eb$nullfunction,
isPointInStroke: eb$nullfunction,
lineTo: eb$nullfunction,
measureText: function(s) {
// returns a TextMetrics object, whatever that is.
// Height and width will depend on the font, but this is just a stub.
return {height: 12, width: s.length * 7};
},
moveTo: eb$nullfunction,
putImageData: eb$nullfunction,
quadraticCurveTo: eb$nullfunction,
rect: eb$nullfunction,
removeHitRegion: eb$nullfunction,
resetTransform: eb$nullfunction,
restore: eb$nullfunction,
rotate: eb$nullfunction,
save: eb$nullfunction,
scale: eb$nullfunction,
scrollPathIntoView: eb$nullfunction,
setLineDash: eb$nullfunction,
setTransform: eb$nullfunction,
stroke: eb$nullfunction,
strokeRect: eb$nullfunction,
strokeText: eb$nullfunction,
transform: eb$nullfunction,
translate: eb$nullfunction }};
Canvas.prototype.toDataURL = function() {
if(this.height === 0 || this.width === 0) return "data:,";
// this is just a stub
return "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAADElEQVQImWNgoBMAAABpAAFEI8ARAAAAAElFTkSuQmCC";
}

postMessage = function (message,target_origin) {
if (this.location.protocol + "//" + this.location.hostname == target_origin || target_origin == "*") {
if (typeof this.onmessage == "function") {
// whose responsibility is it to add the event handler?   The web developer's?
// Because we (the browser implementers) don't have any way of knowing what in particular
// they want the handler to actually do with the message.  Right?
var me = new Event;
me.name = "message";
me.type = "message";
me.data = message
this.onmessage(me);
}
}
}

/*********************************************************************
AudioContext, for playing music etc.
This one we could implement, but I'm not sure if we should.
If speech comes out of the same speakers as music, as it often does,
you might not want to hear it, you might rather see the url, or have a button
to push, and then you call up the music only if / when you want it.
Not sure what to do, so it's pretty much stubs for now.
*********************************************************************/
AudioContext = function() {
this.outputLatency = 1.0;
this.createMediaElementSource = eb$voidfunction;
this.createMediaStreamSource = eb$voidfunction;
this.createMediaStreamDestination = eb$voidfunction;
this.createMediaStreamTrackSource = eb$voidfunction;
this.suspend = eb$voidfunction;
this.close = eb$voidfunction;
}
AudioContext.prototype.dom$class = "AudioContext";

// Document class, I don't know what to make of this,
// but my stubs for frames needs it.
Document = function(){};
Document.prototype.dom$class = "Document";

CSSStyleSheet = function() { this.cssRules = []};
CSSStyleSheet.prototype.dom$class = "CSSStyleSheet";
CSSStyleSheet.prototype.insertRule = function(r, idx) {
var list = this.cssRules;
(typeof idx == "number" && idx >= 0 && idx <= list.length || (idx = 0));
if(idx == list.length)
list.push(r);
else
list.splice(idx, 0, r);
// There may be side effects here, I don't know.
// For now I just want the method to exist so js will march on.
}
CSSStyleSheet.prototype.addRule = function(sel, r, idx) {
var list = this.cssRules;
(typeof idx == "number" && idx >= 0 && idx <= list.length || (idx = list.length));
r = sel + "{" + r + "}";
if(idx == list.length)
list.push(r);
else
list.splice(idx, 0, r);
}

dom$.camelCase = function(t) {
return t.replace(/-./g, function(f){return f[1].toUpperCase()});
}
dom$.dataCamel = function(t) { return dom$.camelCase(t.replace(/^data-/,"")); }

CSSStyleDeclaration = function(){
        this.element = null;
        this.style = this;
	 this.attributes = new NamedNodeMap;
this.ownerDocument = my$doc();
this.attributes.owner = this;
this.sheet = new CSSStyleSheet;
};
CSSStyleDeclaration.prototype.dom$class = "CSSStyleDeclaration";
// these are default properties of a style object
CSSStyleDeclaration.prototype.animationDelay =
CSSStyleDeclaration.prototype.animationDuration =
CSSStyleDeclaration.prototype.transitionDelay =
CSSStyleDeclaration.prototype.transitionDuration ="";
CSSStyleDeclaration.prototype.textTransform = "none", // acid test 46
CSSStyleDeclaration.prototype.toString = function() { return "style object" };
CSSStyleDeclaration.prototype.getPropertyValue = function(p) {
p = dom$.camelCase(p);
                if (this[p] == undefined)                
                        this[p] = "";
                        return this[p];
};
CSSStyleDeclaration.prototype.getProperty = function(p) {
p = dom$.camelCase(p);
return this[p] ? this[p] : "";
};
CSSStyleDeclaration.prototype.setProperty = function(p, v, prv) {
p = dom$.camelCase(p);
this[p] = v;
var pri = p + "$pri";
this[pri] = (prv === "important");
};
CSSStyleDeclaration.prototype.getPropertyPriority = function(p) {
p = dom$.camelCase(p);
var pri = p + "$pri";
return this[pri] ? "important" : "";
};
Object.defineProperty(CSSStyleDeclaration.prototype, "css$data", {
get: function() { var s = ""; for(var i=0; i<this.childNodes.length; ++i) if(this.childNodes[i].nodeName == "#text") s += this.childNodes[i].data; return s; }});

dom$.cssTextGet = function() {
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

document.defaultView = window;
getComputedStyle = function(e,pe) {
	// disregarding pseudoelements for now
var s;

/*********************************************************************
Some sites call getComputedStyle on the same node over and over again.
http://songmeanings.com/songs/view/3530822107858535238/
Can we remember the previous call and just return the same style object?
Can we know that nothing has changed in between the two calls?
I can track when the tree changes, and even the class,
but what about individual attributes?
I haven't found a way to do this without breaking acid test 33 and others.
*********************************************************************/

s = new CSSStyleDeclaration;
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
*********************************************************************/

// remember that this is the window object
dom$.cssGather(false, this);

eb$cssApply(this, e, s);

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

if(e.style) {
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
dom$.computeStyleInline = function(e) {
var s;

e.last$class = e.class, e.last$id = e.id;

// don't put a style under a style.
// There are probably other nodes I should skip too.
if(e.dom$class == "CSSStyleDeclaration") return;
if(e.nodeType != 1 && e.nodeType != 3) return;

// style object should already be there
if(!e.style) {
e.style = new CSSStyleDeclaration;
e.style.element = e;
}
s = e.style;

// This is called on a (possibly large) subtree of nodes,
// so please verify the css style sheets before hand.
// dom$.cssGather(false, this);

// Unlike the above, we remove previous values that were set by css,
// because css is being reapplied.
// I don't know if we should just do this for the node that changed,
// or for all its descendants.
for(var k in s) {
if(!s.hasOwnProperty(k)) continue;
if(!k.match(/\$(\$scy|pri)$/)) continue;
if(k.match(/\$\$scy$/) && s[k] == 100000) continue;
// this one goes away
delete s[k];
delete s[k.replace(/\$(\$scy|pri)$/, "")];
}

// apply all the css rules
eb$cssApply(my$win(), e, s);
// style has been recomputed
// descend into the children
if(e.childNodes)
for(var i=0; i<e.childNodes.length; ++i)
dom$.computeStyleInline(e.childNodes[i]);
}

// It's crude, but just reindex all the rows in a table
rowReindex = function(t) {
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
dom$.insertRow = function(idx) {
if(idx === undefined) idx = -1;
if(typeof idx !== "number") return null;
var t = this;
var nrows = t.rows.length;
if(idx < 0) idx = nrows;
if(idx > nrows) return null;
var r = document.createElement("tr");
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
Table.prototype.insertRow = dom$.insertRow;
tBody.prototype.insertRow = dom$.insertRow;
tHead.prototype.insertRow = dom$.insertRow;
tFoot.prototype.insertRow = dom$.insertRow;

dom$.deleteRow = function(r) {
if(r.dom$class != "tRow") return;
this.removeChild(r);
}
Table.prototype.deleteRow = dom$.deleteRow;
tBody.prototype.deleteRow = dom$.deleteRow;
tHead.prototype.deleteRow = dom$.deleteRow;
tFoot.prototype.deleteRow = dom$.deleteRow;

tRow.prototype.insertCell = function(idx) {
if(idx === undefined) idx = -1;
if(typeof idx !== "number") return null;
var t = this;
var n = t.childNodes.length;
if(idx < 0) idx = n;
if(idx > n) return null;
var r = document.createElement("td");
if(idx == n)
t.appendChild(r);
else
t.insertBefore(r, t.childNodes[idx]);
return r;
}

tRow.prototype.deleteCell = function(r) {
if(r.dom$class != "Cell") return;
this.removeChild(r);
}

Table.prototype.createCaption = function() {
if(this.caption) return this.caption;
var c = document.createElement("caption");
this.appendChild(c);
return c;
}
Table.prototype.deleteCaption = function() {
if(this.caption) this.removeChild(this.caption);
}

Table.prototype.createTHead = function() {
if(this.tHead) return this.tHead;
var c = document.createElement("thead");
this.prependChild(c);
return c;
}
Table.prototype.deleteTHead = function() {
if(this.tHead) this.removeChild(this.tHead);
}

Table.prototype.createTFoot = function() {
if(this.tFoot) return this.tFoot;
var c = document.createElement("tfoot");
this.insertBefore(c, this.caption);
return c;
}
Table.prototype.deleteTFoot = function() {
if(this.tFoot) this.removeChild(this.tFoot);
}

TextNode = function() {
this.data$2 = "";
if(arguments.length > 0) {
// data always has to be a string
this.data$2 += arguments[0];
}
this.nodeName = this.tagName = "#text";
this.nodeType = 3;
this.ownerDocument = my$doc();
this.style = new CSSStyleDeclaration;
this.style.element = this;
this.class = "";
/* A text node chould never have children, and does not need childNodes array,
 * but there is improper html out there <text> <stuff> </text>
 * which has to put stuff under the text node, so against this
 * unlikely occurence, I have to create the array.
 * I have to treat a text node like an html node. */
this.childNodes = [];
this.parentNode = null;
this.attributes = new NamedNodeMap;
this.attributes.owner = this;
}
TextNode.prototype.dom$class = "TextNode";

// setter insures data is always a string, because roving javascript might
// node.data = 7;  ...  if(node.data.match(/x/) ...
// and boom! It blows up because Number doesn't have a match function.
Object.defineProperty(TextNode.prototype, "data", {
get: function() { return this.data$2; },
set: function(s) { this.data$2 = s + ""; }});

document.createTextNode = function(t) {
if(t == undefined) t = "";
var c = new TextNode(t);
eb$logElement(c, "text");
return c;
}

Comment = function(t) {
this.data = t;
this.nodeName = this.tagName = "#comment";
this.nodeType = 8;
this.ownerDocument = my$doc();
this.class = "";
this.childNodes = [];
this.parentNode = null;
}
Comment.prototype.dom$class = "Comment";

document.createComment = function(t) {
if(t == undefined) t = "";
var c = new Comment(t);
eb$logElement(c, "comment");
return c;
}

// The Option class, these are choices in a dropdown list.
Option = function() {
this.nodeName = "OPTION";
this.text = this.value = "";
if(arguments.length > 0)
this.text = arguments[0];
if(arguments.length > 1)
this.value = arguments[1];
this.selected = false;
this.defaultSelected = false;
}
Option.prototype.dom$class = "Option";

document.getBoundingClientRect = function(){
return {
top: 0, bottom: 0, left: 0, right: 0,
x: 0, y: 0,
width: 0, height: 0
}
}

// implementation of getElementsByTagName, getElementsByName, and getElementsByClassName.
// These are recursive as they descend through the tree of nodes.

document.getElementsByTagName = function(s) { 
if(!s) { // missing or null argument
alert3("getElementsByTagName(type " + typeof s + ")");
return [];
}
s = s.toLowerCase();
return dom$.eb$gebtn(this, s);
}

dom$.eb$gebtn = function(top, s) { 
var a = [];
if(s === '*' || (top.nodeName && top.nodeName.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
// don't descend into another frame.
// Look for iframe.document.html, meaning the frame is expanded.
if(top.dom$class != "Frame" || !top.firstChild.firstChild)
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(dom$.eb$gebtn(c, s));
}
}
return a;
}

document.getElementsByName = function(s) { 
if(!s) { // missing or null argument
alert3("getElementsByName(type " + typeof s + ")");
return [];
}
s = s.toLowerCase();
return dom$.eb$gebn(this, s);
}

dom$.eb$gebn = function(top, s) { 
var a = [];
if(s === '*' || (top.name && top.name.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame" || !top.firstChild.firstChild)
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(dom$.eb$gebn(c, s));
}
}
return a;
}

document.getElementById = function(s) { 
if(!s) { // missing or null argument
alert3("getElementById(type " + typeof s + ")");
return null;
}
s = s.toLowerCase();
var a = dom$.eb$gebi(this, s);
return a.length ? a[0] : null;
}

// this could stop when it finds the first match, it just doesn't
dom$.eb$gebi = function(top, s) { 
var a = [];
if(s === '*' || (top.id && top.id.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame" || !top.firstChild.firstChild)
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(dom$.eb$gebi(c, s));
}
}
return a;
}

document.getElementsByClassName = function(s) { 
s = s.toLowerCase();
return dom$.eb$gebcn(this, s);
}

dom$.eb$gebcn = function(top, s) { 
var a = [];
if(s === '*' || (top.class && top.class.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame" || !top.firstChild.firstChild)
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(dom$.eb$gebcn(c, s));
}
}
return a;
}

document.nodeContains = function(n) {  return dom$.eb$cont(this, n); }

dom$.eb$cont = function(top, n) { 
if(top === n) return true;
if(!top.childNodes) return false;
if((top.dom$class == "Frame") &&top.firstChild.firstChild) return false;
for(var i=0; i<top.childNodes.length; ++i)
if(dom$.eb$cont(top.childNodes[i], n)) return true;
return false;
}

/*********************************************************************
If you append a documentFragment you're really appending all its kids.
This is called by the various appendChild routines.
Since we are appending many nodes, I'm not sure what to return.
*********************************************************************/

dom$.appendFragment = function(p, frag) { var c; while(c = frag.firstChild) p.appendChild(c); return null; }
dom$.insertFragment = function(p, frag, l) { var c; while(c = frag.firstChild) p.insertBefore(c, l); return null; }

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
These functions also check for a hierarchy error using isabove().
In fact we may as well throw the exception here.
*********************************************************************/

dom$.isabove = function(a, b) {
var j = 0;
while(b) {
if(b == a) { var e = new Error; e.HIERARCHY_REQUEST_ERR = e.code = 3; throw e; }
if(++j == 1000) { alert3("isabove loop"); break; }
b = b.parentNode;
}
}

dom$.treeBump = function(t) { if(t.ownerDocument) ++t.ownerDocument.tree$n; }

document.appendChild = function(c) {
if(!c) return null;
if(c.nodeType == 11) return dom$.appendFragment(this, c);
dom$.isabove(c, this);
if(c.parentNode) c.parentNode.removeChild(c);
var r = this.eb$apch2(c);
mutFixup(this, false, c, null);
return r;
}

document.prependChild = function(c) {
dom$.isabove(c, this);
if(this.childNodes.length) this.insertBefore(c, this.childNodes[0]);
else this.appendChild(c);
}

document.insertBefore = function(c, t) {
if(!c) return null;
if(!t) return this.appendChild(c);
dom$.isabove(c, this);
if(c.nodeType == 11) return dom$.insertFragment(this, c, t);
if(c.parentNode) c.parentNode.removeChild(c);
var r = this.eb$insbf(c, t);
mutFixup(this, false, r, null);
return r;
}

document.replaceChild = function(newc, oldc) {
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

document.hasChildNodes = function() { return (this.childNodes.length > 0); }

dom$.eb$getSibling = function (obj,direction) {
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

dom$.eb$getElementSibling = function (obj,direction) {
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

// The Attr class and getAttributeNode().
Attr = function(){ this.specified = false; this.owner = null; this.name = ""};
Attr.prototype.dom$class = "Attr";
Object.defineProperty(Attr.prototype, "value", {
get: function() { var n = this.name;
return n.substr(0,5) == "data-" ? (this.owner.dataset ? this.owner.dataset[dom$.dataCamel(n)] :  null)  : this.owner[n]; },
set: function(v) {
this.owner.setAttribute(this.name, v);
this.specified = true;
return;
}});
Attr.prototype.isId = function() { return this.name === "id"; }

// this is sort of an array and sort of not
NamedNodeMap = function() { this.length = 0};
NamedNodeMap.prototype.dom$class = "NamedNodeMap";
NamedNodeMap.prototype.push = function(s) { this[this.length++] = s; }
NamedNodeMap.prototype.item = function(n) { return this[n]; }
NamedNodeMap.prototype.getNamedItem = function(name) { return this[name.toLowerCase()]; }
NamedNodeMap.prototype.setNamedItem = function(name, v) { this.owner.setAttribute(name, v);}
NamedNodeMap.prototype.removeNamedItem = function(name) { this.owner.removeAttribute(name);}

dom$.implicitMember = function(o, name) {
return name === "elements" && o.dom$class == "Form" ||
name === "rows" && (o.dom$class == "Table" || o.dom$class == "tBody" || o.dom$class == "tHead" || o.dom$class == "tFoot") ||
name === "tBodies" && o.dom$class == "Table" ||
name === "cells" && o.dom$class == "tRow" ||
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

document.getAttribute = function(name) {
name = name.toLowerCase();
if(dom$.implicitMember(this, name)) return null;
// has to be a real attribute
if(!this.attributes) return null;
if(!this.attributes[name]) return null;
var v = this.attributes[name].value;
if(v.dom$class == "URL") return v.toString();
var t = typeof v;
if(t == "undefined") return null;
// possibly any object should run through toString(), as we did with URL, idk
return v; }
document.hasAttribute = function(name) { return this.getAttribute(name) !== null; }

document.getAttributeNS = function(space, name) {
if(space && !name.match(/:/)) name = space + ":" + name;
return this.getAttribute(name);
}
document.hasAttributeNS = function(space, name) { return this.getAttributeNS(space, name) !== null;}

document.setAttribute = function(name, v) { 
name = name.toLowerCase();
// special code for style
if(name == "style" && this.style.dom$class == "CSSStyleDeclaration") {
this.style.cssText = v;
return;
}
if(dom$.implicitMember(this, name)) return;
var oldv = null;
if(!this.attributes) this.attributes = new NamedNodeMap;
if(!this.attributes[name]) {
var a = new Attr();
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
if(!this.dataset) this.dataset = {};
this.dataset[dom$.dataCamel(name)] = v;
} else this[name] = v; 
}
mutFixup(this, true, name, oldv);
}
document.markAttribute = function(name) { this.setAttribute(name, "from@@html"); }
document.setAttributeNS = function(space, name, v) {
if(space && !name.match(/:/)) name = space + ":" + name;
this.setAttribute(name, v);
}

document.removeAttribute = function(name) {
    name = name.toLowerCase();
// special code for style
if(name == "style" && this.style.dom$class == "CSSStyleDeclaration") {
// wow I have no clue what this means but it happens, https://www.maersk.com
return;
}
var oldv = null;
if(name.substr(0,5) == "data-") {
var n = dom$.dataCamel(name);
if(this.dataset && this.dataset[n]) { oldv = this.dataset[n]; delete this.dataset[n]; }
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
document.removeAttributeNS = function(space, name) {
if(space && !name.match(/:/)) name = space + ":" + name;
this.removeAttribute(name);
}

document.getAttributeNode = function(name) {
    name = name.toLowerCase();
// this returns null if no such attribute, is that right,
// or should we return a new Attr node with no value?
return this.attributes[name] ? this.attributes[name] : null;
/*
a = new Attr;
a.owner = this;
a.name = name;
return a;
*/
}

/*********************************************************************
This creates a copy of the node and its children recursively.
The argument 'deep' refers to whether or not the clone will recurs.
eb$clone is a helper function that is not tied to any particular prototype.
It's frickin complicated, so set cloneDebug to debug it.
*********************************************************************/

cloneDebug = false;

dom$.eb$clone = function(node1,deep) {
var node2;
var i, j;
var kids = null;
var debug = my$win().cloneDebug;

if(node1.dom$class == "CSSStyleDeclaration") {
if(debug) alert3("copy style");
node2 = new CSSStyleDeclaration;
for (var item in node1){
if(!node1.hasOwnProperty(item)) continue;
if (typeof node1[item] === 'string' ||
typeof node1[item] === 'number') {
if(debug) alert3("copy stattr " + item);
node2[item] = node1[item];
}
}
return node2;
}

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
node2 = document.createTextNode();
else if(node1.nodeName == "#comment")
node2 = document.createComment();
else if(node1.nodeName == "#document-fragment")
node2 = document.createDocumentFragment();
else
node2 = document.createElement(node1.nodeName);
if(node1 == dom$.cloneRoot1) dom$.cloneRoot2 = node2;

if (deep && kids) {
for(i = 0; i < kids.length; ++i) {
var current_item = kids[i];
node2.appendChild(dom$.eb$clone(current_item,true));
}
}

var lostElements = false;

// now for strings and functions and stuff.
for (var item in node1) {
// don't copy the things that come from prototype
if(!node1.hasOwnProperty(item)) continue;

// children already handled
if(item === "childNodes" || item === "parentNode") continue;

if (typeof node1[item] === 'function') {
if(debug) alert3("copy function " + item);
node2[item] = node1[item];
continue;
}

if(node1[item] === node1) {
if(debug) alert3("selflink through " + item);
node2[item] = node2;
continue;
}

// An array of event handlers etc.
if(Array.isArray(node1[item])) {

/*********************************************************************
Ok we need some special code here for form.elements,
an array of input nodes within the form.
We are preserving links, rather like tar or cpio.
The same must be done for an array of rows beneath <table>,
or an array of cells in a row, and perhaps others.
But the thing is, we don't have to do that, because appendChild
does it for us, as side effects, for these various classes.
*********************************************************************/

if(dom$.implicitMember(node1, item)) continue;

node2[item] = [];

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
var p = dom$.findObject(a1[i]);
if(p.length) {
a2.push(dom$.correspondingObject(p));
} else {
a2.push(null);
if(debug) alert3("oops, button " + i + " not linked");
}
}
continue;
}

// It's a regular array.
if(debug) alert3("copy array " + item + " with " + node1[item].length + " members");
node2[item] = [];
for(i = 0; i < node1[item].length; ++i) {
node2[item].push(node1[item][i]);
}
continue;
}

if(typeof node1[item] === "object") {
// An object, not an array.

if(item === "style") continue; // handled later
if(item === "attributes") continue; // handled later
if(item === "ownerDocument") continue; // handled by createElement

// Check for URL objects.
if(node1[item].dom$class == "URL") {
var u = node1[item];
if(debug) alert3("copy URL " + item);
node2[item] = new URL(u.href);
continue;
}

// Look for a link from A to B within the tree of nodes,
// A.foo = B, and try to preserve that link in the new tree, A1.foo = B1,
// rather like tar or cpio preserving hard links.
var p = dom$.findObject(node1[item]);
if(p.length) {
if(debug) alert3("link " + item + " " + p);
node2[item] = dom$.correspondingObject(p);
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
if(showstring.length > 20) showstring = "long";
alert3("copy string " + item + " = " + showstring);
}
node2[item] = node1[item];
continue;
}

if (typeof node1[item] === 'number') {
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
if (node1.style.dom$class == "CSSStyleDeclaration") {
node2.style = dom$.eb$clone(node1.style, false);
node2.style.element = node2;
}

if (node1.attributes.dom$class == "NamedNodeMap") {
if(debug) alert3("copy attributes");
node2.attributes = new NamedNodeMap;
node2.attributes.owner = node2;
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

document.cloneNode = function(deep) {
dom$.cloneRoot1 = this;
return dom$.eb$clone (this,deep);
}

// Look recursively down the tree for an object.
// This is a helper function for cloneNode.
dom$.findObject = function(t) {
var p = "";
while(t != dom$.cloneRoot1) {
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
dom$.correspondingObject = function(p) {
var c = dom$.cloneRoot2;
p = p.substr(1);
while(p) {
var j = p.replace(/,.*/, "");
if(!c.childNodes || j >= c.childNodes.length) return "";
c = c.childNodes[j];
p = p.replace(/^\d+,/, "");
}
return c;
}

/*********************************************************************
importNode seems to be the same as cloneNode, except it is copying a tree
of objects from another context into the current context.
But this is how duktape works by default.
foo.s = cloneNode(bar.s);
If bar is in another context that's ok, we read those objects and create
copies of them in the current context.
*********************************************************************/

document.importNode = function(src, deep) { return src.cloneNode(deep); }

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
*********************************************************************/

document.compareDocumentPosition = function(w) {
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

// classList
// First the functions that will hang off the array to be returned.
dom$.classListRemove = function() {
for(var i=0; i<arguments.length; ++i) {
for(var j=0; j<this.length; ++j) {
if(arguments[i] != this[j]) continue;
this.splice(j, 1);
--j;
}
}
this.node.class = this.join(' ');
}

dom$.classListAdd = function() {
for(var i=0; i<arguments.length; ++i) {
for(var j=0; j<this.length; ++j)
if(arguments[i] == this[j]) break;
if(j == this.length) this.push(arguments[i]);
}
this.node.class = this.join(' ');
}

dom$.classListReplace = function(o, n) {
if(!o) return;
if(!n) { this.remove(o); return; }
for(var j=0; j<this.length; ++j)
if(o == this[j]) { this[j] = n; break; }
this.node.class = this.join(' ');
}

dom$.classListContains = function(t) {
if(!t) return false;
for(var j=0; j<this.length; ++j)
if(t == this[j]) return true;
return false;
}

dom$.classListToggle = function(t, force) {
if(!t) return false;
if(arguments.length > 1) {
if(force) this.add(t); else this.remove(t);
return force;
}
if(this.contains(t)) { this.remove(t); return false; }
this.add(t); return true;
}

dom$.classList = function(node) {
var c = node.class;
if(!c) c = "";
// turn string into array
var a = c.replace(/^\s+/, "").replace(/\s+$/, "").split(/\s+/);
// remember the node you came from
a.node = node;
// attach functions
a.remove = dom$.classListRemove;
a.add = dom$.classListAdd;
a.replace = dom$.classListReplace;
a.contains = dom$.classListContains;
a.toggle = dom$.classListToggle;
return a;
}

dom$.ehsn = 0; // event handler sequence number

// The Event class and various handlers.
Event = function(etype){
    // event state is kept read-only by forcing
    // a new object for each event.  This may not
    // be appropriate in the long run and we'll
    // have to decide if we simply dont adhere to
    // the read-only restriction of the specification
    this.bubbles = true;
    this.cancelable = true;
    this.cancelled = false;
    this.currentTarget = null;
    this.target = null;
    this.eventPhase = 0;
    this.timeStamp = new Date().getTime();
this.defaultPrevented = false;
if(typeof etype == "string") this.type = etype;
};
Event.prototype.dom$class = "Event";

Event.prototype.preventDefault = function(){ this.defaultPrevented = true; }

Event.prototype.stopPropagation = function(){ if(this.cancelable)this.cancelled = true; }

// deprecated!
Event.prototype.initEvent = function(t, bubbles, cancel) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel; this.defaultPrevented = false; }

Event.prototype.initUIEvent = function(t, bubbles, cancel, unused, detail) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel, this.detail = detail; this.defaultPrevented = false; }

Event.prototype.initCustomEvent = function(t, bubbles, cancel, detail) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel, this.detail = detail; }

document.createEvent = function(unused) { return new Event; }

document.dispatchEvent = function (e) {
if(my$win().eventDebug) alert3("dispatch " + this.nodeName + " tag " + (this.eb$seqno?this.eb$seqno:"?") + " " + e.type);
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
var fn = "on" + e.type;
if(typeof t[fn] == "function") {
if(my$win().eventDebug) alert3((l?"capture ":"current ") + t.nodeName + "." + e.type);
e.currentTarget = t;
if(!t[fn + "$$array"] && my$win().eventDebug) alert3("fire assigned");
var r = t[fn](e);
if(!t[fn + "$$array"] && my$win().eventDebug) alert3("endfire assigned");
if((typeof r == "boolean" || typeof r == "number") && !r) return false;
if(e.cancelled) return !e.defaultPrevented;
}
}
if(!e.bubbles) return !e.defaultPrevented;
++l; // step up from the target
while(l < pathway.length) {
t = pathway[l++];
e.eventPhase = 3;
var fn = "on" + e.type;
if(typeof t[fn] == "function") {
// If function was just put here, not part of addEventListener,
// don't run it on the second phase or you're running it twice.
if(!t[fn + "$$array"]) continue;
if(my$win().eventDebug) alert3("bubble " + t.nodeName + "." + e.type);
e.currentTarget = t;
var r = t[fn](e);
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

dom$.attachOn = true;
eventDebug = false;

addEventListener = document.addEventListener = function(ev, handler, iscapture) { this.eb$listen(ev,handler, iscapture, true); }
removeEventListener = document.removeEventListener = function(ev, handler, iscapture) { this.eb$unlisten(ev,handler, iscapture, true); }
if(dom$.attachOn) {
attachEvent = document.attachEvent = function(ev, handler) { this.eb$listen(ev,handler, true, false); }
detachEvent = document.detachEvent = function(ev, handler) { this.eb$unlisten(ev,handler, true, false); }
}

eb$listen = document.eb$listen = function(ev, handler, iscapture, addon) {
if(addon) ev = "on" + ev;
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
alert3((addon ? "listen " : "attach ") + this.nodeName + "." + ev.replace(/^on/,'') + " for " + (iscap?"capture":"bubble") + " with null handler");
return;
}
if(iscap) handler.do$capture = true; else handler.do$bubble = true;
if(once) handler.do$once = true;
if(passive) handler.do$passive = true;
// event handler serial number, for debugging
if(!handler.ehsn) handler.ehsn = ++dom$.ehsn;
if(my$win().eventDebug)  alert3((addon ? "listen " : "attach ") + this.nodeName + "." + ev.replace(/^on/,'') + " tag " + (this.eb$seqno ? this.eb$seqno : -1) + " handler " + handler.ehsn + " for " + (handler.do$capture?"capture":"bubble"));
var evarray = ev + "$$array"; // array of handlers
var evorig = ev + "$$orig"; // original handler from html

if(!this[evarray]) {
/* attaching the first handler */
var a = [];
/* was there already a function from before? */
var prev_fn = this[ev];
if(prev_fn) this[evorig] = prev_fn;

// Is thiis tag this event being watched?
var extension = "";
if(this[ev+"$$watch"]) extension = "$2";

eval(
'this["' + ev + extension + '"] = function(e){ var rc, a = this["' + evarray + '"]; \
if(this["' + evorig + '"] && e.eventPhase < 3) { \
var ehsn = this["' + evorig + '"].ehsn; \
if(ehsn) ehsn = "" + ehsn; else ehsn = ""; /* from int to string */ \
alert3("fire orig tag " + (this.eb$seqno ? this.eb$seqno : -1) + (ehsn.length ? " handler " + ehsn : "")); rc = this["' + evorig + '"](e); alert3("endfire handler " + ehsn); \
if((typeof rc == "boolean" || typeof rc == "number") && !rc) return false; } \
for(var i = 0; i<a.length; ++i) a[i].did$run = false; \
for(var i = 0; i<a.length; ++i) { var h = a[i];if(h.did$run) continue; \
if(e.eventPhase== 1 && !h.do$capture || e.eventPhase == 3 && !h.do$bubble) continue; \
var ehsn = h.ehsn; \
if(ehsn) ehsn = "" + ehsn; else ehsn = ""; /* from int to string */ \
h.did$run = true; alert3("fire tag " + (this.eb$seqno ? this.eb$seqno : -1) + (ehsn.length ? " handler " + ehsn : "")); rc = h.call(this,e); alert3("endfire handler " + ehsn); \
if(h.do$once) { alert3("once"); this.removeEventListener(e.type, h, h.do$capture); } \
if((typeof rc == "boolean" || typeof rc == "number") && !rc) return false; \
i = -1; \
} return true; };');

this[evarray] = a;
}

this[evarray].push(handler);
}

// here is unlisten, the opposite of listen.
// what if every handler is removed and there is an empty array?
// the assumption is that this is not a problem.
eb$unlisten = document.eb$unlisten = function(ev, handler, iscapture, addon) {
var ehsn = (handler.ehsn ? handler.ehsn : 0);
if(addon) ev = "on" + ev;
if(my$win().eventDebug)  alert3((addon ? "unlisten " : "detach ") + this.nodeName + "." + ev.replace(/^on/,'') + " tag " + (this.eb$seqno ? this.eb$seqno : -1) + " handler " + ehsn);

var evarray = ev + "$$array"; // array of handlers
var evorig = ev + "$$orig"; // original handler from html
// remove original html handler after other events have been added.
if(this[evorig] == handler) {
delete this[evorig];
return;
}
// remove original html handler when no other events have been added.
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

MediaQueryList = function() {
this.nodeName = "MediaQueryList";
this.matches = false;
this.media = "";
this.addEventListener = addEventListener;
this.removeEventListener = removeEventListener;
// supporting the above:
this.eb$listen = eb$listen;
this.eb$unlisten = eb$unlisten;
this.addListener = function(f) { this.addEventListener("mediaChange", f, false); }
this.removeListener = function(f) { this.removeEventListener("mediaChange", f, false); }
}
MediaQueryList.prototype.dom$class = "MediaQueryList";

matchMedia = function(s) {
var q = new MediaQueryList;
q.media = s;
q.matches = eb$media(s);
return q;
}

document.insertAdjacentHTML = function(flavor, h) {
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

dom$.htmlString = function(t) {
if(t.nodeType == 3) return t.data;
if(t.nodeType != 1) return "";
var s = "<" + (t.nodeName ? t.nodeName : "x");
if(t.class) s += ' class="' + t.class + '"';
if(t.id) s += ' id="' + t.id + '"';
s += '>';
if(t.childNodes)
for(var i=0; i<t.childNodes.length; ++i)
s += dom$.htmlString(t.childNodes[i]);
s += "</";
s += (t.nodeName ? t.nodeName : "x");
s += '>';
return s;
}

dom$.outer$1 = function(t, h) {
var p = t.parentNode;
if(!p) return;
t.innerHTML = h;
while(t.lastChild) p.insertBefore(t.lastChild, t.nextSibling);
p.removeChild(t);
}

// There are subtle differences between contentText and textContent, which I don't grok.
dom$.textUnder = function(top, flavor) {
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

dom$.newTextUnder = function(top, s, flavor) {
var l = top.childNodes.length;
for(var i=l-1; i>=0; --i)
top.removeChild(top.childNodes[i]);
top.appendChild(document.createTextNode(s));
}

/*********************************************************************
Add prototype methods to the standard nodes, nodes that have children,
and the normal set of methods to go with those children.
Form has children for sure, but if we add <input> to Form,
we also have to add it to the array Form.elements.
So there are some nodes that we have to do outside this loop.
Again, leading ; to avert a parsing ambiguity.
*********************************************************************/

; (function() {
var cnlist = ["HTML", "HtmlObj", "Head", "Title", "Body", "CSSStyleDeclaration", "Frame",
"Anchor", "Element","HTMLElement", "Select", "Lister", "Listitem", "tBody", "Table", "Div",
"HTMLAnchorElement", "HTMLLinkElement", "HTMLAreaElement",
"tHead", "tFoot", "tCap", "Label",
"Form", "Span", "tRow", "Cell", "P", "Script", "Header", "Footer",
// The following nodes shouldn't have any children, but the various
// children methods could be called on them anyways.
// And getAttribute applies to just about everything.
"Comment", "Node", "Area", "TextNode", "Image", "Option", "Link", "Meta", "Audio", "Canvas"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
var c = window[cn];
// c is class and cn is classname.
// get elements below
c.prototype.getElementsByTagName = document.getElementsByTagName;
c.prototype.getElementsByName = document.getElementsByName;
c.prototype.getElementsByClassName = document.getElementsByClassName;
c.prototype.contains = document.nodeContains;
c.prototype.querySelectorAll = querySelectorAll;
c.prototype.querySelector = querySelector;
c.prototype.matches = querySelector0;
// children
c.prototype.hasChildNodes = document.hasChildNodes;
c.prototype.appendChild = document.appendChild;
c.prototype.prependChild = document.prependChild;
c.prototype.insertBefore = document.insertBefore;
c.prototype.replaceChild = document.replaceChild;
// These are native, so it's ok to bounce off of document.
c.prototype.eb$apch1 = document.eb$apch1;
c.prototype.eb$apch2 = document.eb$apch2;
c.prototype.eb$insbf = document.eb$insbf;
c.prototype.removeChild = document.removeChild;
Object.defineProperty(c.prototype, "firstChild", { get: function() { return (this.childNodes && this.childNodes.length) ? this.childNodes[0] : null; } });
Object.defineProperty(c.prototype, "firstElementChild", { get: function() { var u = this.childNodes; if(!u) return null; for(var i=0; i<u.length; ++i) if(u[i].nodeType == 1) return u[i]; return null; }});
Object.defineProperty(c.prototype, "lastChild", { get: function() { return (this.childNodes && this.childNodes.length) ? this.childNodes[this.childNodes.length-1] : null; } });
Object.defineProperty(c.prototype, "lastElementChild", { get: function() { var u = this.childNodes; if(!u) return null; for(var i=u.length-1; i>=0; --i) if(u[i].nodeType == 1) return u[i]; return null; }});
Object.defineProperty(c.prototype, "nextSibling", { get: function() { return dom$.eb$getSibling(this,"next"); } });
Object.defineProperty(c.prototype, "nextElementSibling", { get: function() { return dom$.eb$getElementSibling(this,"next"); } });
Object.defineProperty(c.prototype, "previousSibling", { get: function() { return dom$.eb$getSibling(this,"previous"); } });
Object.defineProperty(c.prototype, "previousElementSibling", { get: function() { return dom$.eb$getElementSibling(this,"previous"); } });
// children is subtly different from childnodes; this code taken from
// https://developer.mozilla.org/en-US/docs/Web/API/ParentNode/children
Object.defineProperty(c.prototype, 'children', {
get: function() {
var i = 0, node, nodes = this.childNodes, children = [];
if(!nodes) return children;
while(i<nodes.length) {
node = nodes[i++];
if (node.nodeType === 1)  children.push(node);
}
return children;
}});
// attributes
c.prototype.hasAttribute = document.hasAttribute;
c.prototype.hasAttributeNS = document.hasAttributeNS;
c.prototype.markAttribute = document.markAttribute;
c.prototype.getAttribute = document.getAttribute;
c.prototype.getAttributeNS = document.getAttributeNS;
c.prototype.setAttribute = document.setAttribute;
c.prototype.setAttributeNS = document.setAttributeNS;
c.prototype.removeAttribute = document.removeAttribute;
c.prototype.removeAttributeNS = document.removeAttributeNS;
/* which one is it?
Object.defineProperty(c.prototype, "className", { get: function() { return this.getAttribute("class"); }, set: function(h) { this.setAttribute("class", h); }});
*/
Object.defineProperty(c.prototype, "className", { get: function() { return this.class; }, set: function(h) { this.class = h; }});
Object.defineProperty(c.prototype, "parentElement", { get: function() { return this.parentNode && this.parentNode.nodeType == 1 ? this.parentNode : null; }});
c.prototype.getAttributeNode = document.getAttributeNode;
c.prototype.getClientRects = function(){ return []; }
// clone
c.prototype.cloneNode = document.cloneNode;
c.prototype.importNode = document.importNode;
c.prototype.compareDocumentPosition = dom$.compareDocumentPosition;
// visual
c.prototype.focus = focus;
c.prototype.blur = blur;
c.prototype.getBoundingClientRect = document.getBoundingClientRect; 
// events
c.prototype.eb$listen = eb$listen;
c.prototype.eb$unlisten = eb$unlisten;
c.prototype.addEventListener = addEventListener;
c.prototype.removeEventListener = removeEventListener;
if(dom$.attachOn) {
c.prototype.attachEvent = dom$.attachEvent;
c.prototype.detachEvent = detachEvent;
}
c.prototype.dispatchEvent = document.dispatchEvent;
c.prototype.insertAdjacentHTML = document.insertAdjacentHTML;
// outerHTML is dynamic; should innerHTML be?
Object.defineProperty(c.prototype, "outerHTML", { get: function() { return dom$.htmlString(this);},
set: function(h) { dom$.outer$1(this,h); }});
// constants
c.prototype.ELEMENT_NODE = 1, c.prototype.TEXT_NODE = 3, c.prototype.COMMENT_NODE = 8, c.prototype.DOCUMENT_NODE = 9, c.prototype.DOCUMENT_TYPE_NODE = 10, c.prototype.DOCUMENT_FRAGMENT_NODE = 11;
Object.defineProperty(c.prototype, "classList", { get : function() { return dom$.classList(this);}});
Object.defineProperty(c.prototype, "textContent", {
get: function() { return dom$.textUnder(this, 0); },
set: function(s) { return dom$.newTextUnder(this, s, 0); }});
Object.defineProperty(c.prototype, "contentText", {
get: function() { return dom$.textUnder(this, 1); },
set: function(s) { return dom$.newTextUnder(this, s, 1); }});
Object.defineProperty(c.prototype, "nodeValue", {
get: function() { return this.nodeType == 3 ? this.data : null;},
set: function(h) { if(this.nodeType == 3) this.data = h; }});
if(c !== Body) {
c.prototype.clientHeight = 16;
c.prototype.clientWidth = 120;
c.prototype.scrollHeight = 16;
c.prototype.scrollWidth = 120;
c.prototype.scrollTop = 0;
c.prototype.scrollLeft = 0;
}
c.prototype.offsetHeight = 16;
c.prototype.offsetWidth = 120;
}
})();

/*********************************************************************
As promised, Form is weird.
If you add an input to a form, it adds under childNodes in the usual way,
but also must add in the elements[] array.
Same for insertBefore and removeChild.
When adding an input element to a form,
linnk form[element.name] to that element.
*********************************************************************/

dom$.eb$formname = function(parent, child) {
var s;
if(typeof child.name === "string")
s = child.name;
else if(typeof child.id === "string")
s = child.id;
else return;
if(!parent[s]) parent[s] = child;
if(!parent.elements[s]) parent.elements[s] = child;
}

Form.prototype.appendChildNative = document.appendChild;
Form.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return dom$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.nodeName === "INPUT" || newobj.nodeName === "SELECT") {
this.elements.push(newobj);
newobj.form = this;
dom$.eb$formname(this, newobj);
}
return newobj;
}
Form.prototype.insertBeforeNative = document.insertBefore;
Form.prototype.insertBefore = function(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return dom$.insertFragment(this, newobj, item);
var r = this.insertBeforeNative(newobj, item);
if(!r) return null;
if(newobj.nodeName === "INPUT" || newobj.nodeName === "SELECT") {
for(var i=0; i<this.elements.length; ++i)
if(this.elements[i] == item) {
this.elements.splice(i, 0, newobj);
break;
}
newobj.form = this;
dom$.eb$formname(this, newobj);
}
return newobj;
}
Form.prototype.removeChildNative = document.removeChild;
Form.prototype.removeChild = function(item) {
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

/*********************************************************************
Look out! Select class maintains an array of options beneath,
just as Form maintains an array of elements beneath, so you'd
think we could copy the above code and tweak a few things, but no.
Options under select lists are maintained by rebuildSelectors in ebjs.c.
That is how we synchronize option lists.
So we don't want to synchronize by side-effects.
In other words, we don't want to pass the actions back to edbrowse,
as appendChild does. So I kinda have to reproduce what they do
here, with just js, and no action in C.
*********************************************************************/

Select.prototype.appendChild = function(newobj) {
if(!newobj) return null;
// should only be options!
if(!(newobj.dom$class == "Option")) return newobj;
dom$.isabove(newobj, this);
if(newobj.parentNode) newobj.parentNode.removeChild(newobj);
var l = this.childNodes.length;
if(newobj.defaultSelected) newobj.selected = true, this.selectedIndex = l;
this.childNodes.push(newobj); newobj.parentNode = this;
mutFixup(this, false, newobj, null);
return newobj;
}
Select.prototype.insertBefore = function(newobj, item) {
var i;
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(!(newobj.dom$class == "Option")) return newobj;
dom$.isabove(newobj, this);
if(newobj.parentNode) newobj.parentNode.removeChild(newobj);
for(i=0; i<this.childNodes.length; ++i)
if(this.childNodes[i] == item) {
this.childNodes.splice(i, 0, newobj); newobj.parentNode = this;
if(newobj.defaultSelected) newobj.selected = true, this.selectedIndex = i;
break;
}
if(i == this.childNodes.length) {
// side effect, object is freeed from wherever it was.
return null;
}
mutFixup(this, false, newobj, null);
return newobj;
}
Select.prototype.removeChild = function(item) {
var i;
if(!item) return null;
for(i=0; i<this.childNodes.length; ++i)
if(this.childNodes[i] == item) {
this.childNodes.splice(i, 1);
item.parentNode = null;
break;
}
if(i == this.childNodes.length) return null;
mutFixup(this, false, i, item);
return item;
}

Select.prototype.add = function(o, idx) {
var n = this.options.length;
if(typeof idx != "number" || idx < 0 || idx > n) idx = n;
if(idx == n) this.appendChild(o);
else this.insertBefore(o, this.options[idx]);
}
Select.prototype.remove = function(idx) {
var n = this.options.length;
if(typeof idx == "number" && idx >= 0 && idx < n)
this.removeChild(this.options[idx]);
}

// rows under a table body
tBody.prototype.appendChildNative = document.appendChild;
tBody.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return dom$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.dom$class == "tRow") // shouldn't be anything other than TR
this.rows.push(newobj), rowReindex(this);
return newobj;
}
tBody.prototype.insertBeforeNative = document.insertBefore;
tBody.prototype.insertBefore = function(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return dom$.insertFragment(this, newobj, item);
var r = this.insertBeforeNative(newobj, item);
if(!r) return null;
if(newobj.dom$class == "tRow")
for(var i=0; i<this.rows.length; ++i)
if(this.rows[i] == item) {
this.rows.splice(i, 0, newobj);
rowReindex(this);
break;
}
return newobj;
}
tBody.prototype.removeChildNative = document.removeChild;
tBody.prototype.removeChild = function(item) {
if(!item) return null;
if(!this.removeChildNative(item))
return null;
if(item.dom$class == "tRow")
for(var i=0; i<this.rows.length; ++i)
if(this.rows[i] == item) {
this.rows.splice(i, 1);
rowReindex(this);
break;
}
return item;
}

// head and foot are just like body
tHead.prototype.appendChildNative = document.appendChild;
tHead.prototype.appendChild = tBody.prototype.appendChild;
tHead.prototype.insertBeforeNative = document.insertBefore;
tHead.prototype.insertBefore = tBody.prototype.insertBefore;
tHead.prototype.removeChildNative = document.removeChild;
tHead.prototype.removeChild = tBody.prototype.removeChild;
tFoot.prototype.appendChildNative = document.appendChild;
tFoot.prototype.appendChild = tBody.prototype.appendChild;
tFoot.prototype.insertBeforeNative = document.insertBefore;
tFoot.prototype.insertBefore = tBody.prototype.insertBefore;
tFoot.prototype.removeChildNative = document.removeChild;
tFoot.prototype.removeChild = tBody.prototype.removeChild;

// rows or bodies under a table
Table.prototype.appendChildNative = document.appendChild;
Table.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return dom$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.dom$class == "tRow") rowReindex(this);
if(newobj.dom$class == "tBody") {
this.tBodies.push(newobj);
if(newobj.rows.length) rowReindex(this);
}
if(newobj.dom$class == "tCap") this.caption = newobj;
if(newobj.dom$class == "tHead") {
this.tHead = newobj;
if(newobj.rows.length) rowReindex(this);
}
if(newobj.dom$class == "tFoot") {
this.tFoot = newobj;
if(newobj.rows.length) rowReindex(this);
}
return newobj;
}
Table.prototype.insertBeforeNative = document.insertBefore;
Table.prototype.insertBefore = function(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return dom$.insertFragment(this, newobj, item);
var r = this.insertBeforeNative(newobj, item);
if(!r) return null;
if(newobj.dom$class == "tRow") rowReindex(this);
if(newobj.dom$class == "tBody")
for(var i=0; i<this.tBodies.length; ++i)
if(this.tBodies[i] == item) {
this.tBodies.splice(i, 0, newobj);
if(newobj.rows.length) rowReindex(this);
break;
}
if(newobj.dom$class == "tCap") this.caption = newobj;
if(newobj.dom$class == "tHead") {
this.tHead = newobj;
if(newobj.rows.length) rowReindex(this);
}
if(newobj.dom$class == "tFoot") {
this.tFoot = newobj;
if(newobj.rows.length) rowReindex(this);
}
return newobj;
}
Table.prototype.removeChildNative = document.removeChild;
Table.prototype.removeChild = function(item) {
if(!item) return null;
if(!this.removeChildNative(item))
return null;
if(item.dom$class == "tRow") rowReindex(this);
if(item.dom$class == "tBody")
for(var i=0; i<this.tBodies.length; ++i)
if(this.tBodies[i] == item) {
this.tBodies.splice(i, 1);
if(item.rows.length) rowReindex(this);
break;
}
if(item == this.caption) delete this.caption;
if(item.dom$class == "tHead") {
if(item == this.tHead) delete this.tHead;
if(item.rows.length) rowReindex(this);
}
if(item.dom$class == "tFoot") {
if(item == this.tFoot) delete this.tFoot;
if(item.rows.length) rowReindex(this);
}
return item;
}

tRow.prototype.appendChildNative = document.appendChild;
tRow.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return dom$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.nodeName === "TD") // shouldn't be anything other than TD
this.cells.push(newobj);
return newobj;
}
tRow.prototype.insertBeforeNative = document.insertBefore;
tRow.prototype.insertBefore = function(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return dom$.insertFragment(this, newobj, item);
var r = this.insertBeforeNative(newobj, item);
if(!r) return null;
if(newobj.nodeName === "TD")
for(var i=0; i<this.cells.length; ++i)
if(this.cells[i] == item) {
this.cells.splice(i, 0, newobj);
break;
}
return newobj;
}
tRow.prototype.removeChildNative = document.removeChild;
tRow.prototype.removeChild = function(item) {
if(!item) return null;
if(!this.removeChildNative(item))
return null;
if(item.nodeName === "TD")
for(var i=0; i<this.cells.length; ++i)
if(this.cells[i] == item) {
this.cells.splice(i, 1);
break;
}
return item;
}

/*********************************************************************
acid test 48 sets frame.onclick to a string, then expects that function to run
when the frame loads. There are two designs, both are complicated and subtle,
and I'm not sure which one I like better. I implemented the first.
1. Use a setter so that onload = function just carries the function through,
but onload = string compiles the string into a function then sets onload
to the function, as though you had done that in the first place.
2. Allow functions or strings, but dispatch event, and the C event driver,
check to see if it is a function or a string. If a string then compile it.
There is probably a right answer here.
Maybe there is some javascript somewhere that says
a.onclick = "some_function(7,8,9)"; a.onclick();
That would clinch it; 1 is the right answer.
I don't know, but for now I implemented (1),
and hope I don't have to recant some day and switch to (2).
The compiled function has to run bound to this as the current node,
and the current window as global, and trust me, it wasn't easy to set that up!
You can see what I did in handle$cc().
Then there's another complication. For onclick, the code just runs,
but for onsubmit the code is suppose to return true or false.
Mozilla had no trouble compiling and running  return true  at top level.
Duktape won't do that. Return has to be in a function.
So I wrap the code in (function (){ code })
Then it doesn't matter if the code is just expression, or return expression.
Again, look at handle$cc().
*********************************************************************/

; (function() {
var cnlist = ["Body", "Anchor", "HTMLAnchorElement", "Div", "P", "Area", "Image",
"Element","HTMLElement", "Lister", "Listitem", "tBody", "Table", "tRow", "Cell",
"Form", "Span", "Header", "Footer"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
var evs = ["onclick"];
for(var j=0; j<evs.length; ++j) {
var evname = evs[j];
eval(cn + '.prototype["' + evname + '$$watch"] = true');
eval('Object.defineProperty(' + cn + '.prototype, "' + evname + '", { \
get: function() { return this.' + evname + '$2; }, \
set: function(f) { if(my$win().eventDebug) alert3((this.'+evname+'?(this.'+evname+'$$array?"clobber ":"overwrite "):"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".' + evname + '"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.' + evname + '$2 = f; \
/* I assume this clobbers the addEventListener system */ \
delete this.' + evname + '$$array; delete this.' + evname + '$$orig; }}});');
}}})();

; (function() {
var cnlist = ["Body", "Script", "Link", "Form", "Image", "Frame",
"Element"]; // only for <input type=image>
// documentation also says <style> but I don't understand when that is loaded
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
var evs = ["onload", "onunload"];
for(var j=0; j<evs.length; ++j) {
var evname = evs[j];
eval(cn + '.prototype["' + evname + '$$watch"] = true');
eval('Object.defineProperty(' + cn + '.prototype, "' + evname + '", { \
get: function() { return this.' + evname + '$2; }, \
set: function(f) { if(my$win().eventDebug) alert3((this.'+evname+'?(this.'+evname+'$$array?"clobber ":"overwrite "):"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".' + evname + '"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.' + evname + '$2 = f; \
/* I assume this clobbers the addEventListener system */ \
delete this.' + evname + '$$array; delete this.' + evname + '$$orig; }}});');
}}})();
// separate setters below for window and document

; (function() {
var cnlist = ["Form"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
var evs = ["onsubmit", "onreset"];
for(var j=0; j<evs.length; ++j) {
var evname = evs[j];
eval(cn + '.prototype["' + evname + '$$watch"] = true');
eval('Object.defineProperty(' + cn + '.prototype, "' + evname + '", { \
get: function() { return this.' + evname + '$2; }, \
set: function(f) { if(my$win().eventDebug) alert3((this.'+evname+'?(this.'+evname+'$$array?"clobber ":"overwrite "):"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".' + evname + '"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.' + evname + '$2 = f; \
/* I assume this clobbers the addEventListener system */ \
delete this.' + evname + '$$array; delete this.' + evname + '$$orig; }}});');
}}})();

; (function() {
var cnlist = ["Element", "Select", "Body", "Div"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
var evs = ["onchange", "oninput"];
for(var j=0; j<evs.length; ++j) {
var evname = evs[j];
eval(cn + '.prototype["' + evname + '$$watch"] = true');
eval('Object.defineProperty(' + cn + '.prototype, "' + evname + '", { \
get: function() { return this.' + evname + '$2; }, \
set: function(f) { if(my$win().eventDebug) alert3((this.'+evname+'?(this.'+evname+'$$array?"clobber ":"overwrite "):"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".' + evname + '"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.' + evname + '$2 = f; \
/* I assume this clobbers the addEventListener system */ \
delete this.' + evname + '$$array; delete this.' + evname + '$$orig; }}});');
}}})();

document.createElementNS = function(nsurl,s) {
var mismatch = false;
var u = document.createElement(s);
if(!u) return null;
if(!nsurl) nsurl = "";
u.namespaceURI = new URL(nsurl);
// prefix and url have to fit together, I guess.
// I don't understand any of this.
if(!s.match(/:/)) {
// no colon, let it pass
u.prefix = "";
u.localName = s.toLowerCase();
u.tagName = u.nodeName = u.nodeName.toLowerCase();
return u;
}
// There's a colon, and a prefix, and it has to be real.
if(u.prefix == "prefix") {
; // ok
} else if(u.prefix == "html") {
if(nsurl != "http://www.w3.org/1999/xhtml") mismatch = true;
} else if(u.prefix == "svg") {
if(nsurl != "http://www.w3.org/2000/svg") mismatch = true;
} else if(u.prefix == "xbl") {
if(nsurl != "http://www.mozilla.org/xbl") mismatch = true;
} else if(u.prefix == "xul") {
if(nsurl != "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul") mismatch = true;
} else if(u.prefix == "xmlns") {
if(nsurl != "http://www.w3.org/2000/xmlns/") mismatch = true;
} else mismatch = true;
if(mismatch) {
alert3("bad createElementNS(" + nsurl + "," + s + ')');
var e = new Error; e.code = 14; throw e;
}
return u;
}

document.createElement = function(s) { 
var c;
if(!s) { // a null or missing argument
alert3("bad createElement( type" + typeof s + ')');
return null;
}
var t = s.toLowerCase();
if(!t.match(/^[a-z:\d_]+$/) || t.match(/^\d/)) {
alert3("bad createElement(" + t + ')');
// acid3 says we should throw an exception here.
// But we get these kinds of strings from www.oranges.com all the time.
var e = new Error; e.code = 5; throw e;
}
switch(t) { 
case "body": c = new Body; break;
case "object": c = new HtmlObj; break;
case "a": c = new Anchor; break;
case "htmlanchorelement": c = new HTMLAnchorElement; break;
case "image": t = "img";
case "img": c = new Image; break;
case "link": c = new Link; break;
case "meta": c = new Meta; break;
case "cssstyledeclaration": case "style": c = new CSSStyleDeclaration; break;
case "script": c = new Script; break;
case "div": c = new Div; break;
case "label": c = new Label; break;
case "p": c = new P; break;
case "header": c = new Header; break;
case "footer": c = new Footer; break;
case "table": c = new Table; break;
case "tbody": c = new tBody; break;
case "tr": c = new tRow; break;
case "td": c = new Cell; break;
case "caption": c = new tCap; break;
case "thead": c = new tHead; break;
case "tfoot": c = new tFoot; break;
case "canvas": c = new Canvas; break;
case "audio": case "video": c = new Audio; break;
case "document": c = new Document; break;
case "htmliframeelement": case "iframe": case "frame": c = new Frame; break;
case "select": c = new Select; break;
case "option":
c = new Option;
c.nodeName = c.tagName = "OPTION";
c.childNodes = [];
// we don't log options because rebuildSelectors() checks
// the dropdown lists after every js run.
return c;
case "form": c = new Form; break;
case "input": case "element": case "textarea":
c = new Element;
if(t == "textarea") c.type = t;
break;
case "button": c = new Element; c.type = "submit"; break;
default:
/* eb$puts("createElement default " + s); */
c = new HTMLElement;
}

/* ok, for some element types this perhaps doesn't make sense,
* but for most visible ones it does and I doubt it matters much */
if(c.dom$class == "CSSStyleDeclaration") {
c.element = c;
} else {
c.style = new CSSStyleDeclaration;
c.style.element = c;
}
c.dataset = {};
c.childNodes = [];
if(c.dom$class == "Select") c.options = c.childNodes;
c.parentNode = null;
c.attributes = new NamedNodeMap;
c.attributes.owner = c;
if(t == "input") { // name and type are automatic attributes acid test 53
c.setAttribute("name", "");
c.setAttribute("type", "");
}
// Split on : if this comes from a name space
var colon = t.split(':');
if(colon.length == 2) {
c.nodeName = c.tagName = t;
c.prefix = colon[0], c.localName = colon[1];
} else {
c.nodeName = c.tagName = t.toUpperCase();
}
c.nodeType = 1;
if(t == "document")
c.nodeType = 9, c.tagName = "document";
c.class = "";
c.ownerDocument = my$doc();
eb$logElement(c, t);
if(c.nodeType == 1) c.id = c.name = "";

if(c.dom$class == "Frame") {
var d = document.createElement("document");
c.content$Document = c.content$Window = d;
Object.defineProperty(c, "contentDocument", { get: eb$getter_cd });
Object.defineProperty(c, "contentWindow", { get: eb$getter_cw });
c.appendChild(d);
}

return c;
} 

document.createDocumentFragment = function() {
var c = document.createElement("fragment");
c.nodeType = 11;
c.nodeName = c.tagName = "#document-fragment";
return c;
}

document.implementation = {
/*********************************************************************
This is my tentative implementation of hasFeature:
hasFeature: function(mod, v) {
// tidy claims html5 so we'll run with that
var supported = { "html": "5", "Core": "?", "XML": "?"};
if(!supported[mod]) return false;
if(v == undefined) return true; // no version specified
return (v <= supported[mod]);
},
But this page says we're moving to a world where this function is always true,
https://developer.mozilla.org/en-US/docs/Web/API/Document/implementation
so I don't know what the point is.
*********************************************************************/
hasFeature: eb$truefunction,
createDocumentType: function(tag, pubid, sysid) {
// I really don't know what this function is suppose to do.
var tagstrip = tag.replace(/:.*/, "");
return document.createElement(tagstrip);
},
// https://developer.mozilla.org/en-US/docs/Web/API/DOMImplementation/createHTMLDocument
createHTMLDocument: function(t) {
if(t == undefined) t = "Empty"; // the title
// put it in a paragraph, just cause we have to put it somewhere.
var p = document.createElement("p");
p.innerHTML = "<iframe></iframe>";
var d = p.firstChild; // this is the created document
// This reference will expand the document via the setter.
d.contentDocument.title = t;
return d.contentDocument;
}
};

// @author Originally implemented by Yehuda Katz
// And since then, from envjs, by Thatcher et al

XMLHttpRequest = function(){
    this.headers = {};
    this.responseHeaders = {};
    this.aborted = false;//non-standard
    this.withCredentials = true;
};
XMLHttpRequest.prototype.dom$class = "XMLHttpRequest";
// this form of XMLHttpRequest is deprecated, but still used in places.
XDomainRequest = XMLHttpRequest;

// defined by the standard: http://www.w3.org/TR/XMLHttpRequest/#xmlhttprequest
// but not provided by Firefox.  Safari and others do define it.
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPEN = 1;
XMLHttpRequest.HEADERS_RECEIVED = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;

XMLHttpRequest.prototype.open = function(method, url, async, user, password){
this.readyState = 1;
this.async = (async === false)?false:true;
this.method = method || "GET";
alert3("xhr " + (this.async ? "async " : "") + "open " + url);
this.url = eb$resolveURL(my$win().eb$base, url);
this.status = 0;
this.statusText = "";
};
XMLHttpRequest.prototype.setRequestHeader = function(header, value){
this.headers[header] = value;
};
XMLHttpRequest.prototype.send = function(data, parsedoc/*non-standard*/){
var headerstring = "";
for (var item in this.headers) {
var v1=item;
var v2=this.headers[item];
headerstring+=v1+': '+v2+'\n';
}
if(headerstring) alert3("xhr headers " + headerstring.replace(/\n$/,''));
var urlcopy = this.url;
if(urlcopy.match(/[!*'";\[\]$\u0000-\u0020\u007f-\uffff]/)) {
alert3("xhr url was not encoded");
urlcopy = encodeURI(urlcopy);
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
this.$entire =  eb$fetchHTTP.call(this, urlcopy,this.method,headerstring,data);
if(this.$entire != "async") this.parseResponse();
};
XMLHttpRequest.prototype.parseResponse = function(){
var responsebody_array = this.$entire.split("\r\n\r\n");
var success = parseInt(responsebody_array[0]);
var code = parseInt(responsebody_array[1]);
var http_headers = responsebody_array[2];
responsebody_array[0] = responsebody_array[1] = responsebody_array[2] = "";
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
if(success) {
this.status = code;
// need a real statusText for the codes
this.statusText = (code == 200 ? "OK" : "http error " + code);
// When the major libraries are used, they overload XHR left and right.
// Some versions use onreadystatechange.  This has been replaced by onload in,
// for instance, newer versions of jquery.  It can cause problems to call the
// one that is not being used at that moment, so my remedy here is to have
// empty functions in the prototype so I can call both of them.
this.onreadystatechange();
this.onload();
} else {
this.status = 0;
this.statusText = "network error";
}
};
XMLHttpRequest.prototype.abort = function(){ this.aborted = true; };
XMLHttpRequest.prototype.onreadystatechange = XMLHttpRequest.prototype.onload = XMLHttpRequest.prototype.onerror = eb$voidfunction;
XMLHttpRequest.prototype.getResponseHeader = function(header){
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

if (returnedHeaders.length){
return returnedHeaders.join(", ");
}
}
return null;
};
XMLHttpRequest.prototype.getAllResponseHeaders = function(){
var header, returnedHeaders = [];
if (this.readyState < 3){
throw new Error("INVALID_STATE_ERR");
} else {
for (header in this.responseHeaders) {
returnedHeaders.push( header + ": " + this.responseHeaders[header] );
}
}
return returnedHeaders.join("\r\n");
};
XMLHttpRequest.prototype.async = false;
XMLHttpRequest.prototype.readyState = 0;
XMLHttpRequest.prototype.responseText = "";
XMLHttpRequest.prototype.response = "";
XMLHttpRequest.prototype.status = 0;
XMLHttpRequest.prototype.statusText = "";

// Deminimize javascript for debugging purposes.
// Then the line numbers in the error messages actually mean something.
// This is only called when debugging is on. Users won't invoke this machinery.
// Argument is the script object.
// escodegen.generate and esprima.parse are found in third.js.
dom$.eb$demin = function(s) {
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

// Ok, run it through the deminimizer.
if(window.escodegen) {
s.original = s.text;
s.text = escodegen.generate(esprima.parse(s.text));
s.expanded = true;
} else {
alert("deminimization not available");
}
}

// Trace with possible breakpoints.
dom$.eb$watch = function(s) {
if( s.dom$class != "Script") return;
if(! s.text) return;
if(s.text.indexOf("trace"+"@(") >= 0) // already traced
return;
var w = my$win();
if(w.$jt$c == 'z') w.$jt$c = 'a';
else w.$jt$c = String.fromCharCode(w.$jt$c.charCodeAt(0) + 1);
w.$jt$sn = 0;
// Watch out, tools/uncomment will muck with this regexp if we're not careful!
// I escape some spaces with \ so they don't get crunched away.
// First name the anonymous functions; then put in the trace points.
s.text = s.text.replace(/(\bfunction *)(\([\w ,]*\)\ *{\n)/g, dom$.jtfn1);
s.text = s.text.replace(/(\bdo \{|\bwhile \([^{}\n]*\)\ *{|\bfor \([^{}\n]*\)\ *{|\bif \([^{}\n]*\)\ *{|\bcatch \(\w*\)\ *{|\belse \{|\btry \{|\bfunction *\w*\([\w ,]*\)\ *{|[^\n)]\n *)(var |\n)/g, dom$.jtfn0);
return;
}

// trace functions; these only work on deminimized js.
dom$.jtfn0 = function (all, a, b) {
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

dom$.jtfn1 = function (all, a, b) {
var w = my$win();
var c = w.$jt$c;
var sn = w.$jt$sn;
w.$jt$sn = ++sn;
return a + " " + c + "__" + sn + b;
}

// pages seem to want document.style to exist
document.style = new CSSStyleDeclaration;
document.style.bgcolor = "white";

document.ELEMENT_NODE = 1, document.TEXT_NODE = 3, document.COMMENT_NODE = 8, document.DOCUMENT_NODE = 9, document.DOCUMENT_TYPE_NODE = 10, document.DOCUMENT_FRAGMENT_NODE = 11;

// originally ms extension pre-DOM, we don't fully support it
//but offer the legacy document.all.tags method.
document.all = {};
document.all.tags = function(s) { 
return dom$.eb$gebtn(document.body, s.toLowerCase());
}

eb$demin = dom$.eb$demin;
eb$watch = dom$.eb$watch;
$uv = [];
$uv$sn = 0;
$jt$c = 'z';
$jt$sn = 0;

document.querySelectorAll = querySelectorAll;
document.querySelector = querySelector;
document.childNodes = [];
// We'll make another childNodes array belowe every node in the tree.
// document should always and only have two children: DOCTYPE and HTML
Object.defineProperty(document, "firstChild", {
get: function() { return document.childNodes[0]; }});
Object.defineProperty(document, "firstElementChild", {
get: function() { return document.childNodes[1]; }});
Object.defineProperty(document, "lastChild", {
get: function() { return document.childNodes[document.childNodes.length-1]; }});
Object.defineProperty(document, "lastElementChild", {
get: function() { return document.childNodes[document.childNodes.length-1]; }});
Object.defineProperty(document, "nextSibling", {
get: function() { return dom$.eb$getSibling(this,"next"); }});
Object.defineProperty(document, "nextElementSibling", {
get: function() { return dom$.eb$getElementSibling(this,"next"); }});
Object.defineProperty(document, "previousSibling", {
get: function() { return dom$.eb$getSibling(this,"previous"); }});
Object.defineProperty(document, "previousElementSibling", {
get: function() { return dom$.eb$getElementSibling(this,"previous"); }});

/*********************************************************************
Compile a string for a handler such as onclick or onload.
Warning: this is not protected.
set_property_string(anchorObject, "onclick", "snork 7 7")
will run through a setter, which says this is a string to be compiled into
a function, whence a syntax error will cause duktape to abort.
Perhaps every call, or some calls, to set_property_string should be protected,
as I had to do with typeof_property_nat in jseng_duk.c.
Maybe I should bite the bullet and protect the calls to set_property_string.
I already had to work around an abort when setting readyState = "complete",
see this in html.c. It's ugly.
On the other hand, I may want to do something specific when a handler doesn't compile.
Put in a stub handler that returns true or something.
So maybe it's worth having a specific try catch here.
*********************************************************************/

function handle$cc(f, t) {
var cf; // the compiled function
try {
cf = eval("(function(){" + f + " }.bind(t))");
} catch(e) {
// don't just use eb$nullfunction, cause I'm going to put the source
// onto cf.body, which might help with debugging.
cf = eval("(function(){return true;})");
alert3("handler syntax error <" + f + ">");
}
cf.body = f;
cf.toString = function() { return this.body; }
return cf;
}

; (function() {
var cnlist = ["document", "window"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
var evs = ["onload", "onunload", "onclick", "onchange", "oninput"];
for(var j=0; j<evs.length; ++j) {
var evname = evs[j];
eval(cn + '["' + evname + '$$watch"] = true');
eval('Object.defineProperty(' + cn + ', "' + evname + '", { \
get: function() { return this.' + evname + '$2; }, \
set: function(f) { if(my$win().eventDebug) alert3((this.'+evname+'?(this.'+evname+'$$orig?"clobber ":"overwrite "):"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".' + evname + '"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.' + evname + '$2 = f; \
/* I assume this clobbers the addEventListener system */ \
delete this.' + evname + '$$array; delete this.' + evname + '$$orig; }}});');
}}})();

// Local storage, this is per window.
// Then there's sessionStorage, and honestly I don't understand the difference.
// This is NamedNodeMap, to take advantage of preexisting methods.
localStorage = {}
localStorage.attributes = new NamedNodeMap;
localStorage.attributes.owner = localStorage;
// tell me we don't have to do NS versions of all these.
localStorage.getAttribute = document.getAttribute;
localStorage.getItem = localStorage.getAttribute;
localStorage.setAttribute = document.setAttribute;
localStorage.setItem = localStorage.setAttribute;
localStorage.removeAttribute = document.removeAttribute;
localStorage.removeItem = localStorage.removeAttribute;
localStorage.clear = function() {
var l;
while(l = localStorage.attributes.length)
localStorage.removeItem(localStorage.attributes[l-1].name);
}

sessionStorage = {}
sessionStorage.attributes = new NamedNodeMap;
sessionStorage.attributes.owner = sessionStorage;
sessionStorage.getAttribute = document.getAttribute;
sessionStorage.getItem = sessionStorage.getAttribute;
sessionStorage.setAttribute = document.setAttribute;
sessionStorage.setItem = sessionStorage.setAttribute;
sessionStorage.removeAttribute = document.removeAttribute;
sessionStorage.removeItem = sessionStorage.removeAttribute;
sessionStorage.clear = function() {
var l;
while(l = sessionStorage.attributes.length)
sessionStorage.removeItem(sessionStorage.attributes[l-1].name);
}

/*********************************************************************
I don't need to do any of these Array methods for mozjs or v8,
because these methods are inbuilt.
The only one they don't have is item, and that isn't documented anywhere,
so not sure where I picked that one up from.
I'm just going to leave them all out if not duktape.
*********************************************************************/

if(window.Duktape) {
Array.prototype.item = function(x) { return this[x] };
Object.defineProperty(Array.prototype, "item", { enumerable: false});

Array.prototype.includes = function(x, start) {
if(typeof start != "number") start = 0;
var l = this.length;
if(start < 0) start += l;
if(start < 0) start = 0;
for(var i=start; i<l; ++i)
if(this[i] === x) return true;
return false;
}
Object.defineProperty(Array.prototype, "includes", { enumerable: false});

Array.from = function(o) {
var k = arguments.length;
var a = [];
if(!k) return a;
var l = o.length;
for(var i=0; i<l; ++i) {
var fn = null, thisobj = null;
if(k >= 2) fn = arguments[1];
if(k >= 3) thisobj = arguments[2];
var w = o[i];
if(fn) {
if(thisobj) w = fn.call(thisobj, w);
else w = fn(w);
}
a.push(w);
}
return a;
}

Array.prototype.map = function(fn, t) {
var a = [];
if(!fn) return a; // should never happen
for(var i = 0; i<this.length; ++i) {
var w = this[i];
if(t) w = fn.call(t, w, i, this);
else w = fn(w, i, this);
a.push(w);
}
return a;
}
Object.defineProperty(Array.prototype, "map", { enumerable: false});
}

// On the first call this setter just creates the url, the location of the
// current web page, But on the next call it has the side effect of replacing
// the web page with the new url.
Object.defineProperty(window, "location", {
get: function() { return window.location$2; },
set: function(h) {
if(!window.location$2) {
window.location$2 = new URL(h);
} else {
window.location$2.href = h;
}
}});
Object.defineProperty(document, "location", {
get: function() { return document.location$2; },
set: function(h) {
if(!document.location$2) {
document.location$2 = new URL(h);
} else {
document.location$2.href = h;
}
}});

// Window constructor, passes the url back to edbrowse
// so it can open a new web page.
Window = function() {
var newloc = "";
var winname = "";
if(arguments.length > 0) newloc = arguments[0];
if(arguments.length > 1) winname = arguments[1];
// I only do something if opening a new web page.
// If it's just a blank window, I don't know what to do with that.
if(newloc.length)
eb$newLocation('p' + newloc+ '\n' + winname);
this.opener = window;
}

/* window.open is the same as new window, just pass the args through */
function open() {
return Window.apply(this, arguments);
}

// nasa.gov and perhaps other sites check for self.constructor == Window.
// That is, Window should be the constructor of window.
// The constructor is Object by default.
window.constructor = Window;

// Some websites expect an onhashchange handler from the get-go.
onhashchange = eb$truefunction;

dom$.cssGather = function(pageload, newwin) {
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
eb$cssDocLoad(w, css_all, pageload);
}

// Apply rules to a given style object, which is this.
Object.defineProperty(CSSStyleDeclaration.prototype, "cssText", { get: dom$.cssTextGet, set: eb$cssText });

eb$qs$start = function() {
// This is a stub for now.
my$doc().prependChild(new DocType);
dom$.cssGather(true);
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

eb$visible = function(t) {
// see the DIS_ values in eb.h
var c, rc = 0;
var so; // style object
if(!t || !(so = t.style)) return 0;
// If class has changed, recompute style.
// If id has changed, recompute style, but I don't think that ever happens.
if(t.class != t.last$class || t.id != t.last$id) {
var w = my$win();
if(t.last$class) alert3("restyle " + t.nodeName + "." + t.last$class + "." + t.class+"#"+t.last$id+"#"+t.id);
else alert4("restyle " + t.nodeName + "." + t.last$class + "." + t.class+"#"+t.last$id+"#"+t.id);
if(w.rr$start) {
dom$.cssGather(false, w);
delete w.rr$start;
}
dom$.computeStyleInline(t);
}
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

// This is a stub.
DOMParser = function() {
return {parseFromString: function(t,y) {
var d = my$doc();
if(y == "text/html" || y == "text/xml") {
var v = d.createElement("div");
v.innerHTML = t;
return v;
}
if(y == "text/plain") {
return document.createTextNode(t);
}
alert3("trying to use the DOM parser\n" + y + " <<< ");
alert4(t);
alert3(">>>");
return document.createTextNode("DOMParser not yet implemented");
}}};

XMLSerializer = function(){}
XMLSerializer.prototype.serializeToString = function(root) {
alert3("trying to use XMLSerializer");
return "<div>XMLSerializer not yet implemented</div>"; }

css$ver = 0;
document.xmlVersion = 0;

// if debugThrow is set, see all errors, even caught errors.
// This is only meaningful in duktape.
if(window.Duktape) {
Duktape.errCreate = function (e) {
if(throwDebug) {
var n = e.lineNumber;
var msg = "";
if(typeof n === "number")
msg += "line " + n + ": ";
msg += e.toString();
alert3(msg);
}
    return e;
}
}
throwDebug = false;

// here comes the Iterator and Walker
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
document.createNodeIterator = function(root, mask, callback, unused) {
var o = {}; // the created iterator object
if(typeof callback != "function") callback = null;
o.callback = callback;
if(typeof mask != "number")
mask = 0xffffffff;
// let's reuse some software
if(typeof root == "object") {
o.list = dom$.eb$gebtn(root, "*");
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
if(nt == 9 && !(mask&NodeFilter.SHOW_DOCUMENT))
alive = false;
if(nt == 3 && !(mask&NodeFilter.SHOW_TEXT))
alive = false;
if(nt == 1 && !(mask&NodeFilter.SHOW_ELEMENT))
alive = false;
if(nt == 11 && !(mask&NodeFilter.SHOW_DOCUMENT_FRAGMENT))
alive = false;
if(nt == 8 && !(mask&NodeFilter.SHOW_COMMENT))
alive = false;
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

document.createTreeWalker = function(root, mask, callback, unused) {
var o = {}; // the created iterator object
if(typeof callback != "function") callback = null;
o.callback = callback;
if(typeof mask != "number")
mask = 0xffffffff;
if(typeof root == "object") {
o.list = dom$.eb$gebtn(root, "*");
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
if(nt == 9 && !(mask&NodeFilter.SHOW_DOCUMENT))
alive = false;
if(nt == 3 && !(mask&NodeFilter.SHOW_TEXT))
alive = false;
if(nt == 1 && !(mask&NodeFilter.SHOW_ELEMENT))
alive = false;
if(nt == 11 && !(mask&NodeFilter.SHOW_DOCUMENT_FRAGMENT))
alive = false;
if(nt == 8 && !(mask&NodeFilter.SHOW_COMMENT))
alive = false;
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

MutationObserver = function(f) {
var w = my$win();
w.mutList.push(this);
this.callback = (typeof f == "function" ? f : eb$voidfunction);
this.active = false;
this.target = null;
}
MutationObserver.prototype.dom$class = "MutationObserver";
MutationObserver.prototype.disconnect = function() { this.active = false; }
MutationObserver.prototype.observe = function(target, cfg) {
if(typeof target != "object" || typeof cfg != "object" || !target.nodeType || target.nodeType != 1) {
this.active = false;
return;
}
this.target = target;
this.attr = this.kids = this.subtree = false;
if(cfg.attributes) this.attr = true;
if(cfg.childList) this.kids = true;
if(cfg.subtree) this.subtree = true;
this.active = true;
}
MutationObserver.prototype.takeRecords = function() { return []}

MutationRecord = function(){};
MutationRecord.prototype.dom$class = "MutationRecord";

/*********************************************************************
I'm going to call Fixup from appendChild, removeChild, setAttribute,
anything that changes something we might be observing.
If we are indeed observing, I call the callback function right away.
That's now how we're suppose to do it.
I am suppose to queue up the change records, then call the callback
function later, after this script is done, asynchronously, maybe on a timer.
I could combine a dozen "kids have changed" records into one, to say,
"hey, the kids have changed."
And an attribute change record etc.
So they are expecting an array of change records.
I send an array of length 1, 1 record, right now.
It's just easier.
*********************************************************************/

dom$.mrList = function(x) {
if(Array.isArray(x)) {
// return a copy of the array
return [].concat(x);
}
if(typeof x == "number") return [];
return x ? [x] : [];
}

dom$.mrKids = function(r, b, y, z) {
r.target = b;
r.type = "childList";
r.oldValue = null;
r.addedNodes = dom$.mrList(y);
r.removedNodes = dom$.mrList(z);
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

mutFixup = function(b, isattr, y, z) {
var w = my$win();
var list = w.mutList;
// most of the time there are no observers, so loop over that first
// and this function does nothing and doesn't slow things down too much.
for(var j = 0; j < list.length; ++j) {
var o = list[j]; // the observer
if(!o.active) continue;
var r; // mutation record
if(isattr) { // the easy case
if(o.attr && o.target == b) {
r = new MutationRecord;
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
r = new MutationRecord;
dom$.mrKids(r, b, y, z);
o.callback([r], o);
continue;
}
if(!o.subtree) continue;
// climb up the tree
for(var t = b; t && t.nodeType == 1; t = t.parentNode) {
if(o.subtree && o.target == t) {
r = new MutationRecord;
dom$.mrKids(r, b, y, z);
o.callback([r], o);
break;
}
}
}
}

mutList = [];

crypto = {};
crypto.getRandomValues = function(a) {
if(!Array.isArray(a)) return;
var l = a.length;
for(var i=0; i<l; ++i) a[i] = Math.floor(Math.random()*0x100000000);
}

rastep = 0;
requestAnimationFrame = function() {
// This absolutely doesn't do anything. What is edbrowse suppose to do with animation?
return ++dom$.rastep;
}

// only duktape needs the es6 stuff
if(window.Duktape) {
Set = function() { this.items=[]; }
Object.defineProperty(Set.prototype, "size", {get:function(){return this.items.length}});
Set.prototype.has = function(x) {return this.items.indexOf(x) >= 0;}
Set.prototype.add = function(x) {if(!this.has(x)) this.items.push(x); return x; }
Set.prototype.clear = function(){this.items.length = 0;}
Set.prototype.delete = function(x) {var i = this.items.indexOf(x); if(i < 0) return false; this.items.splice(i,1); return true; }
Set.prototype.forEach = function(fn,t) {
for(var i=0; i<this.items.length; ++i)
if(t) fn.call(t,this.items[i]); else fn(this.items[i]);
}

Map = function() { this.keys = [], this.items=[]; }
Object.defineProperty(Map.prototype, "size", {get:function(){return this.items.length}});
Map.prototype.clear = function(){this.items.length = this.keys.length = 0;}
Map.prototype.delete = function(k) {
for(var i=0; i<this.keys.length; ++i)
if(this.keys[i] === k) {
this.keys.splice(i,1), this.items.splice(i,1);
return true;
}
return false;
}
Map.prototype.get = function(k) {
for(var i=0; i<this.keys.length; ++i)
if(this.keys[i] === k)
return this.items[i];
return undefined;
}
Map.prototype.has = function(k) {
for(var i=0; i<this.keys.length; ++i)
if(this.keys[i] === k)
return true;
return false;
}
Map.prototype.set = function(k,v) {
for(var i=0; i<this.keys.length; ++i)
if(this.keys[i] === k) {
this.items[i] = v;
return this;
}
this.keys.push(k), this.items.push(v);
return this;
}
Map.prototype.forEach = function(fn,t) {
for(var i=0; i<this.items.length; ++i)
if(t) fn.call(t,this.keys[i], this.items[i]); else fn(this.keys[i], this.items[i]);
}

Reflect = function() {};
Reflect.prototype.get = function(target,propertyKey) {
return target[propertyKey];
}

String.prototype.padStart = function(l2, v) {
var l1 = this.length;
var s = new String(this);
if(l2 <= l1) return s;
var l0 = l1;
if(!v) v = " ";
var l3 = v.length;
while(l1+l3 <= l2) s = v+s, l1 += l3;
if(l1 == l2) return s;
v = v.substr(0,l2-l1);
return s.substr(0,l1-l0) + v + s.substr(l1-l0);
}
String.prototype.padEnd = function(l2, v) {
var l1 = this.length;
var s = new String(this);
if(l2 <= l1) return s;
if(!v) v = " ";
var l3 = v.length;
while(l1+l3 <= l2) s = s+v, l1 += l3;
if(l1 == l2) return s;
v = v.substr(0,l2-l1);
return s+v;
}
}

cancelAnimationFrame = eb$voidfunction;

