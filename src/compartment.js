/*********************************************************************
See the comments in master.js.
These are the per window functions and classes.
*********************************************************************/

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
dumptree = mw0.dumptree;
uptrace = mw0.uptrace;
showscripts = mw0.showscripts;
searchscripts = mw0.searchscripts;
snapshot = mw0.snapshot;
aloop = mw0.aloop;
step$stack = mw0.step$stack;
step$l = 0;
step$go = "";
// First line of js in the base file of your snapshot could be
// step$l = 0, step$go = "c275";

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

console = {
log: function(obj) { mw0.eb$logtime(3, "log", obj); },
info: function(obj) { mw0.eb$logtime(3, "info", obj); },
warn: function(obj) { mw0.eb$logtime(3, "warn", obj); },
error: function(obj) { mw0.eb$logtime(3, "error", obj); },
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

navigator = new Object;
navigator.appName = "edbrowse";
navigator["appCode Name"] = "edbrowse C/SpiderMonkey";
/* not sure what product is about */
navigator.product = "edbrowse";
navigator.productSub = "3";
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
history = new Object;
history.length = 1;
history.next = "";
history.previous = "";
history.back = eb$voidfunction;
history.forward = eb$voidfunction;
history.go = eb$voidfunction;
history.pushState = eb$voidfunction;
history.replaceState = eb$voidfunction;
history.toString = function() {
 return "Sorry, edbrowse does not maintain a browsing history.";
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
URL.prototype.rebuild = mw0.URL.prototype.rebuild;
Object.defineProperty(URL.prototype, "rebuild", {enumerable:false});
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
this.port$val = mw0.eb$setDefaultPort(this.protocol$val);
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

Node = function(){}; Node.prototype.dom$class = "Node";

HTML = function(){};
HTML.prototype = {
dom$class: "HTML",
doScroll: eb$voidfunction,
clientHeight: 768, clientWidth: 1024,
offsetHeight: 768, offsetWidth: 1024,
 scrollHeight: 768, scrollWidth: 1024,
scrollTop: 0, scrollLeft: 0
};

// is there a difference between DocType ad DocumentType?
DocType = function(){ this.nodeType = 10, this.nodeName = "DOCTYPE";}; DocType.prototype.dom$class = "DocType";
DocumentType = function(){}; DocumentType.prototype.dom$class = "DocumentType";
CharacterData = function(){}; CharacterData.prototype.dom$class = "CharacterData";
Head = function(){}; Head.prototype.dom$class = "Head";
Meta = function(){}; Meta.prototype.dom$class = "Meta";
Title = function(){}; Title.prototype.dom$class = "Title";
Object.defineProperty(Title.prototype, "text", {
get: function(){ return this.firstChild && this.firstChild.nodeName == "#text" && this.firstChild.data || "";}
// setter should change the title of the document, not yet implemented
});
Link = function(){}; Link.prototype.dom$class = "Link";
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
Base = function(){}; Base.prototype.dom$class = "Base";
Form = function(){ this.elements = []; }; Form.prototype.dom$class = "Form";
Form.prototype.submit = eb$formSubmit;
Form.prototype.reset = eb$formReset;
Object.defineProperty(Form.prototype, "length", { get: function() { return this.elements.length;}});

Validity = function(){}; Validity.prototype.dom$class = "Validity";
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

Element = function() { this.validity = new Validity, this.validity.owner = this}; Element.prototype.dom$class = "Element";
Element.prototype.selectionStart = 0;
Element.prototype.selectionEnd = -1;
Element.prototype.selectionDirection = "none";

// I really don't know what this function does, something visual I think.
Element.prototype.setSelectionRange = function(s, e, dir) {
if(typeof s == "number") this.selectionStart = s;
if(typeof e == "number") this.selectionEnd = e;
if(typeof dir == "string") this.selectionDirection = dir;
}

Element.prototype.click = mw0.Element.prototype.click;
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

Object.defineProperty(Element.prototype, "innerText", {
get: function() { return this.type == "textarea" ? this.value : null },
set: function(t) { if(this.type == "textarea") this.value = t; }});

textarea$html$crossover = mw0.textarea$html$crossover;

HTMLElement = function(){}; HTMLElement.prototype.dom$class = "HTMLElement";
Select = function() { this.selectedIndex = -1; this.value = "";this.validity = new Validity, this.validity.owner = this}; Select.prototype.dom$class = "Select";
Object.defineProperty(Select.prototype, "value", {
get: function() {
var a = this.options;
var n = this.selectedIndex;
return (this.multiple || n < 0 || n >= a.length) ? "" : a[n].value;
}});
Image = function(){}; Image.prototype.dom$class = "Image";
Frame = function(){}; Frame.prototype.dom$class = "Frame";
Anchor = function(){}; Anchor.prototype.dom$class = "Anchor";
HTMLAnchorElement = function(){}; HTMLAnchorElement.prototype.dom$class = "HTMLAnchorElement";
HTMLLinkElement = function(){}; HTMLLinkElement.prototype.dom$class = "HTMLLinkElement";
HTMLAreaElement = function(){}; HTMLAreaElement.prototype.dom$class = "HTMLAreaElement";
Lister = function(){}; Lister.prototype.dom$class = "Lister";
Listitem = function(){}; Listitem.prototype.dom$class = "Listitem";
tBody = function(){ this.rows = []; }; tBody.prototype.dom$class = "tBody";
tHead = function(){ this.rows = []; }; tHead.prototype.dom$class = "tHead";
tFoot = function(){ this.rows = []; }; tFoot.prototype.dom$class = "tFoot";
tCap = function(){}; tCap.prototype.dom$class = "tCap";
Table = function(){ this.rows = []; this.tBodies = []; }; Table.prototype.dom$class = "Table";
Div = function(){}; Div.prototype.dom$class = "Div";
Div.prototype.doScroll = eb$voidfunction;
Div.prototype.click = mw0.Div.prototype.click;
Label = function(){}; Label.prototype.dom$class = "Label";
Object.defineProperty(Label.prototype, "htmlFor", { get: function() { return this.getAttribute("for"); }, set: function(h) { this.setAttribute("for", h); }});
HtmlObj = function(){}; HtmlObj.prototype.dom$class = "HtmlObj";
Area = function(){}; Area.prototype.dom$class = "Area";
Span = function(){}; Span.prototype.dom$class = "Span";
Span.prototype.doScroll = eb$voidfunction;
tRow = function(){ this.cells = []; }; tRow.prototype.dom$class = "tRow";
Cell = function(){}; Cell.prototype.dom$class = "Cell";
P = function(){}; P.prototype.dom$class = "P";
Header = function(){}; Header.prototype.dom$class = "Header";
Footer = function(){}; Footer.prototype.dom$class = "Footer";
Script = function(){}; Script.prototype.dom$class = "Script";
Script.prototype.type = "";
Script.prototype.text = "";
HTMLScriptElement = Script; // alias for Script, I guess
Timer = function(){this.nodeName = "TIMER";}; Timer.prototype.dom$class = "Timer";
Audio = function(){}; Audio.prototype.dom$class = "Audio";
Audio.prototype.play = eb$voidfunction;

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
if(!this.href$2) { this.href$2 = new mw0.URL(h ? eb$resolveURL(w.eb$base,h) : h) } else { if(!this.href$2.href$val && h) h =  eb$resolveURL(w.eb$base,h); \
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

Canvas = function() {}; Canvas.prototype.dom$class = "Canvas";
Canvas.prototype.getContext = mw0.Canvas.prototype.getContext;
Canvas.prototype.toDataURL = mw0.Canvas.prototype.toDataURL;

postMessage = mw0.postMessage;

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

Document = function(){}; Document.prototype.dom$class = "Document";

CSSStyleSheet = function() { this.cssRules = []; }; CSSStyleSheet.prototype.dom$class = "CSSStyleSheet";
CSSStyleSheet.prototype.insertRule = mw0.CSSStyleSheet.prototype.insertRule;
CSSStyleSheet.prototype.addRule = mw0.CSSStyleSheet.prototype.addRule;

CSSStyleDeclaration = function(){
        this.element = null;
        this.style = this;
	 this.attributes = new mw0.NamedNodeMap;
this.ownerDocument = my$doc();
this.attributes.owner = this;
this.sheet = new mw0.CSSStyleSheet;
};
CSSStyleDeclaration.prototype.dom$class = "CSSStyleDeclaration";
CSSStyleDeclaration.prototype.animationDelay =
CSSStyleDeclaration.prototype.animationDuration =
CSSStyleDeclaration.prototype.transitionDelay =
CSSStyleDeclaration.prototype.transitionDuration ="";
CSSStyleDeclaration.prototype.textTransform = "none", // acid test 46
CSSStyleDeclaration.prototype.toString = mw0.CSSStyleDeclaration.prototype.toString;
CSSStyleDeclaration.prototype.getPropertyValue = mw0.CSSStyleDeclaration.prototype.getPropertyValue;
CSSStyleDeclaration.prototype.getProperty = mw0.CSSStyleDeclaration.prototype.getProperty;
CSSStyleDeclaration.prototype.setProperty = mw0.CSSStyleDeclaration.prototype.setProperty;
CSSStyleDeclaration.prototype.getPropertyPriority = mw0.CSSStyleDeclaration.prototype.getPropertyPriority;
Object.defineProperty(CSSStyleDeclaration.prototype, "css$data", {
get: function() { var s = ""; for(var i=0; i<this.childNodes.length; ++i) if(this.childNodes[i].nodeName == "#text") s += this.childNodes[i].data; return s; }});
Object.defineProperty(CSSStyleDeclaration.prototype, "cssText", { get: mw0.cssTextGet, set: eb$cssText });

// pages seem to want document.style to exist
document.style = new CSSStyleDeclaration;
document.style.bgcolor = "white";
document.defaultView = window;
document.defaultView.getComputedStyle = mw0.getComputedStyle;

Table.prototype.insertRow = mw0.insertRow;
tBody.prototype.insertRow = mw0.insertRow;
tHead.prototype.insertRow = mw0.insertRow;
tFoot.prototype.insertRow = mw0.insertRow;
Table.prototype.deleteRow = mw0.deleteRow;
tBody.prototype.deleteRow = mw0.deleteRow;
tHead.prototype.deleteRow = mw0.deleteRow;
tFoot.prototype.deleteRow = mw0.deleteRow;
tRow.prototype.insertCell = mw0.insertCell;
tRow.prototype.deleteCell = mw0.deleteCell;
Table.prototype.createCaption = mw0.Table.prototype.createCaption;
Table.prototype.deleteCaption = mw0.Table.prototype.deleteCaption;
Table.prototype.createTHead = mw0.Table.prototype.createTHead;
Table.prototype.deleteTHead = mw0.Table.prototype.deleteTHead;
Table.prototype.createTFoot = mw0.Table.prototype.createTFoot;
Table.prototype.deleteTFoot = mw0.Table.prototype.deleteTFoot;

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
this.attributes = new mw0.NamedNodeMap;
this.attributes.owner = this;
}
TextNode.prototype.dom$class = "TextNode";
Object.defineProperty(TextNode.prototype, "data", {
get: function() { return this.data$2; },
set: function(s) { this.data$2 = s + ""; }});
document.createTextNode = mw0.createTextNode;

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
document.createComment = mw0.createComment;

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

document.getBoundingClientRect = mw0.getBoundingClientRect;
document.getElementsByTagName = mw0.getElementsByTagName;
document.getElementsByClassName = mw0.getElementsByClassName;
document.contains = mw0.nodeContains;
document.getElementsByName = mw0.getElementsByName;
document.getElementById = mw0.getElementById;
document.appendChild = mw0.appendChild;
document.prependChild = mw0.prependChild;
document.insertBefore = mw0.insertBefore;
document.replaceChild = mw0.replaceChild;
document.hasChildNodes = mw0.hasChildNodes;
document.childNodes = [];
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
get: function() { return mw0.eb$getSibling(this,"next"); }});
Object.defineProperty(document, "nextElementSibling", {
get: function() { return mw0.eb$getElementSibling(this,"next"); }});
Object.defineProperty(document, "previousSibling", {
get: function() { return mw0.eb$getSibling(this,"previous"); }});
Object.defineProperty(document, "previousElementSibling", {
get: function() { return mw0.eb$getElementSibling(this,"previous"); }});

