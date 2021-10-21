// stringname=startWindowJS
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
document = {};
// Stubs for native methods that are normally provided by edbrowse.
// Example: alert, which we can replace with print,
// or console.log, or anything present in the command line js interpreter.
if(!window.print) print = console.log;
alert = print;
eb$nullfunction = function() { return null; }
eb$voidfunction = function() { }
eb$truefunction = function() { return true; }
eb$falsefunction = function() { return false; }
eb$newLocation = function (s) { print("new location " + s); }
eb$logElement = function(o, tag) { print("pass tag " + tag + " to edbrowse"); }
eb$getcook = function() { return "cookies"; }
eb$setcook = function(value) { print(" new cookie " + value); }
eb$parent = function() { return this; }
eb$top = function() { return this; }
eb$frameElement = function() { return this; }
eb$getter_cd = function() { return null; }
eb$getter_cw = function() { return null; }
eb$formSubmit = function() { print("submit"); }
eb$formReset = function() { print("reset"); }
eb$listen = eb$unlisten = addEventListener = removeEventListener = eb$voidfunction;
my$win = function() { return window; }
my$doc = function() { return document; }
document.eb$apch2 = function(c) { alert("append " + c.nodeName  + " to " + this.nodeName); this.childNodes.push(c); }
querySelectorAll = function() { return [] ; }
querySelector = function() { return {} ; }
querySelector0 = function() { return false; }
eb$cssText = function(){}
}

// the third party deminimization stuff is in mw$, the master window.
// Other stuff too, that can be shared.
// The window should just be there from C, but in case it isn't.
if(!window.mw$)
mw$ = {compiled: false, share:false, URL:{}};

if(mw$.share) { // point to native methods in the master window
my$win = mw$.my$win, my$doc = mw$.my$doc;
natok = mw$.natok, db$flags = mw$.db$flags;
eb$voidfunction = mw$.eb$voidfunction, eb$nullfunction = mw$.eb$nullfunction, eb$truefunction = mw$.eb$truefunction, eb$falsefunction = mw$.eb$falsefunction;
close = mw$.win$close;
eb$resolveURL = mw$.eb$resolveURL;
eb$visible = mw$.eb$visible;
atob = mw$.atob, btoa = mw$.btoa;
prompt = mw$.prompt, confirm = mw$.confirm;
eb$newLocation = mw$.eb$newLocation, eb$logElement = mw$.eb$logElement;
}
scroll = scrollTo = scrollBy = scrollByLines = scrollByPages = eb$voidfunction;
document.close = eb$voidfunction;
blur = document.blur = function(){document.activeElement=null}
focus = document.focus = function(){document.activeElement=document.body}

self = window;
Object.defineProperty(window, "parent", {get: eb$parent});
Object.defineProperty(window, "top", {get: eb$top});
Object.defineProperty(window, "frameElement", {get: eb$frameElement});

/* An ok (object keys) function for javascript/dom debugging.
 * This is in concert with the jdb command in edbrowse.
 * I know, it doesn't start with eb$ but I wanted an easy,
 * mnemonic command that I could type in quickly.
 * If a web page creates an ok function it will override this one.
And that does happen, e.g. the react system, so $ok is an alias for this. */
ok = $ok = Object.keys;

window.nodeName = "WINDOW"; // in case you want to start at the top.
document.nodeName = "DOCUMENT"; // in case you want to start at document.
document.tagName = "document";
document.nodeType = 9;
document.ownerDocument = document;