Attr = function(){ this.specified = false; this.owner = null; this.name = ""; }; Attr.prototype.dom$class = "Attr";
Object.defineProperty(Attr.prototype, "value", {
get: function() { var n = this.name;
return n.substr(0,5) == "data-" ? (this.owner.dataset ? this.owner.dataset[mw0.dataCamel(n)] :  null)  : this.owner[n]; },
set: function(v) {
this.owner.setAttribute(this.name, v);
this.specified = true;
return;
}});
Attr.prototype.isId = mw0.Attr.prototype.isId;
NamedNodeMap = function() { this.length = 0; }; NamedNodeMap.prototype.dom$class = "NamedNodeMap";
NamedNodeMap.prototype.push = mw0.NamedNodeMap.prototype.push;
NamedNodeMap.prototype.item = mw0.NamedNodeMap.prototype.item;
NamedNodeMap.prototype.getNamedItem = mw0.NamedNodeMap.prototype.getNamedItem;
NamedNodeMap.prototype.setNamedItem = mw0.NamedNodeMap.prototype.setNamedItem;
NamedNodeMap.prototype.removeNamedItem = mw0.NamedNodeMap.prototype.removeNamedItem;

document.getAttribute = mw0.getAttribute;
document.getAttributeNS = mw0.getAttributeNS;
document.setAttribute = mw0.setAttribute;
document.setAttributeNS = mw0.setAttributeNS;
document.hasAttribute = mw0.hasAttribute;
document.hasAttributeNS = mw0.hasAttributeNS;
document.markAttribute = mw0.markAttribute;
document.removeAttribute = mw0.removeAttribute;
document.removeAttributeNS = mw0.removeAttributeNS;
document.getAttributeNode = mw0.getAttributeNode;
document.cloneNode = mw0.cloneNode;
cloneDebug = false;
document.importNode = mw0.importNode;
document.compareDocumentPosition = mw0.compareDocumentPosition;

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
Event.prototype.preventDefault = mw0.Event.prototype.preventDefault;
Event.prototype.stopPropagation = mw0.Event.prototype.stopPropagation;
Event.prototype.initEvent = mw0.Event.prototype.initEvent;
Event.prototype.initUIEvent = mw0.Event.prototype.initUIEvent;
Event.prototype.initCustomEvent = mw0.Event.prototype.initCustomEvent;

eb$listen = mw0.eb$listen;
eb$unlisten = mw0.eb$unlisten;
addEventListener = mw0.addEventListener;
removeEventListener = mw0.removeEventListener;
dispatchEvent = mw0.dispatchEvent;
document.createEvent = mw0.createEvent;
document.dispatchEvent = mw0.dispatchEvent;
document.eb$listen = mw0.eb$listen;
document.eb$unlisten = mw0.eb$unlisten;
document.addEventListener = mw0.addEventListener;
document.removeEventListener = mw0.removeEventListener;
if(mw0.attachOn) {
attachEvent = mw0.attachEvent;
detachEvent = mw0.detachEvent;
document.attachEvent = mw0.attachEvent;
document.detachEvent = mw0.detachEvent;
}
eventDebug = false;

MediaQueryList = function()
{
this.nodeName = "MediaQueryList";
this.matches = false;
this.media = "";
this.addEventListener = mw0.addEventListener;
this.removeEventListener = mw0.removeEventListener;
// supporting the above:
this.eb$listen = mw0.eb$listen;
this.eb$unlisten = mw0.eb$unlisten;
this.addListener = function(f) { this.addEventListener("mediaChange", f, false); }
this.removeListener = function(f) { this.removeEventListener("mediaChange", f, false); }
}