if(mw$.share) {
alert = mw$.alert, alert3 = mw$.alert3, alert4 = mw$.alert4;
dumptree = mw$.dumptree, uptrace = mw$.uptrace;
showscripts = mw$.showscripts, searchscripts = mw$.searchscripts, showframes = mw$.showframes;
snapshot = mw$.snapshot, aloop = mw$.aloop;
eb$base$snapshot = mw$.eb$base$snapshot, set_location_hash = mw$.set_location_hash;
document.getElementsByTagName = mw$.getElementsByTagName, document.getElementsByName = mw$.getElementsByName, document.getElementsByClassName = mw$.getElementsByClassName, document.getElementById = mw$.getElementById;
document.nodeContains = mw$.nodeContains;
document.dispatchEvent = mw$.dispatchEvent;
// make sure to wrap global dispatchEvent, so this becomes this window,
// and not the shared window.
dispatchEvent = function(e) { return mw$.dispatchEvent.call(window, e);}
addEventListener = document.addEventListener = function(ev, handler, iscapture) { this.eb$listen(ev,handler, iscapture, true); }
removeEventListener = document.removeEventListener = function(ev, handler, iscapture) { this.eb$unlisten(ev,handler, iscapture, true); }
if(mw$.attachOn) {
attachEvent = document.attachEvent = function(ev, handler) { this.eb$listen(ev,handler, true, false); }
detachEvent = document.detachEvent = function(ev, handler) { this.eb$unlisten(ev,handler, true, false); }
}
eb$listen = document.eb$listen = mw$.eb$listen;
eb$unlisten = document.eb$unlisten = mw$.eb$unlisten;
NodeFilter = mw$.NodeFilter, document.createNodeIterator = mw$.createNodeIterator, document.createTreeWalker = mw$.createTreeWalker;
rowReindex = mw$.rowReindex, getComputedStyle = mw$.getComputedStyle.bind(window);
mutFixup = mw$.mutFixup;
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

document.bgcolor = "white";
document.readyState = "loading";
document.contentType = "text/html";
function readyStateComplete() { document.readyState = "complete"; document.activeElement = document.body;
if(document.onreadystatechange$$fn) {
var e = new Event;
e.initEvent("readystatechange", true, true);
e.target = e.currentTarget = document;
e.eventPhase = 2;
document.onreadystatechange$$fn(e);
}
}

screen = {
height: 768, width: 1024,
availHeight: 768, availWidth: 1024, availTop: 0, availLeft: 0,
colorDepth: 24};

console = {
log: function(obj) { mw$.logtime(3, "log", obj); },
info: function(obj) { mw$.logtime(3, "info", obj); },
warn: function(obj) { mw$.logtime(3, "warn", obj); },
error: function(obj) { mw$.logtime(3, "error", obj); },
timeStamp: function(label) { if(label === undefined) label = "x"; return label.toString() + (new Date).getTime(); }
};

Object.defineProperty(document, "cookie", {
get: eb$getcook, set: eb$setcook});

Object.defineProperty(document, "documentElement", {get: mw$.getElement});
Object.defineProperty(document, "head", {get: mw$.getHead,set:mw$.setHead});
Object.defineProperty(document, "body", {get: mw$.getBody,set:mw$.setBody});
// document should always have children, but...
document.hasChildNodes = mw$.hasChildNodes;
// This is set to body after browse.
document.activeElement = null;

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
// pending jobs, mostly to debug promise functions.
$pjobs = [];

// symbolic constants for compareDocumentPosition
DOCUMENT_POSITION_DISCONNECTED = mw$.DOCUMENT_POSITION_DISCONNECTED;
DOCUMENT_POSITION_PRECEDING = mw$.DOCUMENT_POSITION_PRECEDING;
DOCUMENT_POSITION_FOLLOWING = mw$.DOCUMENT_POSITION_FOLLOWING;
DOCUMENT_POSITION_CONTAINS = mw$.DOCUMENT_POSITION_CONTAINS;
DOCUMENT_POSITION_CONTAINED_BY = mw$.DOCUMENT_POSITION_CONTAINED_BY;
document.compareDocumentPosition = mw$.compareDocumentPosition;

/*********************************************************************
The URL class is head-spinning in its complexity and its side effects.
Almost all of these can be handled in JS,
except for setting window.location or document.location to a new url,
which replaces the web page you are looking at.
This side effect does not take place in the constructor, which establishes the initial url.
Here's one reason, perhaps not the only reason, we can't share the URL class.
Why it has to stay here in startwindow.js.
This may apply to every other DOM class as well.
There are websites that replace URL.prototype.toString with their own function.
They want to change the way URLs stringify, or whatever. I can't
prevent sites from doing that, things might not work properly without it!
So, if site A does that in the shared window, and site B invokes
a.href.toString, directly or indirectly, B is calling a function from
the unrelated website A.
This could really screw things up, or worse, site A could use it to hack into
site B, hoping site B is your banking site or something important.
*********************************************************************/

z$URL = URL = function() {
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

Object.defineProperty(URL.prototype, "href", {
  get: function() {return this.href$val; },
  set: function(v) {
var inconstruct = true;
if(v.dom$class == "URL") v = v.toString();
if(v === null || v === undefined) v = "";
if(typeof v != "string") return;
if(typeof this.href$val == "string") {
// Ok, we already had a url, and here's another one.
// I think we're suppose to resolve it against what was already there,
// so that /foo against www.xyz.com becomes www.xyz.com/foo
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
this.port$val = mw$.setDefaultPort(this.protocol$val);
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
of all the others.
I include Node because some javascript will interrogate Node to see
which methods all the nodes possess?
Do we support appendchild?   etc.
*********************************************************************/

Node = function(){};
Node.prototype.dom$class = "Node";

// a node list is and isn't an array; I don't really understand it.
// I'll just have it inherit from array, until someone tells me I'm wrong.
NodeList = function(){}
NodeList.prototype = new Array;
NodeList.prototype.dom$class = "NodeList";

HTMLElement = function(){};
HTMLElement.prototype = new Node;
HTMLElement.prototype.dom$class = "HTMLElement";

z$HTML = function(){};
z$HTML.prototype = new HTMLElement;
z$HTML.prototype.dom$class = "HTML";
// Some screen attributes that are suppose to be there.
z$HTML.prototype.doScroll = eb$voidfunction;
z$HTML.prototype.clientHeight = 768;
z$HTML.prototype.clientWidth = 1024;
z$HTML.prototype.offsetHeight = 768;
z$HTML.prototype.offsetWidth = 1024;
z$HTML.prototype.scrollHeight = 768;
z$HTML.prototype.scrollWidth = 1024;
z$HTML.prototype.scrollTop = 0;
z$HTML.prototype.scrollLeft = 0;

// is there a difference between DocType ad DocumentType?
z$DocType = function(){ this.nodeType = 10, this.nodeName = "DOCTYPE";};
z$DocType.prototype = new HTMLElement;
z$DocType.prototype.dom$class = "DocType";
DocumentType = function(){};
DocumentType.prototype = new HTMLElement;
DocumentType.prototype.dom$class = "DocumentType";
CharacterData = function(){};
CharacterData.prototype.dom$class = "CharacterData";
HTMLHeadElement = function(){};
HTMLHeadElement.prototype = new HTMLElement;
HTMLHeadElement.prototype.dom$class = "HTMLHeadElement";
z$Meta = function(){};
z$Meta.prototype = new HTMLElement;
z$Meta.prototype.dom$class = "Meta";
z$Title = function(){};
z$Title.prototype = new HTMLElement;
z$Title.prototype.dom$class = "Title";
Object.defineProperty(z$Title.prototype, "text", {
get: function(){ return this.firstChild && this.firstChild.nodeName == "#text" && this.firstChild.data || "";}
// setter should change the title of the document, not yet implemented
});
HTMLLinkElement = function(){};
HTMLLinkElement.prototype = new HTMLElement;
HTMLLinkElement.prototype.dom$class = "HTMLLinkElement";
// It's a list but why would it ever be more than one?
Object.defineProperty(HTMLLinkElement.prototype, "relList", {
get: function() { var a = this.rel ? [this.rel] : [];
// edbrowse only supports stylesheet
a.supports = function(s) { return s === "stylesheet"; }
return a;
}});

HTMLBodyElement = function(){};
HTMLBodyElement.prototype = new HTMLElement;
HTMLBodyElement.prototype.dom$class = "HTMLBodyElement";
HTMLBodyElement.prototype.doScroll = eb$voidfunction;
HTMLBodyElement.prototype.clientHeight = 768;
HTMLBodyElement.prototype.clientWidth = 1024;
HTMLBodyElement.prototype.offsetHeight = 768;
HTMLBodyElement.prototype.offsetWidth = 1024;
HTMLBodyElement.prototype. scrollHeight = 768;
HTMLBodyElement.prototype.scrollWidth = 1024;
HTMLBodyElement.prototype.scrollTop = 0;
HTMLBodyElement.prototype.scrollLeft = 0;
// document.body.innerHTML =
HTMLBodyElement.prototype.eb$dbih = function(s){this.innerHTML = s}

z$Base = function(){};
z$Base.prototype = new HTMLElement;
z$Base.prototype.dom$class = "Base";

HTMLFormElement = function(){this.elements = []}
HTMLFormElement.prototype = new HTMLElement;
HTMLFormElement.prototype.dom$class = "HTMLFormElement";
HTMLFormElement.prototype.submit = eb$formSubmit;
HTMLFormElement.prototype.reset = eb$formReset;
Object.defineProperty(HTMLFormElement.prototype, "length", { get: function() { return this.elements.length;}});

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

z$Element = Element = function() { this.validity = new Validity, this.validity.owner = this};
z$Element.prototype = new HTMLElement;
z$Element.prototype.dom$class = "Element";
z$Element.prototype.selectionStart = 0;
z$Element.prototype.selectionEnd = -1;
z$Element.prototype.selectionDirection = "none";
// I really don't know what this function does, something visual I think.
z$Element.prototype.setSelectionRange = function(s, e, dir) {
if(typeof s == "number") this.selectionStart = s;
if(typeof e == "number") this.selectionEnd = e;
if(typeof dir == "string") this.selectionDirection = dir;
}
z$Element.prototype.click = mw$.clickfn;

// We only need this in the rare case of setting click and clearing
// the other radio buttons. acid test 43
Object.defineProperty(z$Element.prototype, "checked", {
get: function() { return this.checked$2 ? true : false; },
set: mw$.checkset});

Object.defineProperty(z$Element.prototype, "name", {
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
Object.defineProperty(z$Element.prototype, "innerText", {
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

textarea$html$crossover = function(t) {
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

z$Select = function() { this.selectedIndex = -1; this.value = "";this.validity = new Validity, this.validity.owner = this};
z$Select.prototype = new HTMLElement;
z$Select.prototype.dom$class = "Select";
Object.defineProperty(z$Select.prototype, "value", {
get: function() {
var a = this.options;
var n = this.selectedIndex;
return (this.multiple || n < 0 || n >= a.length) ? "" : a[n].value;
}});
z$Datalist = function() {}
z$Datalist.prototype = new HTMLElement;
z$Datalist.prototype.dom$class = "Datalist";
z$Datalist.prototype.multiple = true;
Image = HTMLImageElement = function(){};
HTMLImageElement.prototype = new HTMLElement;
HTMLImageElement.prototype.dom$class = "HTMLImageElement";

HTMLFrameElement = function(){};
HTMLFrameElement.prototype = new HTMLElement;
HTMLFrameElement.prototype.dom$class = "Frame";
Object.defineProperty(HTMLFrameElement.prototype, "contentDocument", { get: eb$getter_cd});
Object.defineProperty(HTMLFrameElement.prototype, "contentWindow", { get: eb$getter_cw});
// These may be different but for now I'm calling them the same.
HTMLIFrameElement = function(){};
HTMLIFrameElement.prototype = new HTMLFrameElement;

HTMLAnchorElement = function(){};
HTMLAnchorElement.prototype = new HTMLElement;
HTMLAnchorElement.prototype.dom$class = "HTMLAnchorElement";
z$Lister = function(){};
z$Lister.prototype = new HTMLElement;
z$Lister.prototype.dom$class = "Lister";
z$Listitem = function(){};
z$Listitem.prototype = new HTMLElement;
z$Listitem.prototype.dom$class = "Listitem";
z$tBody = function(){ this.rows = []};
z$tBody.prototype = new HTMLElement;
z$tBody.prototype.dom$class = "tBody";
z$tHead = function(){ this.rows = []};
z$tHead.prototype = new HTMLElement;
z$tHead.prototype.dom$class = "tHead";
z$tFoot = function(){ this.rows = []};
z$tFoot.prototype = new HTMLElement;
z$tFoot.prototype.dom$class = "tFoot";
z$tCap = function(){};
z$tCap.prototype = new HTMLElement;
z$tCap.prototype.dom$class = "tCap";
HTMLTableElement = function(){ this.rows = []; this.tBodies = []};
HTMLTableElement.prototype = new HTMLElement;
HTMLTableElement.prototype.dom$class = "HTMLTableElement";
HTMLTableRowElement = function(){ this.cells = []};
HTMLTableRowElement.prototype = new HTMLElement;
HTMLTableRowElement.prototype.dom$class = "HTMLTableRowElement";
HTMLTableCellElement = function(){};
HTMLTableCellElement.prototype = new HTMLElement;
HTMLTableCellElement.prototype.dom$class = "HTMLTableCellElement";
z$Div = function(){};
z$Div.prototype = new HTMLElement;
z$Div.prototype.dom$class = "Div";
z$Div.prototype.doScroll = eb$voidfunction;
z$Div.prototype.click = function() {
// as though the user had clicked on this
var e = new Event;
e.initEvent("click", true, true);
this.dispatchEvent(e);
}
z$Label = function(){};
z$Label.prototype = new HTMLElement;
z$Label.prototype.dom$class = "Label";
Object.defineProperty(z$Label.prototype, "htmlFor", { get: function() { return this.getAttribute("for"); }, set: function(h) { this.setAttribute("for", h); }});
HtmlObj = function(){};
HtmlObj.prototype = new HTMLElement;
HtmlObj.prototype.dom$class = "HtmlObj";
HTMLAreaElement = function(){};
HTMLAreaElement.prototype = new HTMLElement;
HTMLAreaElement.prototype.dom$class = "HTMLAreaElement";
z$Span = function(){};
z$Span.prototype = new HTMLElement;
z$Span.prototype.dom$class = "Span";
z$Span.prototype.doScroll = eb$voidfunction;
z$P = function(){};
z$P.prototype = new HTMLElement;
z$P.prototype.dom$class = "P";
z$Header = function(){};
z$Header.prototype = new HTMLElement;
z$Header.prototype.dom$class = "Header";
z$Footer = function(){};
z$Footer.prototype = new HTMLElement;
z$Footer.prototype.dom$class = "Footer";
HTMLScriptElement = function(){};
HTMLScriptElement.prototype = new HTMLElement;
HTMLScriptElement.prototype.dom$class = "HTMLScriptElement";
HTMLScriptElement.prototype.type = "";
HTMLScriptElement.prototype.text = "";
Timer = function(){this.nodeName = "TIMER"};
Timer.prototype.dom$class = "Timer";
z$Audio = function(){};
z$Audio.prototype = new HTMLElement;
z$Audio.prototype.dom$class = "Audio";
z$Audio.prototype.play = eb$voidfunction;

// I don't implement any of the performance features;
// but the object has to exist for some sites to work.
Performance = function(){}
Performance.prototype = {
getEntriesByType:function(x){ return []; }
}
Object.defineProperty(window, "performance", {get: function(){return new Performance}});

// I don't implement any of the custom elements registry methods;
// but the object has to exist for some sites to work.
Object.defineProperty(window, "customElements", {get:function(){ return {}}});

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
var cnlist = ["HTMLAnchorElement", "HTMLAreaElement", "HTMLFrameElement"];
var ulist = ["href", "href", "src"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i]; // class name
var u = ulist[i]; // url name
eval('Object.defineProperty(' + cn + '.prototype, "' + u + '", { \
get: function() { return this.href$2 ? this.href$2 : ""}, \
set: function(h) { if(h instanceof URL || h.dom$class == "URL") h = h.toString(); \
if(h === null || h === undefined) h = ""; \
var w = my$win(); \
if(typeof h != "string") { alert3("hrefset " + typeof h); \
w.hrefset$p.push("' + cn + '"); \
w.hrefset$a.push(h); \
return; } \
/* h is a string version of the url. Dont know what to do if h is empty. */ \
if(!h) return; \
var last_href = (this.href$2 ? this.href$2.toString() : null); \
/* resolve h against the base */ \
h = eb$resolveURL(w.eb$base,h); \
this.href$2 = new z$URL(h); \
/* special code for setting frame.src, redirect to a new page. */ \
if(this.dom$class == "Frame" && this.eb$expf && last_href != h) { \
/* There is a nasty corner case here, dont know if it ever happens. What if we are replacing the running frame? window.parent.src = new_url; See if we can get around it this way. */ \
if(w == this.contentWindow) { w.location = h; return; } \
delete this.eb$expf; \
eb$unframe(this); /* fix links on the edbrowse side */ \
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
Ok - a.href is a url object, but script.src is a string.
You won't find that anywhere in the documentation, w3 schools etc, nope, I just
respond to the javascript in the wild, and that's what it seems to expect.
I only know for sure a.href is URL, and script.src is string,
everything else is a guess.
*********************************************************************/

; (function() {
var cnlist = ["HTMLFormElement", "HTMLImageElement", "HTMLScriptElement", "z$Base", "HTMLLinkElement", "z$Audio"];
var ulist = ["action", "src", "src", "href", "href", "src"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i]; // class name
var u = ulist[i]; // url name
eval('Object.defineProperty(' + cn + '.prototype, "' + u + '", { \
get: function() { return this.href$2 ? this.href$2 : ""}, \
set: function(h) { if(h instanceof URL || h.dom$class == "URL") h = h.toString(); \
if(h === null || h === undefined) h = ""; \
var w = my$win(); \
if(typeof h != "string") { alert3("hrefset " + typeof h); \
w.hrefset$p.push("' + cn + '"); \
w.hrefset$a.push(h); \
return; } \
if(!h) return; \
var last_href = (this.href$2 ? this.href$2 : null); \
/* resolve h against the base */ \
h = eb$resolveURL(w.eb$base,h); \
this.href$2 = h; \
 }});');
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
z$Canvas = function() {};
z$Canvas.prototype = new HTMLElement;
z$Canvas.prototype.dom$class = "Canvas";
z$Canvas.prototype.getContext = function(x) { return { addHitRegion: eb$nullfunction,
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
z$Canvas.prototype.toDataURL = function() {
if(this.height === 0 || this.width === 0) return "data:,";
// this is just a stub
return "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAADElEQVQImWNgoBMAAABpAAFEI8ARAAAAAElFTkSuQmCC";
}

onmessage$$queue = [];
function  postMessage(message,target_origin) {
if (this.location.protocol + "//" + this.location.hostname == target_origin || target_origin == "*") {
var me = new Event;
me.name = "message";
me.type = "message";
me.origin = this.location.protocol + "//" + this.location.hostname;
me.data = message;
this.onmessage$$queue.push(me);
alert3("posting message of length " + message.length + " to context " + this.eb$ctx);
}
}
function onmessage$$running() {
if(window.onmessage$$array && onmessage$$array.length) { // handlers are ready
while(onmessage$$queue.length) {
// better run messages fifo
var me = onmessage$$queue[0];
onmessage$$queue.splice(0, 1);
alert3("context " + eb$ctx + " processes message of length " + me.data.length + " ↑" +
(me.data.length >= 200 ? "long" : me.data) + "↑");
// yeah you really need window.onmessage$$fn, for subtle reasons
window.onmessage$$fn(me);
alert3("process message complete");
}
}
}
Object.defineProperty(window, "onmessage$$queue", {writable:false,configurable:false});
Object.defineProperty(window, "onmessage$$running", {writable:false,configurable:false});
Object.defineProperty(window, "postMessage", {writable:false,configurable:false});

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
Document.prototype = new HTMLElement;
Document.prototype.dom$class = "Document";

DocumentFragment = function(){};
DocumentFragment.prototype = new HTMLElement;
DocumentFragment.prototype.dom$class = "DocumentFragment";

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

CSSStyleDeclaration = function(){
        this.element = null;
        this.style = this;
this.ownerDocument = my$doc();
};
CSSStyleDeclaration.prototype = new HTMLElement;
CSSStyleDeclaration.prototype.dom$class = "CSSStyleDeclaration";
// sheet on demand
Object.defineProperty(CSSStyleDeclaration.prototype, "sheet", { get: function(){ if(!this.sheet$2) this.sheet$2 = new CSSStyleSheet; return this.sheet$2; }});
// these are default properties of a style object
CSSStyleDeclaration.prototype.animationDelay =
CSSStyleDeclaration.prototype.animationDuration =
CSSStyleDeclaration.prototype.transitionDelay =
CSSStyleDeclaration.prototype.transitionDuration ="";
CSSStyleDeclaration.prototype.textTransform = "none", // acid test 46
CSSStyleDeclaration.prototype.toString = function() { return "style object" };
CSSStyleDeclaration.prototype.getPropertyValue = function(p) {
p = mw$.camelCase(p);
                if (this[p] == undefined)                
                        this[p] = "";
                        return this[p];
};
CSSStyleDeclaration.prototype.getProperty = function(p) {
p = mw$.camelCase(p);
return this[p] ? this[p] : "";
};
CSSStyleDeclaration.prototype.setProperty = function(p, v, prv) {
p = mw$.camelCase(p);
this[p] = v;
var pri = p + "$pri";
this[pri] = (prv === "important");
};
CSSStyleDeclaration.prototype.getPropertyPriority = function(p) {
p = mw$.camelCase(p);
var pri = p + "$pri";
return this[pri] ? "important" : "";
};
Object.defineProperty(CSSStyleDeclaration.prototype, "css$data", {
get: function() { var s = ""; for(var i=0; i<this.childNodes.length; ++i) if(this.childNodes[i].nodeName == "#text") s += this.childNodes[i].data; return s; }});

document.defaultView = window;

HTMLTableElement.prototype.insertRow = mw$.insertRow;
z$tBody.prototype.insertRow = mw$.insertRow;
z$tHead.prototype.insertRow = mw$.insertRow;
z$tFoot.prototype.insertRow = mw$.insertRow;

HTMLTableElement.prototype.deleteRow = mw$.deleteRow;
z$tBody.prototype.deleteRow = mw$.deleteRow;
z$tHead.prototype.deleteRow = mw$.deleteRow;
z$tFoot.prototype.deleteRow = mw$.deleteRow;

HTMLTableRowElement.prototype.insertCell = mw$.insertCell;
HTMLTableRowElement.prototype.deleteCell = mw$.deleteCell;

HTMLTableElement.prototype.createCaption = function() {
if(this.caption) return this.caption;
var c = my$doc().createElement("caption");
this.appendChild(c);
return c;
}
HTMLTableElement.prototype.deleteCaption = function() {
if(this.caption) this.removeChild(this.caption);
}

HTMLTableElement.prototype.createTHead = function() {
if(this.tHead) return this.tHead;
var c = my$doc().createElement("thead");
this.prependChild(c);
return c;
}
HTMLTableElement.prototype.deleteTHead = function() {
if(this.tHead) this.removeChild(this.tHead);
}

HTMLTableElement.prototype.createTFoot = function() {
if(this.tFoot) return this.tFoot;
var c = my$doc().createElement("tfoot");
this.insertBefore(c, this.caption);
return c;
}
HTMLTableElement.prototype.deleteTFoot = function() {
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
this.class = "";
}
TextNode.prototype = new HTMLElement;
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
c.ownerDocument = this;
/* A text node chould never have children, and does not need childNodes array,
 * but there is improper html out there <text> <stuff> </text>
 * which has to put stuff under the text node, so against this
 * unlikely occurence, I have to create the array.
 * I have to treat a text node like an html node. */
c.childNodes = [];
c.parentNode = null;
eb$logElement(c, "text");
return c;
}

Comment = function(t) {
this.data = t;
this.nodeName = this.tagName = "#comment";
this.nodeType = 8;
this.class = "";
}
Comment.prototype = new HTMLElement;
Comment.prototype.dom$class = "Comment";

document.createComment = function(t) {
if(t == undefined) t = "";
var c = new Comment(t);
c.ownerDocument = this;
c.childNodes = [];
c.parentNode = null;
eb$logElement(c, "comment");
return c;
}

// The Option class, these are choices in a dropdown list.
Option = function() {
this.nodeName = this.tagName = "OPTION";
this.nodeType = 1;
this.text = this.value = "";
if(arguments.length > 0)
this.text = arguments[0];
if(arguments.length > 1)
this.value = arguments[1];
this.selected = false;
this.defaultSelected = false;
}
Option.prototype = new HTMLElement;
Option.prototype.dom$class = "Option";

document.getBoundingClientRect = function(){
return {
top: 0, bottom: 0, left: 0, right: 0,
x: 0, y: 0,
width: 0, height: 0
}
}

// The Attr class and getAttributeNode().
Attr = function(){ this.specified = false; this.owner = null; this.name = ""};
Attr.prototype.dom$class = "Attr";
Object.defineProperty(Attr.prototype, "value", {
get: function() { var n = this.name;
return n.substr(0,5) == "data-" ? (this.owner.dataset$2 ? this.owner.dataset$2[mw$.dataCamel(n)] :  null)  : this.owner[n]; },
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

document.getAttribute = mw$.getAttribute;
document.hasAttribute = mw$.hasAttribute;
document.getAttributeNames = mw$.getAttributeNames;
document.getAttributeNS = mw$.getAttributeNS;
document.hasAttributeNS = mw$.hasAttributeNS;
document.setAttribute = mw$.setAttribute;
document.markAttribute = mw$.markAttribute;
document.setAttributeNS = mw$.setAttributeNS;
document.removeAttribute = mw$.removeAttribute;
document.removeAttributeNS = mw$.removeAttributeNS;
document.getAttributeNode = mw$.getAttributeNode;

document.cloneNode = function(deep) {
cloneRoot1 = this;
return mw$.clone1 (this,deep);
}

/*********************************************************************
importNode seems to be the same as cloneNode, except it is copying a tree
of objects from another context into the current context.
But this is how quickjs works by default.
foo.s = cloneNode(bar.s);
If bar is in another context that's ok, we read those objects and create
copies of them in the current context.
*********************************************************************/

document.importNode = function(src, deep) { return src.cloneNode(deep); }

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
Event.prototype.dom$class = "Event";

Event.prototype.preventDefault = function(){ this.defaultPrevented = true; }

Event.prototype.stopPropagation = function(){ if(this.cancelable)this.cancelled = true; }

// deprecated - I guess - but a lot of people still use it.
Event.prototype.initEvent = function(t, bubbles, cancel) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel; this.defaultPrevented = false; }

Event.prototype.initUIEvent = function(t, bubbles, cancel, unused, detail) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel, this.detail = detail; this.defaultPrevented = false; }
Event.prototype.initCustomEvent = function(t, bubbles, cancel, detail) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel, this.detail = detail; }

document.createEvent = function(unused) { return new Event; }

MouseEvent = function(etype){
    this.bubbles =     this.cancelable = true;
    this.cancelled = this.defaultPrevented = false;
    this.currentTarget =     this.target = null;
    this.eventPhase = 0;
    this.timeStamp = new Date().getTime();
if(typeof etype == "string") this.type = etype;
};
MouseEvent.prototype = new Event;
MouseEvent.prototype.altKey = false;
MouseEvent.prototype.ctrlKey = false;
MouseEvent.prototype.shiftKey = false;
MouseEvent.prototype.metaKey = false;
MouseEvent.prototype.initMouseEvent = function() { this.initEvent.apply(this, arguments)}

CustomEvent = function(etype, o){
alert3("customEvent " + etype + " " + typeof o);
    this.bubbles =     this.cancelable = true;
    this.cancelled = this.defaultPrevented = false;
    this.currentTarget =     this.target = null;
    this.eventPhase = 0;
    this.timeStamp = new Date().getTime();
if(typeof etype == "string") this.type = etype;
// This is nowhere documented.
// I'm basing it on some js I saw in the wild.
if(typeof o == "object")
this.name = o.name, this.detail = o.detail;
};
CustomEvent.prototype = new Event;

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

document.insertAdjacentHTML = mw$.insertAdjacentHTML;

/*********************************************************************
Add prototype methods to the standard nodes, nodes that have children,
and the normal set of methods to go with those children.
Form has children for sure, but if we add <input> to Form,
we also have to add it to the array Form.elements.
So there are some nodes that we have to do outside this loop.
Again, leading ; to avert a parsing ambiguity.
*********************************************************************/

; (function() {
var c = window.Node;
var p = c.prototype;
// These subordinate objects are on-demand.
Object.defineProperty( p, "dataset", { get: function(){ return this.dataset$2 ? this.dataset$2 : this.dataset$2 = {}; }});
Object.defineProperty( p, "attributes", { get: function(){ if(!this.attributes$2) this.attributes$2 = new NamedNodeMap, this.attributes$2.owner = this, this.attributes$2.ownerDocument = my$doc(); return this.attributes$2;}});
Object.defineProperty( p, "style", { get: function(){ if(!this.style$2) this.style$2 = new CSSStyleDeclaration, this.style$2.element = this; return this.style$2;}});
// get elements below
p.getElementsByTagName = mw$.getElementsByTagName;
p.getElementsByName = mw$.getElementsByName;
p.getElementsByClassName = mw$.getElementsByClassName;
p.contains = mw$.nodeContains;
p.querySelectorAll = querySelectorAll;
p.querySelector = querySelector;
p.matches = querySelector0;
p.closest = function(s) { var u = this; while(u.nodeType == 1) { if(u.matches(s)) return u; u = u.parentNode; } return null; }
// children
p.hasChildNodes = mw$.hasChildNodes;
p.appendChild = mw$.appendChild;
p.prependChild = mw$.prependChild;
p.insertBefore = mw$.insertBefore;
p.insertAdjacentElement = mw$.insertAdjacentElement;
p.append = mw$.append;
p.prepend = mw$.prepend;
p.before = mw$.before;
p.after = mw$.after;
p.replaceWith = mw$.replaceWith;
p.replaceChild = mw$.replaceChild;
// These are native, so it's ok to bounce off of document.
p.eb$apch1 = document.eb$apch1;
p.eb$apch2 = document.eb$apch2;
p.eb$insbf = document.eb$insbf;
p.removeChild = document.removeChild;
p.remove = function() { if(this.parentNode) this.parentNode.removeChild(this);}
Object.defineProperty(p, "firstChild", { get: function() { return (this.childNodes && this.childNodes.length) ? this.childNodes[0] : null; } });
Object.defineProperty(p, "firstElementChild", { get: function() { var u = this.childNodes; if(!u) return null; for(var i=0; i<u.length; ++i) if(u[i].nodeType == 1) return u[i]; return null; }});
Object.defineProperty(p, "lastChild", { get: function() { return (this.childNodes && this.childNodes.length) ? this.childNodes[this.childNodes.length-1] : null; } });
Object.defineProperty(p, "lastElementChild", { get: function() { var u = this.childNodes; if(!u) return null; for(var i=u.length-1; i>=0; --i) if(u[i].nodeType == 1) return u[i]; return null; }});
Object.defineProperty(p, "childElementCount", { get: function() { var z=0, u = this.childNodes; if(!u) return z; for(var i=0; i<u.length; ++i) if(u[i].nodeType == 1) ++z; return z; }});
Object.defineProperty(p, "nextSibling", { get: function() { return mw$.eb$getSibling(this,"next"); } });
Object.defineProperty(p, "nextElementSibling", { get: function() { return mw$.eb$getElementSibling(this,"next"); } });
Object.defineProperty(p, "previousSibling", { get: function() { return mw$.eb$getSibling(this,"previous"); } });
Object.defineProperty(p, "previousElementSibling", { get: function() { return mw$.eb$getElementSibling(this,"previous"); } });
// children is subtly different from childnodes; this code taken from
// https://developer.mozilla.org/en-US/docs/Web/API/ParentNode/children
Object.defineProperty(p, 'children', {
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
p.hasAttribute = mw$.hasAttribute;
p.hasAttributeNS = mw$.hasAttributeNS;
p.markAttribute = mw$.markAttribute;
p.getAttribute = mw$.getAttribute;
p.getAttributeNS = mw$.getAttributeNS;
p.getAttributeNames = mw$.getAttributeNames;
p.setAttribute = mw$.setAttribute;
p.setAttributeNS = mw$.setAttributeNS;
p.removeAttribute = mw$.removeAttribute;
p.removeAttributeNS = mw$.removeAttributeNS;
/* which one is it?
Object.defineProperty(p, "className", { get: function() { return this.getAttribute("class"); }, set: function(h) { this.setAttribute("class", h); }});
*/
Object.defineProperty(p, "className", { get: function() { return this.class; }, set: function(h) { this.class = h; }});
Object.defineProperty(p, "parentElement", { get: function() { return this.parentNode && this.parentNode.nodeType == 1 ? this.parentNode : null; }});
p.getAttributeNode = mw$.getAttributeNode;
p.getClientRects = function(){ return []; }
// clone
p.cloneNode = document.cloneNode;
p.importNode = document.importNode;
p.compareDocumentPosition = mw$.compareDocumentPosition;
// visual
p.focus = function(){document.activeElement=this}
p.blur = blur;
p.getBoundingClientRect = document.getBoundingClientRect;
// events
p.eb$listen = eb$listen;
p.eb$unlisten = eb$unlisten;
p.addEventListener = addEventListener;
p.removeEventListener = removeEventListener;
if(mw$.attachOn) {
p.attachEvent = attachEvent;
p.detachEvent = detachEvent;
}
p.dispatchEvent = mw$.dispatchEvent;
p.insertAdjacentHTML = mw$.insertAdjacentHTML;
// outerHTML is dynamic; should innerHTML be?
Object.defineProperty(p, "outerHTML", { get: function() { return mw$.htmlString(this);},
set: function(h) { mw$.outer$1(this,h); }});
p.injectSetup = mw$.injectSetup;
// constants
p.ELEMENT_NODE = 1, p.TEXT_NODE = 3, p.COMMENT_NODE = 8, p.DOCUMENT_NODE = 9, p.DOCUMENT_TYPE_NODE = 10, p.DOCUMENT_FRAGMENT_NODE = 11;
// default tabIndex is 0 but running js can override this.
p.tabIndex = 0;
// class and text methods
Object.defineProperty(p, "classList", { get : function() { return mw$.classList(this);}});
p.cl$present = true;
Object.defineProperty(p, "textContent", {
get: function() { return mw$.textUnder(this, 0); },
set: function(s) { return mw$.newTextUnder(this, s, 0); }});
Object.defineProperty(p, "contentText", {
get: function() { return mw$.textUnder(this, 1); },
set: function(s) { return mw$.newTextUnder(this, s, 1); }});
Object.defineProperty(p, "nodeValue", {
get: function() { return this.nodeType == 3 ? this.data : null;},
set: function(h) { if(this.nodeType == 3) this.data = h; }});
p.clientHeight = 16;
p.clientWidth = 120;
p.scrollHeight = 16;
p.scrollWidth = 120;
p.scrollTop = 0;
p.scrollLeft = 0;
p.offsetHeight = 16;
p.offsetWidth = 120;
p.dir = "auto";
})();

// This is needed by mozilla, not by duktape, not sure how duktape
// skates past it. See the first call to apch1 in decorate.c.
// If this is <html> from the expanded frame, linking into an object
// of class Document, not the window document, it still has to work.
Document.prototype.eb$apch1 = document.eb$apch1;

HTMLFormElement.prototype.appendChildNative = mw$.appendChild;
HTMLFormElement.prototype.appendChild = mw$.formAppendChild;
HTMLFormElement.prototype.insertBeforeNative = mw$.insertBefore;
HTMLFormElement.prototype.insertBefore = mw$.formInsertBefore;
HTMLFormElement.prototype.removeChildNative = document.removeChild;
HTMLFormElement.prototype.removeChild = mw$.formRemoveChild;

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

z$Select.prototype.appendChild = function(newobj) {
if(!newobj) return null;
// should only be options!
if(!(newobj.dom$class == "Option")) return newobj;
mw$.isabove(newobj, this);
if(newobj.parentNode) newobj.parentNode.removeChild(newobj);
var l = this.childNodes.length;
if(newobj.defaultSelected) newobj.selected = true, this.selectedIndex = l;
this.childNodes.push(newobj); newobj.parentNode = this;
mutFixup(this, false, newobj, null);
return newobj;
}
z$Select.prototype.insertBefore = function(newobj, item) {
var i;
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(!(newobj.dom$class == "Option")) return newobj;
mw$.isabove(newobj, this);
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
z$Select.prototype.removeChild = function(item) {
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

z$Select.prototype.add = function(o, idx) {
var n = this.options.length;
if(typeof idx != "number" || idx < 0 || idx > n) idx = n;
if(idx == n) this.appendChild(o);
else this.insertBefore(o, this.options[idx]);
}
z$Select.prototype.remove = function(idx) {
var n = this.options.length;
if(typeof idx == "number" && idx >= 0 && idx < n)
this.removeChild(this.options[idx]);
}

// rows under a table body
z$tBody.prototype.appendChildNative = mw$.appendChild;
z$tBody.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return mw$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.dom$class == "HTMLTableRowElement") // shouldn't be anything other than TR
this.rows.push(newobj), rowReindex(this);
return newobj;
}
z$tBody.prototype.insertBeforeNative = mw$.insertBefore;
z$tBody.prototype.insertBefore = function(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return mw$.insertFragment(this, newobj, item);
var r = this.insertBeforeNative(newobj, item);
if(!r) return null;
if(newobj.dom$class == "HTMLTableRowElement")
for(var i=0; i<this.rows.length; ++i)
if(this.rows[i] == item) {
this.rows.splice(i, 0, newobj);
rowReindex(this);
break;
}
return newobj;
}
z$tBody.prototype.removeChildNative = document.removeChild;
z$tBody.prototype.removeChild = function(item) {
if(!item) return null;
if(!this.removeChildNative(item))
return null;
if(item.dom$class == "HTMLTableRowElement")
for(var i=0; i<this.rows.length; ++i)
if(this.rows[i] == item) {
this.rows.splice(i, 1);
rowReindex(this);
break;
}
return item;
}

// head and foot are just like body
z$tHead.prototype.appendChildNative = mw$.appendChild;
z$tHead.prototype.appendChild = z$tBody.prototype.appendChild;
z$tHead.prototype.insertBeforeNative = mw$.insertBefore;
z$tHead.prototype.insertBefore = z$tBody.prototype.insertBefore;
z$tHead.prototype.removeChildNative = document.removeChild;
z$tHead.prototype.removeChild = z$tBody.prototype.removeChild;
z$tFoot.prototype.appendChildNative = mw$.appendChild;
z$tFoot.prototype.appendChild = z$tBody.prototype.appendChild;
z$tFoot.prototype.insertBeforeNative = mw$.insertBefore;
z$tFoot.prototype.insertBefore = z$tBody.prototype.insertBefore;
z$tFoot.prototype.removeChildNative = document.removeChild;
z$tFoot.prototype.removeChild = z$tBody.prototype.removeChild;

// rows or bodies under a table
HTMLTableElement.prototype.appendChildNative = mw$.appendChild;
HTMLTableElement.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return mw$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.dom$class == "HTMLTableRowElement") rowReindex(this);
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
HTMLTableElement.prototype.insertBeforeNative = mw$.insertBefore;
HTMLTableElement.prototype.insertBefore = function(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return mw$.insertFragment(this, newobj, item);
var r = this.insertBeforeNative(newobj, item);
if(!r) return null;
if(newobj.dom$class == "HTMLTableRowElement") rowReindex(this);
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
HTMLTableElement.prototype.removeChildNative = document.removeChild;
HTMLTableElement.prototype.removeChild = function(item) {
if(!item) return null;
if(!this.removeChildNative(item))
return null;
if(item.dom$class == "HTMLTableRowElement") rowReindex(this);
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

HTMLTableRowElement.prototype.appendChildNative = mw$.appendChild;
HTMLTableRowElement.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return mw$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.nodeName === "TD") // shouldn't be anything other than TD
this.cells.push(newobj);
return newobj;
}
HTMLTableRowElement.prototype.insertBeforeNative = mw$.insertBefore;
HTMLTableRowElement.prototype.insertBefore = function(newobj, item) {
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(newobj.nodeType == 11) return mw$.insertFragment(this, newobj, item);
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
HTMLTableRowElement.prototype.removeChildNative = document.removeChild;
HTMLTableRowElement.prototype.removeChild = function(item) {
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
var cnlist = ["HTMLElement.prototype", "document", "window"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
// there are lots more events, onmouseout etc, that we don't responnd to,
// should we watch for them anyways?
var evs = ["onload", "onunload", "onclick", "onchange", "oninput",
"onsubmit", "onreset", "onmessage"];
for(var j=0; j<evs.length; ++j) {
var evname = evs[j];
eval(cn + '["' + evname + '$$watch"] = true');
eval('Object.defineProperty(' + cn + ', "' + evname + '", { \
get: function() { return this.' + evname + '$2; }, \
set: function(f) { if(db$flags(1)) alert3((this.'+evname+'?"clobber ":"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".' + evname + '"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.' + evname + '$2 = f}}})')
}}})();

document.createElementNS = function(nsurl,s) {
var mismatch = false;
var u = this.createElement(s);
if(!u) return null;
if(!nsurl) nsurl = "";
u.namespaceURI = new z$URL(nsurl);
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
// throw error code 14
return null;
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
// I'll just return null and tweak acid3 accordingly.
// throw error code 5
return null;
}

switch(t) {
case "body": c = new HTMLBodyElement; break;
case "object": c = new HtmlObj; break;
case "a": c = new HTMLAnchorElement; break;
case "area": c = new HTMLAreaElement; break;
case "image": t = "img";
case "img": c = new HTMLImageElement; break;
case "link": c = new HTMLLinkElement; break;
case "meta": c = new z$Meta; break;
case "cssstyledeclaration": case "style":
c = new CSSStyleDeclaration; c.element = null; break;
case "script": c = new HTMLScriptElement; break;
case "div": c = new z$Div; break;
case "label": c = new z$Label; break;
case "p": c = new z$P; break;
case "header": c = new z$Header; break;
case "footer": c = new z$Footer; break;
case "table": c = new HTMLTableElement; break;
case "tbody": c = new z$tBody; break;
case "tr": c = new HTMLTableRowElement; break;
case "td": c = new HTMLTableCellElement; break;
case "caption": c = new z$tCap; break;
case "thead": c = new z$tHead; break;
case "tfoot": c = new z$tFoot; break;
case "canvas": c = new z$Canvas; break;
case "audio": case "video": c = new z$Audio; break;
case "document": c = new Document; break;
case "fragment": c = new DocumentFragment; break;
case "frame": c = new HTMLFrameElement; break;
case "iframe": c = new HTMLIFrameElement; break;
case "select": c = new z$Select; break;
case "option":
c = new Option;
c.childNodes = [];
// we don't log options because rebuildSelectors() checks
// the dropdown lists after every js run.
return c;
case "form": c = new HTMLFormElement; break;
case "input": case "element": case "textarea":
c = new z$Element;
if(t == "textarea") c.type = t;
break;
case "button": c = new z$Element; c.type = "submit"; break;
default:
/* alert("createElement default " + s); */
c = new HTMLElement;
}

c.childNodes = [];
if(c.dom$class == "Select") c.options = c.childNodes;
c.parentNode = null;
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
c.ownerDocument = this;
eb$logElement(c, t);
if(c.nodeType == 1) c.id = c.name = "";

return c;
} 

document.createDocumentFragment = function() {
var c = this.createElement("fragment");
c.nodeType = 11;
c.nodeName = c.tagName = "#document-fragment";
return c;
}

document.implementation = {
owner: document,
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
return owner.createElement(tagstrip);
},
// https://developer.mozilla.org/en-US/docs/Web/API/DOMImplementation/createHTMLDocument
createHTMLDocument: function(t) {
if(t == undefined) t = "Empty"; // the title
var f = this.owner.createElement("iframe");
var d = f.contentDocument; // this is the created document
d.title = t;
return d;
},
createDocument: function(uri, str, t) {
// I don't know if this is right at all, but it's quick and easy
var doc = document.createElementNS(uri, "document");
var below = document.createElementNS(uri, str);
if(!doc || !below) { alert3("createDocument unable to create document or " + str + " tag for namespace " + uri); return null; }
doc.appendChild(below);
doc.documentElement = below;
return doc;
}
};

// Extra things, beyond Node, that should be in Document.prototype
Document.prototype.getElementById = mw$.getElementById;
Document.prototype.nodeContains = mw$.nodeContains;
Document.prototype.createNodeIterator = mw$.createNodeIterator;
Document.prototype.createTreeWalker = mw$.createTreeWalker;
Document.prototype.createElement = document.createElement;
Document.prototype.createElementNS = document.createElementNS;
Document.prototype.createTextNode = document.createTextNode;
Document.prototype.createComment = document.createComment;
Document.prototype.createDocumentFragment = document.createDocumentFragment;
Document.prototype.createEvent = document.createEvent;
Document.prototype.implementation = document.implementation;

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
this.$entire =  eb$fetchHTTP.call(this, urlcopy,this.method,headerstring,data);
if(this.$entire != "async") this.parseResponse();
};
XMLHttpRequest.prototype.parseResponse = function(){
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

// pages seem to want document.style to exist
document.style = new CSSStyleDeclaration;
document.style.element = document;
document.style.bgcolor = "white";

document.ELEMENT_NODE = 1, document.TEXT_NODE = 3, document.COMMENT_NODE = 8, document.DOCUMENT_NODE = 9, document.DOCUMENT_TYPE_NODE = 10, document.DOCUMENT_FRAGMENT_NODE = 11;

// originally ms extension pre-DOM, we don't fully support it
//but offer the legacy document.all.tags method.
document.all = {};
document.all.tags = function(s) {
return mw$.eb$gebtn(document.body, s.toLowerCase());
}

eb$demin = mw$.deminimize;
eb$watch = mw$.addTrace;
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
get: function() { return this.childNodes[0]; }});
Object.defineProperty(document, "firstElementChild", {
get: function() { return this.childNodes[1]; }});
Object.defineProperty(document, "lastChild", {
get: function() { return this.childNodes[document.childNodes.length-1]; }});
Object.defineProperty(document, "lastElementChild", {
get: function() { return this.childNodes[document.childNodes.length-1]; }});
Object.defineProperty(document, "nextSibling", {
get: function() { return mw$.eb$getSibling(this,"next"); }});
Object.defineProperty(document, "nextElementSibling", {
get: function() { return mw$.eb$getElementSibling(this,"next"); }});
Object.defineProperty(document, "previousSibling", {
get: function() { return mw$.eb$getSibling(this,"previous"); }});
Object.defineProperty(document, "previousElementSibling", {
get: function() { return mw$.eb$getElementSibling(this,"previous"); }});

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

// Local storage, this is per window.
// Then there's sessionStorage, and honestly I don't understand the difference.
// This is NamedNodeMap, to take advantage of preexisting methods.
localStorage = {}
localStorage.attributes = new NamedNodeMap;
localStorage.attributes.owner = localStorage;
// tell me we don't have to do NS versions of all these.
localStorage.getAttribute = mw$.getAttribute;
localStorage.getItem = localStorage.getAttribute;
localStorage.setAttribute = mw$.setAttribute;
localStorage.setItem = localStorage.setAttribute;
localStorage.removeAttribute = mw$.removeAttribute;
localStorage.removeItem = localStorage.removeAttribute;
localStorage.clear = function() {
var l;
while(l = localStorage.attributes.length)
localStorage.removeItem(localStorage.attributes[l-1].name);
}

sessionStorage = {}
sessionStorage.attributes = new NamedNodeMap;
sessionStorage.attributes.owner = sessionStorage;
sessionStorage.getAttribute = mw$.getAttribute;
sessionStorage.getItem = sessionStorage.getAttribute;
sessionStorage.setAttribute = mw$.setAttribute;
sessionStorage.setItem = sessionStorage.setAttribute;
sessionStorage.removeAttribute = mw$.removeAttribute;
sessionStorage.removeItem = sessionStorage.removeAttribute;
sessionStorage.clear = function() {
var l;
while(l = sessionStorage.attributes.length)
sessionStorage.removeItem(sessionStorage.attributes[l-1].name);
}

/*********************************************************************
I don't need to do any of these Array methods for mozjs or v8 or quick,
because these methods are inbuilt.
The only one they don't have is item, so I better leave that one in.
*********************************************************************/

Array.prototype.item = function(x) { return this[x] };
Object.defineProperty(Array.prototype, "item", { enumerable: false});

// On the first call this setter just creates the url, the location of the
// current web page, But on the next call it has the side effect of replacing
// the web page with the new url.
Object.defineProperty(window, "location", {
get: function() { return window.location$2; },
set: function(h) {
if(!window.location$2) {
window.location$2 = new z$URL(h);
} else {
window.location$2.href = h;
}
}});
Object.defineProperty(document, "location", {
get: function() { return this.location$2; },
set: function(h) {
if(!this.location$2) {
this.location$2 = new z$URL(h);
} else {
this.location$2.href = h;
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

// Apply rules to a given style object, which is this.
Object.defineProperty(CSSStyleDeclaration.prototype, "cssText", { get: mw$.cssTextGet,
set: function(h) { var w = my$win(); w.soj$ = this; eb$cssText.call(this,h); delete w.soj$; } });

function eb$qs$start() { mw$.cssGather(true); }

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
return d.createTextNode(t);
}
alert3("trying to use the DOM parser\n" + y + " <<< ");
alert4(t);
alert3(">>>");
return d.createTextNode("DOMParser not yet implemented");
}}};

XMLSerializer = function(){}
XMLSerializer.prototype.serializeToString = function(root) {
alert3("trying to use XMLSerializer");
return "<div>XMLSerializer not yet implemented</div>"; }

css$ver = 0;
document.xmlVersion = 0;

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
if(cfg.attributes$2) this.attr = true;
if(cfg.childList) this.kids = true;
if(cfg.subtree) this.subtree = true;
this.active = true;
}
MutationObserver.prototype.takeRecords = function() { return []}

MutationRecord = function(){};
MutationRecord.prototype.dom$class = "MutationRecord";

mutList = [];

crypto = {};
crypto.getRandomValues = function(a) {
if(typeof a != "object") return NULL;
var l = a.length;
for(var i=0; i<l; ++i) a[i] = Math.floor(Math.random()*0x100000000);
return a;
}

ra$step = 0;
requestAnimationFrame = function() {
// This absolutely doesn't do anything. What is edbrowse suppose to do with animation?
return ++ra$step;
}

cancelAnimationFrame = eb$voidfunction;

// link in the blob code
Blob = mw$.Blob
File = mw$.File
FileReader = mw$.FileReader
URL.createObjectURL = mw$.URL.createObjectURL
URL.revokeObjectURL = mw$.URL.revokeObjectURL
MessagePortPolyfill = mw$.MessagePortPolyfill;
MessageChannel = MessageChannelPolyfill = mw$.MessageChannelPolyfill;
URLSearchParams = mw$.URLSearchParams