matchMedia = mw0.matchMedia;

document.insertAdjacentHTML = mw0.insertAdjacentHTML;
document.ELEMENT_NODE = 1, document.TEXT_NODE = 3, document.COMMENT_NODE = 8, document.DOCUMENT_NODE = 9, document.DOCUMENT_TYPE_NODE = 10, document.DOCUMENT_FRAGMENT_NODE = 11;
document.createElement = mw0.createElement;
document.createElementNS = mw0.createElementNS;
document.createDocumentFragment = mw0.createDocumentFragment;
document.implementation = mw0.implementation;
document.all = {};
document.all.tags = function(s) { 
return mw0.eb$gebtn(document.body, s.toLowerCase());
}

; (function() {
var cnlist = ["HTML", "HtmlObj", "Head", "Title", "Body", "CSSStyleDeclaration", "Frame",
"Anchor", "Element","HTMLElement", "Select", "Lister", "Listitem", "tBody", "Table", "Div",
"HTMLAnchorElement", "HTMLLinkElement", "HTMLAreaElement",
"tHead", "tFoot", "tCap", "Label",
"Form", "Span", "tRow", "Cell", "P", "Script", "Header", "Footer",
"Comment", "Node", "Area", "TextNode", "Image", "Option", "Link", "Meta", "Audio", "Canvas"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
var c = window[cn];
// c is class and cn is classname.
c.prototype.getElementsByTagName = mw0.getElementsByTagName;
c.prototype.getElementsByName = mw0.getElementsByName;
c.prototype.getElementsByClassName = mw0.getElementsByClassName;
c.prototype.contains = mw0.nodeContains;
c.prototype.querySelectorAll = querySelectorAll;
c.prototype.querySelector = querySelector;
c.prototype.matches = querySelector0;
c.prototype.hasChildNodes = mw0.hasChildNodes;
c.prototype.appendChild = mw0.appendChild;
c.prototype.prependChild = mw0.prependChild;
c.prototype.insertBefore = mw0.insertBefore;
c.prototype.replaceChild = mw0.replaceChild;
c.prototype.eb$apch1 = document.eb$apch1;
c.prototype.eb$apch2 = document.eb$apch2;
c.prototype.eb$insbf = document.eb$insbf;
c.prototype.removeChild = document.removeChild;
Object.defineProperty(c.prototype, "firstChild", { get: function() { return (this.childNodes && this.childNodes.length) ? this.childNodes[0] : null; } });
Object.defineProperty(c.prototype, "firstElementChild", { get: function() { var u = this.childNodes; if(!u) return null; for(var i=0; i<u.length; ++i) if(u[i].nodeType == 1) return u[i]; return null; }});
Object.defineProperty(c.prototype, "lastChild", { get: function() { return (this.childNodes && this.childNodes.length) ? this.childNodes[this.childNodes.length-1] : null; } });
Object.defineProperty(c.prototype, "lastElementChild", { get: function() { var u = this.childNodes; if(!u) return null; for(var i=u.length-1; i>=0; --i) if(u[i].nodeType == 1) return u[i]; return null; }});
Object.defineProperty(c.prototype, "nextSibling", { get: function() { return mw0.eb$getSibling(this,"next"); } });
Object.defineProperty(c.prototype, "nextElementSibling", { get: function() { return mw0.eb$getElementSibling(this,"next"); } });
Object.defineProperty(c.prototype, "previousSibling", { get: function() { return mw0.eb$getSibling(this,"previous"); } });
Object.defineProperty(c.prototype, "previousElementSibling", { get: function() { return mw0.eb$getElementSibling(this,"previous"); } });
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
c.prototype.hasAttribute = mw0.hasAttribute;
c.prototype.hasAttributeNS = mw0.hasAttributeNS;
c.prototype.markAttribute = mw0.markAttribute;
c.prototype.getAttribute = mw0.getAttribute;
c.prototype.getAttributeNS = mw0.getAttributeNS;
c.prototype.setAttribute = mw0.setAttribute;
c.prototype.setAttributeNS = mw0.setAttributeNS;
c.prototype.removeAttribute = mw0.removeAttribute;
c.prototype.removeAttributeNS = mw0.removeAttributeNS;
Object.defineProperty(c.prototype, "className", { get: function() { return this.class; }, set: function(h) { this.class = h; }});
Object.defineProperty(c.prototype, "parentElement", { get: function() { return this.parentNode && this.parentNode.nodeType == 1 ? this.parentNode : null; }});
c.prototype.getAttributeNode = mw0.getAttributeNode;
c.prototype.getClientRects = function(){ return []; }
c.prototype.cloneNode = mw0.cloneNode;
c.prototype.importNode = mw0.importNode;
c.prototype.compareDocumentPosition = mw0.compareDocumentPosition;
c.prototype.focus = focus;
c.prototype.blur = blur;
c.prototype.getBoundingClientRect = mw0.getBoundingClientRect; 
c.prototype.eb$listen = mw0.eb$listen;
c.prototype.eb$unlisten = mw0.eb$unlisten;
c.prototype.addEventListener = mw0.addEventListener;
c.prototype.removeEventListener = mw0.removeEventListener;
if(mw0.attachOn) {
c.prototype.attachEvent = mw0.attachEvent;
c.prototype.detachEvent = mw0.detachEvent;
}
c.prototype.dispatchEvent = mw0.dispatchEvent;
c.prototype.insertAdjacentHTML = mw0.insertAdjacentHTML;
Object.defineProperty(c.prototype, "outerHTML", { get: function() { return mw0.htmlString(this);},
set: function(h) { mw0.outer$1(this,h); }});
c.prototype.ELEMENT_NODE = 1, c.prototype.TEXT_NODE = 3, c.prototype.COMMENT_NODE = 8, c.prototype.DOCUMENT_NODE = 9, c.prototype.DOCUMENT_TYPE_NODE = 10, c.prototype.DOCUMENT_FRAGMENT_NODE = 11;
Object.defineProperty(c.prototype, "classList", { get : function() { return mw0.classList(this);}});
Object.defineProperty(c.prototype, "textContent", {
get: function() { return mw0.textUnder(this, 0); },
set: function(s) { return mw0.newTextUnder(this, s, 0); }});
Object.defineProperty(c.prototype, "contentText", {
get: function() { return mw0.textUnder(this, 1); },
set: function(s) { return mw0.newTextUnder(this, s, 1); }});
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

Form.prototype.appendChildNative = mw0.appendChild;
Form.prototype.appendChild = mw0.Form.prototype.appendChild;
Form.prototype.insertBeforeNative = mw0.insertBefore;
Form.prototype.insertBefore = mw0.Form.prototype.insertBefore;
Form.prototype.removeChildNative = document.removeChild;
Form.prototype.removeChild = mw0.Form.prototype.removeChild;

Select.prototype.appendChild = mw0.Select.prototype.appendChild;
Select.prototype.insertBefore = mw0.Select.prototype.insertBefore;
Select.prototype.removeChild = mw0.Select.prototype.removeChild;
Select.prototype.add = mw0.Select.prototype.add;
Select.prototype.remove = mw0.Select.prototype.remove;

Table.prototype.appendChildNative = mw0.appendChild;
Table.prototype.appendChild = mw0.Table.prototype.appendChild;
Table.prototype.insertBeforeNative = mw0.insertBefore;
Table.prototype.insertBefore = mw0.Table.prototype.insertBefore;
Table.prototype.removeChildNative = document.removeChild;
Table.prototype.removeChild = mw0.Table.prototype.removeChild;
tBody.prototype.appendChildNative = mw0.appendChild;
tBody.prototype.appendChild = mw0.tBody.prototype.appendChild;
tBody.prototype.insertBeforeNative = mw0.insertBefore;
tBody.prototype.insertBefore = mw0.tBody.prototype.insertBefore;
tBody.prototype.removeChildNative = document.removeChild;
tBody.prototype.removeChild = mw0.tBody.prototype.removeChild;
tHead.prototype.appendChildNative = mw0.appendChild;
tHead.prototype.appendChild = mw0.tBody.prototype.appendChild;
tHead.prototype.insertBeforeNative = mw0.insertBefore;
tHead.prototype.insertBefore = mw0.tBody.prototype.insertBefore;
tHead.prototype.removeChildNative = document.removeChild;
tHead.prototype.removeChild = mw0.tBody.prototype.removeChild;
tFoot.prototype.appendChildNative = mw0.appendChild;
tFoot.prototype.appendChild = mw0.tBody.prototype.appendChild;
tFoot.prototype.insertBeforeNative = mw0.insertBefore;
tFoot.prototype.insertBefore = mw0.tBody.prototype.insertBefore;
tFoot.prototype.removeChildNative = document.removeChild;
tFoot.prototype.removeChild = mw0.tBody.prototype.removeChild;
tRow.prototype.appendChildNative = mw0.appendChild;
tRow.prototype.appendChild = mw0.tRow.prototype.appendChild;
tRow.prototype.insertBeforeNative = mw0.insertBefore;
tRow.prototype.insertBefore = mw0.tRow.prototype.insertBefore;
tRow.prototype.removeChildNative = document.removeChild;
tRow.prototype.removeChild = mw0.tRow.prototype.removeChild;

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

XMLHttpRequest = function(){
    this.headers = {};
    this.responseHeaders = {};
    this.aborted = false;//non-standard
    this.withCredentials = true;
};
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPEN = 1;
XMLHttpRequest.HEADERS_RECEIVED = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;
XMLHttpRequest.prototype.open = mw0.XMLHttpRequest.prototype.open;
XMLHttpRequest.prototype.setRequestHeader = mw0.XMLHttpRequest.prototype.setRequestHeader;
XMLHttpRequest.prototype.send = mw0.XMLHttpRequest.prototype.send;
XMLHttpRequest.prototype.parseResponse = mw0.XMLHttpRequest.prototype.parseResponse;
XMLHttpRequest.prototype.abort = mw0.XMLHttpRequest.prototype.abort;
XMLHttpRequest.prototype.onreadystatechange = XMLHttpRequest.prototype.onload = XMLHttpRequest.prototype.onerror = eb$voidfunction;
XMLHttpRequest.prototype.getResponseHeader = mw0.XMLHttpRequest.prototype.getResponseHeader;
XMLHttpRequest.prototype.getAllResponseHeaders = mw0.XMLHttpRequest.prototype.getAllResponseHeaders;
mw0.XMLHttpRequest.prototype.async = false;
mw0.XMLHttpRequest.prototype.readyState = 0;
mw0.XMLHttpRequest.prototype.responseText = "";
mw0.XMLHttpRequest.prototype.response = "";
mw0.XMLHttpRequest.prototype.status = 0;
mw0.XMLHttpRequest.prototype.statusText = "";
XDomainRequest = XMLHttpRequest;

eb$demin = mw0.eb$demin;
eb$watch = mw0.eb$watch;
$uv = [];
$uv$sn = 0;
$jt$c = 'z';
$jt$sn = 0;
eb$uplift = mw0.eb$uplift;

document.querySelectorAll = querySelectorAll;
document.querySelector = querySelector;

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

localStorage = {}
localStorage.attributes = new NamedNodeMap;
localStorage.attributes.owner = localStorage;
localStorage.getAttribute = mw0.getAttribute;
localStorage.getItem = localStorage.getAttribute;
localStorage.setAttribute = mw0.setAttribute;
localStorage.setItem = localStorage.setAttribute;
localStorage.removeAttribute = mw0.removeAttribute;
localStorage.removeItem = localStorage.removeAttribute;
localStorage.clear = function() {
var l;
while(l = localStorage.attributes.length)
localStorage.removeItem(localStorage.attributes[l-1].name);
}

sessionStorage = {}
sessionStorage.attributes = new NamedNodeMap;
sessionStorage.attributes.owner = sessionStorage;
sessionStorage.getAttribute = mw0.getAttribute;
sessionStorage.getItem = sessionStorage.getAttribute;
sessionStorage.setAttribute = mw0.setAttribute;
sessionStorage.setItem = sessionStorage.setAttribute;
sessionStorage.removeAttribute = mw0.removeAttribute;
sessionStorage.removeItem = sessionStorage.removeAttribute;
sessionStorage.clear = function() {
var l;
while(l = sessionStorage.attributes.length)
sessionStorage.removeItem(sessionStorage.attributes[l-1].name);
}

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
window.constructor = Window;
function open() {
return Window.apply(this, arguments);
}

onhashchange = eb$truefunction;

eb$qs$start = mw0.eb$qs$start;
eb$visible = mw0.eb$visible;
css$ver = 0;
DOMParser = mw0.DOMParser;
document.xmlVersion = 0;

XMLSerializer = function(){}
XMLSerializer.prototype.serializeToString = mw0.XMLSerializer.prototype.serializeToString;

throwDebug = false;

NodeFilter = mw0.NodeFilter;
document.createNodeIterator = mw0.createNodeIterator;
document.createTreeWalker = mw0.createTreeWalker;

MutationObserver = function(f) {
var w = my$win();
w.mutList.push(this);
this.callback = (typeof f == "function" ? f : eb$voidfunction);
this.active = false;
this.target = null;
}
MutationObserver.prototype.dom$class = "MutationObserver";
MutationObserver.prototype.disconnect = mw0.MutationObserver.prototype.disconnect;
MutationObserver.prototype.observe = mw0.MutationObserver.prototype.observe;
MutationObserver.prototype.takeRecords = mw0.MutationObserver.prototype.takeRecords;

MutationRecord = function(){};
MutationRecord.prototype.dom$class = "MutationRecord";

mutList = [];

crypto = {};
crypto.getRandomValues = function(a) {
if(!Array.isArray(a)) return;
var l = a.length;
for(var i=0; i<l; ++i) a[i] = Math.floor(Math.random()*0x100000000);
}

rastep = 0;
requestAnimationFrame = mw0.requestAnimationFrame;
cancelAnimationFrame = eb$voidfunction;

