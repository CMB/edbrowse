/*********************************************************************
This file contains support javascript functions used by a browser.
They are easier to write here in javascript, then in C using the js api.
And it is portable amongst all js engines.
This file is converted into a C string and compiled and run
at the start of each javascript window.
Please take advantage of this machinery and put functions here,
including prototypes and getter / setter support functions,
whenever it makes sense to do so.

edbrowse support functions and native methods will always start with eb$,
hoping they will not accidentally collide with js functions in the wild.
Example: eb$newLocation, a native method that redirects this web page to another.

It would be nice to run this file stand-alone, outside of edbrowse,
even if the functionality is limited.
To this end, I create the window object if it isn't already there,
using some clever code I found on the internet.
*********************************************************************/

if(typeof window === "undefined") {
window = (function() { return this; })();
document = new Object;
// Stubs for native methods that are normally provided by edbrowse.
// Example: eb$puts, which we can replace with print,
// which is native to the duktape shell.
eb$puts = print;
eb$logputs = function(a,b) { print(b); }
eb$newLocation = function (s) { print("new location " + s); }
eb$logElement = function(o, tag) { print("pass tag " + tag + " to edbrowse"); }
eb$getcook = function(member) { return "cookies"; }
eb$setcook = function(value, member) { print(" new cookie " + value); }
eb$formSubmit = function() { print("submit"); }
eb$formReset = function() { print("reset"); }
document.eb$apch2 = function(c) { alert("append " + c.nodeName  + " to " + this.nodeName); this.childNodes.push(c); }
document.nodeName = "document";
document.head = {};
document.head.childNodes = [];
document.body = {};
document.body.childNodes = [];
}

// other names for window
self = top = parent = window;
// parent and top could be changed if this is a frame in a larger frameset.

/* An ok (object keys) function for javascript/dom debugging.
 * This is in concert with the jdb command in edbrowse.
 * I know, it doesn't start with eb$ but I wanted an easy,
 * mnemonic command that I could type in quickly.
 * If a web page creates an ok function it will override this one. */
ok = Object.keys = Object.keys || (function () { 
		var hasOwnProperty = Object.prototype.hasOwnProperty, 
		hasDontEnumBug = !{toString:null}.propertyIsEnumerable("toString"),
		DontEnums = [ 
		'toString', 'toLocaleString', 'valueOf', 'hasOwnProperty', 
		'isPrototypeOf', 'propertyIsEnumerable', 'constructor' 
		], 
		DontEnumsLength = DontEnums.length; 
		return function (o) { 
		if (typeof o != "object" && typeof o != "function" || o === null) 
		throw new TypeError("Object.keys called on a non-object");
		var result = []; 
		for (var name in o) { 
		if (hasOwnProperty.call(o, name)) 
		result.push(name); 
		} 
		if (hasDontEnumBug) { 
		for (var i = 0; i < DontEnumsLength; i++) { 
		if (hasOwnProperty.call(o, DontEnums[i]))
		result.push(DontEnums[i]);
		}
		}
		return result; 
		}; 
		})(); 

// Dump the tree below a node, this is for debugging.
document.nodeName = "document"; // in case you want to start at the top.
// Print the first line of text for a text node, and no braces
// because nothing should be below a text node.
// You can make this more elaborate and informative if you wish.
function dumptree(top) {
var nn = top.nodeName.toLowerCase();
var extra = "";
if(nn === "text" && top.data) {
extra = top.data;
extra = extra.replace(/^[ \t\n]*/, "");
var l = extra.indexOf('\n');
if(l >= 0) extra = extra.substr(0,l);
}
if(nn === "option" && top.text)
extra = top.text;
if(nn === "a" && top.href)
extra = top.href.toString();
if(nn === "base" && top.href)
extra = top.href.toString();
if(extra.length) extra = ' ' + extra;
// some tags should never have anything below them so skip the parentheses notation for these.
if((nn == "base" || nn == "meta" || nn == "link" ||nn == "text" || nn == "image" || nn == "option") && top.childNodes.length == 0) {
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

// This is our bailout function, it references a variable that does not exist.
function eb$stopexec() { return javascript$interrupt; }

/* Some visual attributes of the window.
 * These are just guesses.
 * Better to have something than nothing at all. */
height = 768;
width = 1024;
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
document.nodeType = 9;
document.implementation = {};

screen = new Object;
screen.height = 768;
screen.width = 1024;
screen.availHeight = 768;
screen.availWidth = 1024;
screen.availTop = 0;
screen.availLeft = 0;

// window.alert is a simple wrapper around native puts.
function alert(s) { eb$puts(s); }

// The web console, one argument, print based on debugLevel.
// First a helper function, then the console object.
eb$logtime = function(debug, level, obj) {
var today=new Date();
var h=today.getHours();
var m=today.getMinutes();
var s=today.getSeconds();
// add a zero in front of numbers<10
if(h < 10) h = "0" + h;
if(m < 10) m = "0" + m;
if(s < 10) s = "0" + s;
eb$logputs(debug, "console " + level + " [" + h + ":" + m + ":" + s + "] " + obj);
}

console = new Object;
console.log = function(obj) { eb$logtime(3, "log", obj); }
console.info = function(obj) { eb$logtime(3, "info", obj); }
console.warn = function(obj) { eb$logtime(3, "warn", obj); }
console.error = function(obj) { eb$logtime(3, "error", obj); }

eb$nullfunction = function() { return null; }
eb$voidfunction = function() { }
eb$truefunction = function() { return true; }
eb$falsefunction = function() { return false; }

focus = blur = scroll = eb$voidfunction;
document.focus = document.blur = document.open = document.close = eb$voidfunction;

Object.defineProperty(document, "cookie", {
get: eb$getcook, set: eb$setcook});

navigator = new Object;
navigator.appName = "edbrowse";
navigator["appCode Name"] = "edbrowse C/duktape";
/* not sure what product is about */
navigator.product = "duktape";
navigator.productSub = "2.1";
navigator.vendor = "Karl Dahlke";
navigator.javaEnabled = eb$falsefunction;
navigator.taintEnabled = eb$falsefunction;
navigator.cookieEnabled = true;
navigator.onLine = true;
navigator.mimeTypes = [];
navigator.plugins = [];
// the rest of navigator, and of course the plugins,
// must be filled in at run time based on the config file.
// This line lets us run querySelectorAll in stand alone mode,
// it is overwritten at startup by edbrowse.
navigator.userAgent = "edbrowse/3.0.0";

/* There's no history in edbrowse. */
/* Only the current file is known, hence length is 1. */
history = new Object;
history.length = 1;
history.next = "";
history.previous = "";
history.back = eb$voidfunction;
history.forward = eb$voidfunction;
history.go = eb$voidfunction;
history.toString = function() {
 return "Sorry, edbrowse does not maintain a browsing history.";
} 

/* some base arrays - lists of things we'll probably need */
document.heads = new Array;
document.bases = new Array;
document.links = new Array;
document.metas = new Array;
document.styles = new Array;
document.bodies = new Array;
document.forms = new Array;
document.elements = new Array;
document.anchors = new Array;
document.divs = new Array;
document.htmlobjs = new Array;
document.scripts = new Array;
document.paragraphs = new Array;
document.tables = new Array;
document.spans = new Array;
document.images = new Array;
document.areas = new Array;
frames = new Array;

// implementation of getElementsByTagName, getElementsByName, and getElementsByClassName.
// These are recursive as they descend through the tree of nodes.

document.getElementsByTagName = function(s) { 
s = s.toLowerCase();
return document.eb$gebtn(this, s);
}
document.eb$gebtn = function(top, s) { 
var a = new Array;
if(s === '*' || (top.nodeName && top.nodeName.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(document.eb$gebtn(c, s));
}
}
return a;
}

document.getElementsByName = function(s) { 
s = s.toLowerCase();
return document.eb$gebn(this, s);
}
document.eb$gebn = function(top, s) { 
var a = new Array;
if(s === '*' || (top.name && top.name.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(document.eb$gebn(c, s));
}
}
return a;
}

document.getElementsByClassName = function(s) { 
s = s.toLowerCase();
return document.eb$gebcn(this, s);
}
document.eb$gebcn = function(top, s) { 
var a = new Array;
if(s === '*' || (top.className && top.className.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(document.eb$gebcn(c, s));
}
}
return a;
}

document.idMaster = new Object;
document.getElementById = function(s) { 
return document.idMaster[s]; 
}

// originally ms extension pre-DOM, we don't fully support it
//but offer the legacy document.all.tags method.
document.all = new Object;
document.all.tags = function(s) { 
return document.eb$gebtn(document.body, s.toLowerCase());
}

/*********************************************************************
Set and clear attributes. This is done in 3 different ways,
the third using attributes as an array.
This may be overkill - I don't know.
*********************************************************************/

document.getAttribute = function(name) { return this[name.toLowerCase()]; }
document.hasAttribute = function(name) { if (this[name.toLowerCase()]) return true; else return false; }
document.setAttribute = function(name, v) { 
var n = name.toLowerCase();
this[n] = v; 
this.attributes[n] = v;
this.attributes.push(n);
}
document.removeAttribute = function(name) {
    var n = name.toLowerCase();
    if (this[n]) delete this[n];
if(this.attributes[n]) delete this.attributes[n];
    for (var i=this.attributes.length - 1; i >= 0; --i) {
        if (this.attributes[i] == n) {
this.attributes.splice(i, 1);
break;
}
}
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
This is a call to removeChild, which unlinks in js,
and passses the remove side effect back to edbrowse.
The same reasoning holds for insertBefore.
*********************************************************************/

document.appendChild = function(c) {
if(c.parentNode) c.parentNode.removeChild(c);
return this.eb$apch2(c);
}

document.insertBefore = function(c, t) {
if(c.parentNode) c.parentNode.removeChild(c);
return this.eb$insbf(c, t);
}

function eb$getSibling(obj,direction)
{
if (typeof obj.parentNode == 'undefined') {
// need calling node to have parent and it doesn't, error
return null;
}
var pn = obj.parentNode;
var j, l;
l = pn.childNodes.length;
for (j=0; j<l; ++j)
if (pn.childNodes[j] == obj) break;
if (j == l) {
// child not found under parent, error
return null;
}
switch(direction)
{
case "previous":
return (j > 0 ? pn.childNodes[j-1] : null);
break;
case "next":
return (j < l-1 ? pn.childNodes[j+1] : null);
break;
default:
// the function should always have been called with either 'previous' or 'next' specified
return null;
}
}

document.childNodes = new Array;
// We'll make another childNodes array belowe every node in the tree.

document.hasChildNodes = function() { return (this.childNodes.length > 0); }
Object.defineProperty(document, "firstChild", {
get: function() { return document.childNodes[0]; }
});
Object.defineProperty(document, "lastChild", {
get: function() { return document.childNodes[document.childNodes.length-1]; }
});
Object.defineProperty(document, "nextSibling", {
get: function() { return eb$getSibling(this,"next"); }
});
Object.defineProperty(document, "previousSibling", {
get: function() { return eb$getSibling(this,"previous"); }
});
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

// The first DOM class is the easiest: textNode.
// No weird native methods or side effects.
TextNode = function() {
this.data = "";
if(arguments.length > 0) {
// do your best to turn the arg into a string.
this.data += arguments[0];
}
this.nodeName = "text";
this.tagName = "text";
this.nodeValue = this.data;
this.nodeType=3;
this.ownerDocument = document;
this.style = new CSSStyleDeclaration;
this.style.element = this;
this.className = new String;
/* A text node chould never have children, and does not need childNodes array,
 * but there is improper html out there <text> <stuff> </text>
 * which has to put stuff under the text node, so against this
 * unlikely occurence, I have to create the array.
 * I have to treat a text node like an html node. */
this.childNodes = [];
this.attributes = new Array;
}

document.createTextNode = function(t) {
if(t == undefined) t = "";
var c = new TextNode(t);
eb$logElement(c, "text");
return c;
}

/*********************************************************************
Next, the URL class, which is head-spinning in its complexity and its side effects.
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

// No idea why we can't just assign the property directly.
// URL.prototype.protocol = { ... };
Object.defineProperty(URL.prototype, "protocol", {
  get: function() {return this.protocol$val; },
  set: function(v) { this.protocol$val = v; this.rebuild(); }
});

Object.defineProperty(URL.prototype, "pathname", {
  get: function() {return this.pathname$val; },
  set: function(v) { this.pathname$val = v; this.rebuild(); }
});

Object.defineProperty(URL.prototype, "search", {
  get: function() {return this.search$val; },
  set: function(v) { this.search$val = v; this.rebuild(); }
});

Object.defineProperty(URL.prototype, "hash", {
  get: function() {return this.hash$val; },
  set: function(v) { this.hash$val = v; this.rebuild(); }
});

Object.defineProperty(URL.prototype, "port", {
  get: function() {return this.port$val; },
  set: function(v) { this.port$val = v;
if(this.hostname$val.length)
this.host$val = this.hostname$val + ":" + v;
this.rebuild(); }
});

Object.defineProperty(URL.prototype, "hostname", {
  get: function() {return this.hostname$val; },
  set: function(v) { this.hostname$val = v;
if(this.port$val)
this.host$val = v + ":" +  this.port$val;
this.rebuild(); }
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
this.rebuild(); }
});

var eb$defport = {
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
function eb$setDefaultPort(p) {
var port = 0;
p = p.toLowerCase().replace(/:/, "");
if(eb$defport.hasOwnProperty(p))
port = parseInt(eb$defport[p]);
return port;
}

Object.defineProperty(URL.prototype, "href", {
  get: function() {return this.href$val; },
  set: function(v) {
var inconstruct = true;
if(this.href$val) inconstruct = false;
this.href$val = v;
// initialize components to empty,
// then fill them in from href if they are present */
this.protocol$val = "";
this.hostname$val = "";
this.port$val = 0;
this.host$val = "";
this.pathname$val = "";
this.search$val = "";
this.hash$val = "";
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
this.port$val = eb$setDefaultPort(this.protocol$val);
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
if(!inconstruct && (this == window.location || this == document.location)) {
// replace the web page
eb$newLocation('r' + this.href$val + '\n');
}
}
});

URL.prototype.toString = function() { 
return this.href$val;
}

URL.prototype.indexOf = function(s) { 
return this.toString().indexOf(s);
}

URL.prototype.lastIndexOf = function(s) { 
return this.toString().lastIndexOf(s);
}

URL.prototype.substring = function(from, to) { 
return this.toString().substring(from, to);
}

URL.prototype.substr = function(from, to) {
return this.toString().substr(from, to);
}

URL.prototype.toLowerCase = function() { 
return this.toString().toLowerCase();
}

URL.prototype.toUpperCase = function() { 
return this.toString().toUpperCase();
}

URL.prototype.match = function(s) { 
return this.toString().match(s);
}

URL.prototype.replace = function(s, t) { 
return this.toString().replace(s, t);
}

URL.prototype.split = function(s) {
return this.toString().split(s);
}

// On the first call this setter just creates the url, the location of the
// current web page, But on the next call it has the side effect of replacing
// the web page with the new url.
Object.defineProperty(window, "location", {
get: function() { return window.location$2; },
set: function(h) {
if(typeof window.location$2 === "undefined") {
window.location$2 = new URL(h);
} else {
window.location$2.href = h;
}
}});
Object.defineProperty(document, "location", {
get: function() { return document.location$2; },
set: function(h) {
if(typeof document.location$2 === "undefined") {
document.location$2 = new URL(h);
} else {
document.location$2.href = h;
}
}});

// The Attr class and getAttributeNode().
Attr = function(){ this.isId = this.specified = false; this.owner = null; this.name = ""; }

Object.defineProperty(Attr.prototype, "value", {
get: function() { return this.owner.getAttribute(this.name); },
set: function(v) {
this.owner.setAttribute(this.name, v);
this.specified = true;
return;
}});

document.getAttributeNode = function(s) {
var n = new Attr;
n.owner = this;
n.name = s;
if(this.getAttribute(s) != undefined)
n.specified = true;
return n;
}

// The Option class, these are choices in a dropdown list.
Option = function() {
this.nodeName = "option";
this.text = this.value = "";
if(arguments.length > 0)
this.text = arguments[0];
if(arguments.length > 1)
this.value = arguments[1];
this.selected = false;
this.defaultSelected = false;
}

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

// Document class, I don't know what to make of this,
// but my stubs for frames needs it.
Document = function(){}

CSSStyleDeclaration = function(){
        this.element = null;
        this.style = this;
this.attributes = new Array;
};

CSSStyleDeclaration.prototype.getPropertyValue = function(p) {
                if (this[p] == undefined)                
                        this[p] = "";
                        return this[p];
}

// pages seem to want document.style to exist
document.style = new CSSStyleDeclaration();
document.style.bgcolor = "white";

getComputedStyle = function(e,pe) {
	// disregarding pseudoelements for now
s = new CSSStyleDeclaration;
s.element = e;
// This is a rather inefficient use of cssApply, but it is hardly ever called.
cssApply(e, s);
return s;
}

document.defaultView = function() { return window; }

document.defaultView.getComputedStyle = getComputedStyle;

// @author Originally implemented by Yehuda Katz
// And since then, from envjs, by Thatcher et al

XMLHttpRequest = function(){
    this.headers = {};
    this.responseHeaders = {};
    this.aborted = false;//non-standard
};

// defined by the standard: http://www.w3.org/TR/XMLHttpRequest/#xmlhttprequest
// but not provided by Firefox.  Safari and others do define it.
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPEN = 1;
XMLHttpRequest.HEADERS_RECEIVED = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;

XMLHttpRequest.prototype = {
open: function(method, url, async, user, password){
this.readyState = 1;
this.async = false;
// Note: async is currently hardcoded to false
// In the implementation in envjs, the line here was:
// this.async = (async === false)?false:true;

this.method = method || "GET";
this.url = eb$resolveURL(url);
this.status = 0;
this.statusText = "";
this.onreadystatechange();
},
setRequestHeader: function(header, value){
this.headers[header] = value;
},
send: function(data, parsedoc/*non-standard*/){
var headerstring = "";
for (var item in this.headers) {
var v1=item;
var v2=this.headers[item];
headerstring+=v1;
headerstring+=": ";
headerstring+=v2;
headerstring+=",";
}

var entire_http_response =  eb$fetchHTTP(this.url,this.method,headerstring,data);

var responsebody_array = entire_http_response.split("\r\n\r\n");
var http_headers = responsebody_array[0];
responsebody_array[0] = "";
var responsebody = responsebody_array.join("\r\n\r\n");
responsebody = responsebody.trim();

this.responseText = responsebody;
var hhc = http_headers.split("\r\n");
var i=0;
while (i < hhc.length) {
var value1 = hhc[i]+":";
var value2 = value1.split(":")[0];
var value3 = value1.split(":")[1];
this.responseHeaders[value2] = value3.trim();
i++;
}

try{
this.readyState = 4;
}catch(e){
}


if ((!this.aborted) && this.responseText.length > 0){
this.readyState = 4;
this.status = 200;
this.statusText = "OK";
this.onreadystatechange();
}

},
abort: function(){
this.aborted = true;
},
onreadystatechange: function(){
//Instance specific
},
getResponseHeader: function(header){
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
},
getAllResponseHeaders: function(){
var header, returnedHeaders = [];
if (this.readyState < 3){
throw new Error("INVALID_STATE_ERR");
} else {
for (header in this.responseHeaders) {
returnedHeaders.push( header + ": " + this.responseHeaders[header] );
}
}
return returnedHeaders.join("\r\n");
},
async: false,
readyState: 0,
responseText: "",
status: 0,
statusText: ""
};

// Here are the DOM classes with generic constructors.
Head = function(){}
Meta = function(){}
Link = function(){}
Body = function(){}
// Some screen attributes that are suppose to be there.
Body.prototype. clientHeight = 768;
Body.prototype. clientWidth = 1024;
Body.prototype. offsetHeight = 768;
Body.prototype. offsetWidth = 1024;
Body.prototype. scrollHeight = 768;
Body.prototype. scrollWidth = 1024;
Body.prototype. scrollTop = 0;
Body.prototype. scrollLeft = 0;
Base = function(){}
Form = function(){}
Form.prototype.submit = eb$formSubmit;
Form.prototype.reset = eb$formReset;
Element = function(){}
Image = function(){}
Frame = function(){}
Anchor = function(){}
Lister = function(){}
Listitem = function(){}
Tbody = function(){}
Table = function(){}
Div = function(){}
HtmlObj = function(){}
Area = function(){}
Span = function(){}
Trow = function(){}
Cell = function(){}
P = function(){}
Script = function(){}
Timer = function(){}

/*********************************************************************
This creates a copy of the node and its children recursively.
The argument 'deep' refers to whether or not the clone will recurs.
eb$clone is a helper function that is not tied to any particular prototype.
*********************************************************************/

document.cloneNode = function(deep) {
return eb$clone (this,deep);
}

function eb$clone(nodeToCopy,deep)
{
var nodeToReturn;
var i;

// special case for array, which is the select node.
if(nodeToCopy instanceof Array) {
nodeToReturn = new Array;
for(i = 0; i < nodeToCopy.length; ++i)
nodeToReturn.push(eb$clone(nodeToCopy[i]));
} else {

nodeToReturn = document.createElement(nodeToCopy.nodeName);
if (deep && nodeToCopy.childNodes) {
for(i = 0; i < nodeToCopy.childNodes.length; ++i) {
var current_item = nodeToCopy.childNodes[i];
nodeToReturn.appendChild(eb$clone(current_item,true));
}
}
}

// now for the strings.
for (var item in nodeToCopy) {
if (typeof nodeToCopy[item] == 'string') {
// don't copy strings that are really setters; we'll be copying inner$html
// as a true string so won't need to copy innerHTML, and shouldn't.
if(item == "innerHTML")
continue;
if(item == "innerText")
continue;
if(item == "value" &&
!(nodeToCopy instanceof Array) && !(nodeToCopy instanceof Option))
continue;
nodeToReturn[item] = nodeToCopy[item];
}
}

// copy style object if present and its subordinate strings.
if (typeof nodeToCopy.style == "object") {
nodeToReturn.style = new CSSStyleDeclaration();
nodeToReturn.style.element = nodeToReturn;
for (var item in nodeToCopy.style){
if (typeof nodeToCopy.style[item] == 'string' ||
typeof nodeToCopy.style[item] == 'number')
nodeToReturn.style[item] = nodeToCopy.style[item];
}
}

// copy any objects of class URL.
for (var url in nodeToCopy) {
var u = nodeToCopy[url];
if(typeof u == "object" && u instanceof URL)
nodeToReturn[url] = new URL(u.href);
}

return nodeToReturn;
}

document.createElement = function(s) { 
var c;
var t = s.toLowerCase();
switch(t) { 
case "body":
c = new Body();
break;
case "a":
c = new Anchor();
break;
case "image":
t = "img";
case "img":
c = new Image();
break;
case "cssstyledeclaration":
case "style":
c = new CSSStyleDeclaration;
break;
case "script":
c = new Script();
break;
case "div":
c = new Div();
break;
case "p":
c = new P();
break;
case "table":
c = new Table();
break;
case "tbody":
c = new Tbody();
break;
case "tr":
c = new Trow();
break;
case "td":
c = new Cell();
break;
case "select":
/* select and radio are special form elements in that they are intrinsically
 * arrays, with all the options as array elements,
 * and also "options" or "childNodes" linked to itself
 * so it looks like it has children in the usual way. */
c = new Array;
c.nodeName = t;
c.tagName = t;
c.options = c;
c.childNodes = c;
c.selectedIndex = -1;
c.value = "";
// no style, and childNodes already self-linked, so just return.
eb$logElement(c, t);
return c;
case "option":
c = new Option();
c.nodeName = t;
c.tagName = t;
// we don't log options because rebuildSelectors() checks
// the dropdown lists after every js run.
return c;
default:
/* eb$puts("createElement default " + s); */
c = new Element();
}

/* ok, for some element types this perhaps doesn't make sense,
* but for most visible ones it does and I doubt it matters much */
if(c instanceof CSSStyleDeclaration) {
c.element = c;
} else {
c.style = new CSSStyleDeclaration;
c.style.element = c;
}
c.childNodes = new Array;
c.attributes = new Array;
c.nodeName = t;
c.tagName = t;
c.nodeType = 1;
c.nodeValue = undefined;
c.class = new String;
c.className = new String;
c.ownerDocument = document;
eb$logElement(c, t);
return c;
} 

document.createDocumentFragment = function() {
var c = document.createElement("fragment");
c.nodeType = 11;
return c;
}

document.createComment = function() {
var c = document.createElement("comment");
c.nodeType = 8;
return c;
}

/*********************************************************************
This is our addEventListener function.
It is bound to window, which is ok because window has such a function
to listen to load and unload.
Later on we will bind it to document and to other elements via
element.addEventListener = addEventListener
Or maybe URL.prototype.addEventListener = addEventListener
to cover all the instantiated objects in one go.
first arg is a string like click, second arg is a js handler,
Third arg is not used cause I don't understand it.
*********************************************************************/

function addEventListener(ev, handler, notused)
{
ev = "on" + ev;
var evarray = ev + "$$array"; // array of handlers
var evorig = ev + "$$orig"; // original handler from html
if(!this[evarray]) {
/* attaching the first handler */
var a = new Array;
/* was there already a function from before? */
if(this[ev]) {
this[evorig] = this[ev];
this[ev] = undefined;
}
this[evarray] = a;
eval(
'this.' + ev + ' = function(){ var a = this.' + evarray + '; if(this.' + evorig + ') this.' + evorig + '(); for(var i = 0; i<a.length; ++i) {a[i]();} };');
}
this[evarray].push(handler);
}

// here is remove, the opposite of add.
// what if every handler is removed and there is an empty array?
// the assumption is that this is not a problem
function removeEventListener(ev, handler, notused)
{
ev = "on" + ev;
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
a.splice(i, 1);
return;
}
}
}

// For grins let's put in the other standard.
function attachEvent(ev, handler)
{
var evarray = ev + "$$array"; // array of handlers
var evorig = ev + "$$orig"; // original handler from html
if(!this[evarray]) {
/* attaching the first handler */
var a = new Array;
/* was there already a function from before? */
if(this[ev]) {
this[evorig] = this[ev];
this[ev] = undefined;
}
this[evarray] = a;
eval(
'this.' + ev + ' = function(){ var a = this.' + evarray + '; if(this.' + evorig + ') this.' + evorig + '(); for(var i = 0; i<a.length; ++i) a[i](); };');
}
this[evarray].push(handler);
}

document.addEventListener = window.addEventListener;
document.attachEvent = window.attachEvent;
document.removeEventListener = window.removeEventListener;

(function() {
for(var cn in {Body, Form, Element, Anchor}) {
var c = window[cn];
// c is class and cn is classname.
c.prototype.addEventListener = window.addEventListener;
c.prototype.removeEventListener = window.removeEventListener;
c.prototype.attachEvent = window.attachEvent;
}
})();

// Add prototype methods to the standard nodes, nodes that have children,
// and the normal set of methods to go with those children.
// Form has children for sure, but if we add <input> to Form,
// we also have to add it to the array Form.elements.
// So there are some nodes that we have to do outside this loop.
for(var cn in {HtmlObj, Head, Body, CSSStyleDeclaration, Frame,
Anchor, Element, Lister, Listitem, Tbody, Table, Div,
Span, Trow, Cell, P, Script,
// The following nodes shouldn't have any children, but the various
// children methods could be called on them anyways.
Area, TextNode, Image, Option, Link, Meta}) {
var c = window[cn];
// c is class and cn is classname.
// get elements below
c.prototype.getElementsByTagName = document.getElementsByTagName;
c.prototype.getElementsByName = document.getElementsByName;
c.prototype.getElementsByClassName = document.getElementsByClassName;
// children
c.prototype.hasChildNodes = document.hasChildNodes;
c.prototype.appendChild = document.appendChild;
c.prototype.eb$apch1 = document.eb$apch1;
c.prototype.eb$apch2 = document.eb$apch2;
c.prototype.insertBefore = document.insertBefore;
c.prototype.eb$insbf = document.eb$insbf;
c.prototype.removeChild = document.removeChild;
c.prototype.replaceChild = document.replaceChild;
Object.defineProperty(c.prototype, "firstChild", { get: function() { return this.childNodes[0]; } });
Object.defineProperty(c.prototype, "lastChild", { get: function() { return this.childNodes[this.childNodes.length-1]; } });
Object.defineProperty(c.prototype, "nextSibling", { get: function() { return eb$getSibling(this,"next"); } });
Object.defineProperty(c.prototype, "previousSibling", { get: function() { return eb$getSibling(this,"previous"); } });
// attributes
c.prototype.hasAttribute = document.hasAttribute;
c.prototype.getAttribute = document.getAttribute;
c.prototype.setAttribute = document.setAttribute;
c.prototype.removeAttribute = document.removeAttribute;
c.prototype.getAttributeNode = document.getAttributeNode;
// clone
c.prototype.cloneNode = document.cloneNode;
// visual
c.prototype.focus = focus;
c.prototype.blur = blur;
}

/*********************************************************************
As promised, Form is weird.
If you add an input to a form, it adds under childNodes in the usual way,
but also must add in the elements[] array.
Same for insertBefore and removeChild.
When adding an input element to a form,
linnk form[element.name] to that element.
*********************************************************************/

Form.prototype.getElementsByTagName = document.getElementsByTagName;
Form.prototype.getElementsByName = document.getElementsByName;
Form.prototype.getElementsByClassName = document.getElementsByClassName;

function eb$formname(parent, child)
{
var s;
if(typeof child.name == "string")
s = child.name;
else if(typeof child.id == "string")
s = child.id;
else return;
// Is it ok if name is "action"? I'll assume it is,
// but then there is no way to submit the form. Oh well.
parent[s] = child;
}

Form.prototype.appendChildNative = document.appendChild;
Form.prototype.appendChild = function(newobj) {
this.appendChildNative(newobj);
if(newobj.nodeName === "input" || newobj.nodeName === "select") {
this.elements.appendChild(newobj);
eb$formname(this, newobj);
}
}
Form.prototype.eb$apch1 = document.eb$apch1;
Form.prototype.eb$apch2 = document.eb$apch2;
Form.prototype.eb$insbf = document.eb$insbf;
Form.prototype.insertBeforeNative = document.insertBefore;
Form.prototype.insertBefore = function(newobj, item) {
this.insertBeforeNative(newobj, item);
if(newobj.nodeName === "input" || newobj.nodeName === "select") {
// the following won't work unless item is also type input.
this.elements.insertBefore(newobj, item);
eb$formname(this, newobj);
}
}
Form.prototype.hasChildNodes = document.hasChildNodes;
Form.prototype.removeChildNative = document.removeChild;
Form.prototype.removeChild = function(item) {
this.removeChildNative(item);
if(item.nodeName === "input" || item.nodeName === "select")
this.elements.removeChild(item);
}
Form.prototype.replaceChild = document.replaceChild;
Object.defineProperty(Form.prototype, "firstChild", { get: function() { return this.childNodes[0]; } });
Object.defineProperty(Form.prototype, "lastChild", { get: function() { return this.childNodes[this.childNodes.length-1]; } });
Object.defineProperty(Form.prototype, "nextSibling", { get: function() { return eb$getSibling(this,"next"); } });
Object.defineProperty(Form.prototype, "previousSibling", { get: function() { return eb$getSibling(this,"previous"); } });

Form.prototype.getAttribute = document.getAttribute;
Form.prototype.setAttribute = document.setAttribute;
Form.prototype.hasAttribute = document.hasAttribute;
Form.prototype.removeAttribute = document.removeAttribute;
Form.prototype.getAttributeNode = document.getAttributeNode;

Form.prototype.cloneNode = document.cloneNode;

/* The select element in a form is itself an array, so the children functions have
 * to be on array prototype, except appendchild is to have no side effects,
 * because select options are maintained by rebuildSelectors(), so appendChild
 * is just array.push(). */
Array.prototype.appendChild = function(child) {
// check to see if it's already there
for(var i=0; i<this.length; ++i)
if(this[i] == child)
return child;
this.push(child);return child; }
/* insertBefore maps to splice, but we have to find the element. */
/* This prototype assumes all elements are objects. */
Array.prototype.insertBefore = function(newobj, item) {
// check to see if it's already there
for(var i=0; i<this.length; ++i)
if(this[i] == newobj)
return newobj;
for(var i=0; i<this.length; ++i)
if(this[i] == item) {
this.splice(i, 0, newobj);
return newobj;
}
}
Array.prototype.removeChild = function(item) {
for(var i=0; i<this.length; ++i)
if(this[i] == item) {
this.splice(i, 1);
return;
}
}
Array.prototype.hasChildNodes = document.hasChildNodes;
Array.prototype.replaceChild = document.replaceChild;
Object.defineProperty(Array.prototype, "firstChild", { get: function() { return this[0]; } });
Object.defineProperty(Array.prototype, "lastChild", { get: function() { return this[this.length-1]; } });
Object.defineProperty(Array.prototype, "nextSibling", { get: function() { return eb$getSibling(this,"next"); } });
Object.defineProperty(Array.prototype, "previousSibling", { get: function() { return eb$getSibling(this,"previous"); } });
Array.prototype.getAttribute = document.getAttribute;
Array.prototype.setAttribute = document.setAttribute;
Array.prototype.hasAttribute = document.hasAttribute;
Array.prototype.removeAttribute = document.removeAttribute;
Array.prototype.getAttributeNode = document.getAttributeNode;

/*********************************************************************
The rest of this file contains open source software form third parties.
These functions are useful to edbrowse, particularly in the area
of parsing css and applying css attributes to the corresponding objects.
This is almost pointless in a browser that does not draw the screen, except,
some websites test for the presence of these values, and so they must be there.
This software is copyright under the MIT license, as shown below.

The MIT License (MIT)
Copyright (c) 2015 JotForm
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Parse a css string into an array of selecters and their attributes.
https://github.com/jotform/css.js.git
Snapshot taken on 08/20/2017.
https://raw.githubusercontent.com/jotform/css.js/master/css.js
Minimized code is available, but I thought it more confusing than helpful.
*********************************************************************/

/* jshint unused:false */
/* global base64_decode, CSSWizardView, window, console, jQuery */
(function(global) {
  'use strict';
  var fi = function() {

    this.cssImportStatements = [];
    this.cssKeyframeStatements = [];

    this.cssRegex = new RegExp('([\\s\\S]*?){([\\s\\S]*?)}', 'gi');
    this.cssMediaQueryRegex = '((@media [\\s\\S]*?){([\\s\\S]*?}\\s*?)})';
    this.cssKeyframeRegex = '((@.*?keyframes [\\s\\S]*?){([\\s\\S]*?}\\s*?)})';
    this.combinedCSSRegex = '((\\s*?(?:\\/\\*[\\s\\S]*?\\*\\/)?\\s*?@media[\\s\\S]*?){([\\s\\S]*?)}\\s*?})|(([\\s\\S]*?){([\\s\\S]*?)})'; //to match css & media queries together
    this.cssCommentsRegex = '(\\/\\*[\\s\\S]*?\\*\\/)';
    this.cssImportStatementRegex = new RegExp('@import .*?;', 'gi');
  };

  /*
    Strip outs css comments and returns cleaned css string

    @param css, the original css string to be stipped out of comments

    @return cleanedCSS contains no css comments
  */
  fi.prototype.stripComments = function(cssString) {
    var regex = new RegExp(this.cssCommentsRegex, 'gi');

    return cssString.replace(regex, '');
  };

  /*
    Parses given css string, and returns css object
    keys as selectors and values are css rules
    eliminates all css comments before parsing

    @param source css string to be parsed

    @return object css
  */
  fi.prototype.parseCSS = function(source) {

    if (source === undefined) {
      return [];
    }

    var css = [];
    //strip out comments
    //source = this.stripComments(source);

    //get import statements

    while (true) {
      var imports = this.cssImportStatementRegex.exec(source);
      if (imports !== null) {
        this.cssImportStatements.push(imports[0]);
        css.push({
          selector: '@imports',
          type: 'imports',
          styles: imports[0]
        });
      } else {
        break;
      }
    }
    source = source.replace(this.cssImportStatementRegex, '');
    //get keyframe statements
    var keyframesRegex = new RegExp(this.cssKeyframeRegex, 'gi');
    var arr;
    while (true) {
      arr = keyframesRegex.exec(source);
      if (arr === null) {
        break;
      }
      css.push({
        selector: '@keyframes',
        type: 'keyframes',
        styles: arr[0]
      });
    }
    source = source.replace(keyframesRegex, '');

    //unified regex
    var unified = new RegExp(this.combinedCSSRegex, 'gi');

    while (true) {
      arr = unified.exec(source);
      if (arr === null) {
        break;
      }
      var selector = '';
      if (arr[2] === undefined) {
        selector = arr[5].split('\r\n').join('\n').trim();
      } else {
        selector = arr[2].split('\r\n').join('\n').trim();
      }

      /*
        fetch comments and associate it with current selector
      */
      var commentsRegex = new RegExp(this.cssCommentsRegex, 'gi');
      var comments = commentsRegex.exec(selector);
      if (comments !== null) {
        selector = selector.replace(commentsRegex, '').trim();
      }

      // Never have more than a single line break in a row
      selector = selector.replace(/\n+/, "\n");

      //determine the type
      if (selector.indexOf('@media') !== -1) {
        //we have a media query
        var cssObject = {
          selector: selector,
          type: 'media',
          subStyles: this.parseCSS(arr[3] + '\n}') //recursively parse media query inner css
        };
        if (comments !== null) {
          cssObject.comments = comments[0];
        }
        css.push(cssObject);
      } else {
        //we have standard css
        var rules = this.parseRules(arr[6]);
        var style = {
          selector: selector,
          rules: rules
        };
        if (selector === '@font-face') {
          style.type = 'font-face';
        }
        if (comments !== null) {
          style.comments = comments[0];
        }
        css.push(style);
      }
    }

    return css;
  };

  /*
    parses given string containing css directives
    and returns an array of objects containing ruleName:ruleValue pairs

    @param rules, css directive string example
        \n\ncolor:white;\n    font-size:18px;\n
  */
  fi.prototype.parseRules = function(rules) {
    //convert all windows style line endings to unix style line endings
    rules = rules.split('\r\n').join('\n');
    var ret = [];

    rules = rules.split(';');

    //proccess rules line by line
    for (var i = 0; i < rules.length; i++) {
      var line = rules[i];

      //determine if line is a valid css directive, ie color:white;
      line = line.trim();
      if (line.indexOf(':') !== -1) {
        //line contains :
        line = line.split(':');
        var cssDirective = line[0].trim();
        var cssValue = line.slice(1).join(':').trim();

        //more checks
        if (cssDirective.length < 1 || cssValue.length < 1) {
          continue; //there is no css directive or value that is of length 1 or 0
          // PLAIN WRONG WHAT ABOUT margin:0; ?
        }

        //push rule
        ret.push({
          directive: cssDirective,
          value: cssValue
        });
      } else {
        //if there is no ':', but what if it was mis splitted value which starts with base64
        if (line.trim().substr(0, 7) === 'base64,') { //hack :)
          ret[ret.length - 1].value += line.trim();
        } else {
          //add rule, even if it is defective
          if (line.length > 0) {
            ret.push({
              directive: '',
              value: line,
              defective: true
            });
          }
        }
      }
    }

    return ret; //we are done!
  };
  /*
    just returns the rule having given directive
    if not found returns false;
  */
  fi.prototype.findCorrespondingRule = function(rules, directive, value) {
    if (value === undefined) {
      value = false;
    }
    var ret = false;
    for (var i = 0; i < rules.length; i++) {
      if (rules[i].directive === directive) {
        ret = rules[i];
        if (value === rules[i].value) {
          break;
        }
      }
    }
    return ret;
  };

  /*
      Finds styles that have given selector, compress them,
      and returns them
  */
  fi.prototype.findBySelector = function(cssObjectArray, selector, contains) {
    if (contains === undefined) {
      contains = false;
    }

    var found = [];
    for (var i = 0; i < cssObjectArray.length; i++) {
      if (contains === false) {
        if (cssObjectArray[i].selector === selector) {
          found.push(cssObjectArray[i]);
        }
      } else {
        if (cssObjectArray[i].selector.indexOf(selector) !== -1) {
          found.push(cssObjectArray[i]);
        }
      }

    }
    if (selector === '@imports' || found.length < 2) {
      return found;
    } else {
      var base = found[0];
      for (i = 1; i < found.length; i++) {
        this.intelligentCSSPush([base], found[i]);
      }
      return [base]; //we are done!! all properties merged into base!
    }
  };

  /*
    deletes cssObjects having given selector, and returns new array
  */
  fi.prototype.deleteBySelector = function(cssObjectArray, selector) {
    var ret = [];
    for (var i = 0; i < cssObjectArray.length; i++) {
      if (cssObjectArray[i].selector !== selector) {
        ret.push(cssObjectArray[i]);
      }
    }
    return ret;
  };

  /*
      Compresses given cssObjectArray and tries to minimize
      selector redundence.
  */
  fi.prototype.compressCSS = function(cssObjectArray) {
    var compressed = [];
    var done = {};
    for (var i = 0; i < cssObjectArray.length; i++) {
      var obj = cssObjectArray[i];
      if (done[obj.selector] === true) {
        continue;
      }

      var found = this.findBySelector(cssObjectArray, obj.selector); //found compressed
      if (found.length !== 0) {
        compressed = compressed.concat(found);
        done[obj.selector] = true;
      }
    }
    return compressed;
  };

  /*
    Received 2 css objects with following structure
      {
        rules : [{directive:"", value:""}, {directive:"", value:""}, ...]
        selector : "SOMESELECTOR"
      }

    returns the changed(new,removed,updated) values on css1 parameter, on same structure

    if two css objects are the same, then returns false

      if a css directive exists in css1 and     css2, and its value is different, it is included in diff
      if a css directive exists in css1 and not css2, it is then included in diff
      if a css directive exists in css2 but not css1, then it is deleted in css1, it would be included in diff but will be marked as type='DELETED'

      @object css1 css object
      @object css2 css object

      @return diff css object contains changed values in css1 in regards to css2 see test input output in /test/data/css.js
  */
  fi.prototype.cssDiff = function(css1, css2) {
    if (css1.selector !== css2.selector) {
      return false;
    }

    //if one of them is media query return false, because diff function can not operate on media queries
    if ((css1.type === 'media' || css2.type === 'media')) {
      return false;
    }

    var diff = {
      selector: css1.selector,
      rules: []
    };
    var rule1, rule2;
    for (var i = 0; i < css1.rules.length; i++) {
      rule1 = css1.rules[i];
      //find rule2 which has the same directive as rule1
      rule2 = this.findCorrespondingRule(css2.rules, rule1.directive, rule1.value);
      if (rule2 === false) {
        //rule1 is a new rule in css1
        diff.rules.push(rule1);
      } else {
        //rule2 was found only push if its value is different too
        if (rule1.value !== rule2.value) {
          diff.rules.push(rule1);
        }
      }
    }

    //now for rules exists in css2 but not in css1, which means deleted rules
    for (var ii = 0; ii < css2.rules.length; ii++) {
      rule2 = css2.rules[ii];
      //find rule2 which has the same directive as rule1
      rule1 = this.findCorrespondingRule(css1.rules, rule2.directive);
      if (rule1 === false) {
        //rule1 is a new rule
        rule2.type = 'DELETED'; //mark it as a deleted rule, so that other merge operations could be true
        diff.rules.push(rule2);
      }
    }


    if (diff.rules.length === 0) {
      return false;
    }
    return diff;
  };

  /*
      Merges 2 different css objects together
      using intelligentCSSPush,

      @param cssObjectArray, target css object array
      @param newArray, source array that will be pushed into cssObjectArray parameter
      @param reverse, [optional], if given true, first parameter will be traversed on reversed order
              effectively giving priority to the styles in newArray
  */
  fi.prototype.intelligentMerge = function(cssObjectArray, newArray, reverse) {
    if (reverse === undefined) {
      reverse = false;
    }


    for (var i = 0; i < newArray.length; i++) {
      this.intelligentCSSPush(cssObjectArray, newArray[i], reverse);
    }
    for (i = 0; i < cssObjectArray.length; i++) {
      var cobj = cssObjectArray[i];
      if (cobj.type === 'media' || (cobj.type === 'keyframes')) {
        continue;
      }
      cobj.rules = this.compactRules(cobj.rules);
    }
  };

  /*
    inserts new css objects into a bigger css object
    with same selectors grouped together

    @param cssObjectArray, array of bigger css object to be pushed into
    @param minimalObject, single css object
    @param reverse [optional] default is false, if given, cssObjectArray will be reversly traversed
            resulting more priority in minimalObject's styles
  */
  fi.prototype.intelligentCSSPush = function(cssObjectArray, minimalObject, reverse) {
    var pushSelector = minimalObject.selector;
    //find correct selector if not found just push minimalObject into cssObject
    var cssObject = false;

    if (reverse === undefined) {
      reverse = false;
    }

    if (reverse === false) {
      for (var i = 0; i < cssObjectArray.length; i++) {
        if (cssObjectArray[i].selector === minimalObject.selector) {
          cssObject = cssObjectArray[i];
          break;
        }
      }
    } else {
      for (var j = cssObjectArray.length - 1; j > -1; j--) {
        if (cssObjectArray[j].selector === minimalObject.selector) {
          cssObject = cssObjectArray[j];
          break;
        }
      }
    }

    if (cssObject === false) {
      cssObjectArray.push(minimalObject); //just push, because cssSelector is new
    } else {
      if (minimalObject.type !== 'media') {
        for (var ii = 0; ii < minimalObject.rules.length; ii++) {
          var rule = minimalObject.rules[ii];
          //find rule inside cssObject
          var oldRule = this.findCorrespondingRule(cssObject.rules, rule.directive);
          if (oldRule === false) {
            cssObject.rules.push(rule);
          } else if (rule.type === 'DELETED') {
            oldRule.type = 'DELETED';
          } else {
            //rule found just update value

            oldRule.value = rule.value;
          }
        }
      } else {
        cssObject.subStyles = cssObject.subStyles.concat(minimalObject.subStyles); //TODO, make this intelligent too
      }

    }
  };

  /*
    filter outs rule objects whose type param equal to DELETED

    @param rules, array of rules

    @returns rules array, compacted by deleting all unnecessary rules
  */
  fi.prototype.compactRules = function(rules) {
    var newRules = [];
    for (var i = 0; i < rules.length; i++) {
      if (rules[i].type !== 'DELETED') {
        newRules.push(rules[i]);
      }
    }
    return newRules;
  };
  /*
    computes string for ace editor using this.css or given cssBase optional parameter

    @param [optional] cssBase, if given computes cssString from cssObject array
  */
  fi.prototype.getCSSForEditor = function(cssBase, depth) {
    if (depth === undefined) {
      depth = 0;
    }
    var ret = '';
    if (cssBase === undefined) {
      cssBase = this.css;
    }
    //append imports
    for (var i = 0; i < cssBase.length; i++) {
      if (cssBase[i].type === 'imports') {
        ret += cssBase[i].styles + '\n\n';
      }
    }
    for (i = 0; i < cssBase.length; i++) {
      var tmp = cssBase[i];
      if (tmp.selector === undefined) { //temporarily omit media queries
        continue;
      }
      var comments = "";
      if (tmp.comments !== undefined) {
        comments = tmp.comments + '\n';
      }

      if (tmp.type === 'media') { //also put media queries to output
        ret += comments + tmp.selector + '{\n';
        ret += this.getCSSForEditor(tmp.subStyles, depth + 1);
        ret += '}\n\n';
      } else if (tmp.type !== 'keyframes' && tmp.type !== 'imports') {
        ret += this.getSpaces(depth) + comments + tmp.selector + ' {\n';
        ret += this.getCSSOfRules(tmp.rules, depth + 1);
        ret += this.getSpaces(depth) + '}\n\n';
      }
    }

    //append keyFrames
    for (i = 0; i < cssBase.length; i++) {
      if (cssBase[i].type === 'keyframes') {
        ret += cssBase[i].styles + '\n\n';
      }
    }

    return ret;
  };

  fi.prototype.getImports = function(cssObjectArray) {
    var imps = [];
    for (var i = 0; i < cssObjectArray.length; i++) {
      if (cssObjectArray[i].type === 'imports') {
        imps.push(cssObjectArray[i].styles);
      }
    }
    return imps;
  };
  /*
    given rules array, returns visually formatted css string
    to be used inside editor
  */
  fi.prototype.getCSSOfRules = function(rules, depth) {
    var ret = '';
    for (var i = 0; i < rules.length; i++) {
      if (rules[i] === undefined) {
        continue;
      }
      if (rules[i].defective === undefined) {
        ret += this.getSpaces(depth) + rules[i].directive + ': ' + rules[i].value + ';\n';
      } else {
        ret += this.getSpaces(depth) + rules[i].value + ';\n';
      }

    }
    return ret || '\n';
  };

  /*
      A very simple helper function returns number of spaces appended in a single string,
      the number depends input parameter, namely input*2
  */
  fi.prototype.getSpaces = function(num) {
    var ret = '';
    for (var i = 0; i < num * 4; i++) {
      ret += ' ';
    }
    return ret;
  };

  /*
    Given css string or objectArray, parses it and then for every selector,
    prepends this.cssPreviewNamespace to prevent css collision issues

    @returns css string in which this.cssPreviewNamespace prepended
  */
  fi.prototype.applyNamespacing = function(css, forcedNamespace) {
    var cssObjectArray = css;
    var namespaceClass = '.' + this.cssPreviewNamespace;
    if(forcedNamespace !== undefined){
      namespaceClass = forcedNamespace;
    }

    if (typeof css === 'string') {
      cssObjectArray = this.parseCSS(css);
    }

    for (var i = 0; i < cssObjectArray.length; i++) {
      var obj = cssObjectArray[i];

      //bypass namespacing for @font-face @keyframes @import
      if(obj.selector.indexOf('@font-face') > -1 || obj.selector.indexOf('keyframes') > -1 || obj.selector.indexOf('@import') > -1 || obj.selector.indexOf('.form-all') > -1 || obj.selector.indexOf('#stage') > -1){
        continue;
      }

      if (obj.type !== 'media') {
        var selector = obj.selector.split(',');
        var newSelector = [];
        for (var j = 0; j < selector.length; j++) {
          if (selector[j].indexOf('.supernova') === -1) { //do not apply namespacing to selectors including supernova
            newSelector.push(namespaceClass + ' ' + selector[j]);
          } else {
            newSelector.push(selector[j]);
          }
        }
        obj.selector = newSelector.join(',');
      } else {
        obj.subStyles = this.applyNamespacing(obj.subStyles, forcedNamespace); //handle media queries as well
      }
    }

    return cssObjectArray;
  };

  /*
    given css string or object array, clears possible namespacing from
    all of the selectors inside the css
  */
  fi.prototype.clearNamespacing = function(css, returnObj) {
    if (returnObj === undefined) {
      returnObj = false;
    }
    var cssObjectArray = css;
    var namespaceClass = '.' + this.cssPreviewNamespace;
    if (typeof css === 'string') {
      cssObjectArray = this.parseCSS(css);
    }

    for (var i = 0; i < cssObjectArray.length; i++) {
      var obj = cssObjectArray[i];

      if (obj.type !== 'media') {
        var selector = obj.selector.split(',');
        var newSelector = [];
        for (var j = 0; j < selector.length; j++) {
          newSelector.push(selector[j].split(namespaceClass + ' ').join(''));
        }
        obj.selector = newSelector.join(',');
      } else {
        obj.subStyles = this.clearNamespacing(obj.subStyles, true); //handle media queries as well
      }
    }
    if (returnObj === false) {
      return this.getCSSForEditor(cssObjectArray);
    } else {
      return cssObjectArray;
    }

  };

  /*
    creates a new style tag (also destroys the previous one)
    and injects given css string into that css tag
  */
  fi.prototype.createStyleElement = function(id, css, format) {
    if (format === undefined) {
      format = false;
    }

    if (this.testMode === false && format !== 'nonamespace') {
      //apply namespacing classes
      css = this.applyNamespacing(css);
    }

    if (typeof css !== 'string') {
      css = this.getCSSForEditor(css);
    }
    //apply formatting for css
    if (format === true) {
      css = this.getCSSForEditor(this.parseCSS(css));
    }

    if (this.testMode !== false) {
      return this.testMode('create style #' + id, css); //if test mode, just pass result to callback
    }

    var __el = document.getElementById(id);
    if (__el) {
      __el.parentNode.removeChild(__el);
    }

    var head = document.head || document.getElementsByTagName('head')[0],
      style = document.createElement('style');

    style.id = id;
    style.type = 'text/css';

    head.appendChild(style);

    if (style.styleSheet && !style.sheet) {
      style.styleSheet.cssText = css;
    } else {
      style.appendChild(document.createTextNode(css));
    }
  };

  global.cssjs = fi;

})(this);

/*********************************************************************
A querySelectorAll function to turn a css styole selector into an array of nodes.
https://github.com/yiminghe/query-selector.git
Snapshot taken on 08/20/2017.  query-selector-standalone-debug.js
Minimized code is available, but I thought it more confusing than helpful.
*********************************************************************/

var querySelectorAll = null;

// Can't run this until the dom framework is in place; it creates new div tags etc.
function eb$qs$start()
{
if(querySelectorAll) return false;

/*
Copyright 2014, query-selector@1.0.6
MIT Licensed
build time: Thu, 16 Oct 2014 03:51:57 GMT
*/
querySelectorAll = (function(){ var module = {};

var _querySelector_;
_querySelector_ = function (exports) {
  /*
  combined modules:
  query-selector
  query-selector/util
  query-selector/parser
  */
  var querySelectorUtil, querySelectorParser, querySelector;
  querySelectorUtil = function (exports) {
    /**
     * attr fix for old ie
     * @author yiminghe@gmail.com
     */
    var R_BOOLEAN = /^(?:autofocus|autoplay|async|checked|controls|defer|disabled|hidden|loop|multiple|open|readonly|required|scoped|selected)$/i, R_FOCUSABLE = /^(?:button|input|object|select|textarea)$/i, R_CLICKABLE = /^a(?:rea)?$/i, R_INVALID_CHAR = /:|^on/;
    var attrFix = {}, propFix, attrHooks = {
        // http://fluidproject.org/blog/2008/01/09/getting-setting-and-removing-tabindex-values-with-javascript/
        tabindex: {
          get: function (el) {
            // elem.tabIndex doesn't always return the correct value when it hasn't been explicitly set
            var attributeNode = el.getAttributeNode('tabindex');
            return attributeNode && attributeNode.specified ? parseInt(attributeNode.value, 10) : R_FOCUSABLE.test(el.nodeName) || R_CLICKABLE.test(el.nodeName) && el.href ? 0 : undefined;
          }
        }
      }, boolHook = {
        get: function (elem, name) {
          // 转发到 prop 方法
          return elem[propFix[name] || name] ? // 根据 w3c attribute , true 时返回属性名字符串
          name.toLowerCase() : undefined;
        }
      }, attrNodeHook = {};
    attrHooks.style = {
      get: function (el) {
        return el.style.cssText;
      }
    };
    propFix = {
      hidefocus: 'hideFocus',
      tabindex: 'tabIndex',
      readonly: 'readOnly',
      'for': 'htmlFor',
      'class': 'className',
      maxlength: 'maxLength',
      cellspacing: 'cellSpacing',
      cellpadding: 'cellPadding',
      rowspan: 'rowSpan',
      colspan: 'colSpan',
      usemap: 'useMap',
      frameborder: 'frameBorder',
      contenteditable: 'contentEditable'
    };
    var ua = typeof navigator !== 'undefined' ? navigator.userAgent : '';
    var doc = typeof document !== 'undefined' ? document : {};
    function numberify(s) {
      var c = 0;
      // convert '1.2.3.4' to 1.234
      return parseFloat(s.replace(/\./g, function () {
        return c++ === 0 ? '.' : '';
      }));
    }
    function ieVersion() {
      var m, v;
      if ((m = ua.match(/MSIE ([^;]*)|Trident.*; rv(?:\s|:)?([0-9.]+)/)) && (v = m[1] || m[2])) {
        return doc.documentMode || numberify(v);
      }
    }
    function mix(s, t) {
      for (var p in t) {
        s[p] = t[p];
      }
    }
    function each(arr, fn) {
      var i = 0, l = arr.length;
      for (; i < l; i++) {
        if (fn(arr[i], i) === false) {
          break;
        }
      }
    }
    var ie = ieVersion();
    if (ie && ie < 8) {
      attrHooks.style.set = function (el, val) {
        el.style.cssText = val;
      };
      // get attribute value from attribute node for ie
      mix(attrNodeHook, {
        get: function (elem, name) {
          var ret = elem.getAttributeNode(name);
          // Return undefined if attribute node specified by user
          return ret && (ret.specified || ret.nodeValue) ? ret.nodeValue : undefined;
        }
      });
      // ie6,7 不区分 attribute 与 property
      mix(attrFix, propFix);
      // http://fluidproject.org/blog/2008/01/09/getting-setting-and-removing-tabindex-values-with-javascript/
      attrHooks.tabIndex = attrHooks.tabindex;
      // 不光是 href, src, 还有 rowspan 等非 mapping 属性，也需要用第 2 个参数来获取原始值
      // 注意 colSpan rowSpan 已经由 propFix 转为大写
      each([
        'href',
        'src',
        'width',
        'height',
        'colSpan',
        'rowSpan'
      ], function (name) {
        attrHooks[name] = {
          get: function (elem) {
            var ret = elem.getAttribute(name, 2);
            return ret === null ? undefined : ret;
          }
        };
      });
      attrHooks.placeholder = {
        get: function (elem, name) {
          return elem[name] || attrNodeHook.get(elem, name);
        }
      };
    }
    if (ie) {
      var hrefFix = attrHooks.href = attrHooks.href || {};
      hrefFix.set = function (el, val, name) {
        var childNodes = el.childNodes, b, len = childNodes.length, allText = len > 0;
        for (len = len - 1; len >= 0; len--) {
          if (childNodes[len].nodeType !== 3) {
            allText = 0;
          }
        }
        if (allText) {
          b = el.ownerDocument.createElement('b');
          b.style.display = 'none';
          el.appendChild(b);
        }
        el.setAttribute(name, '' + val);
        if (b) {
          el.removeChild(b);
        }
      };
    }
    var RE_TRIM = /^[\s\xa0]+|[\s\xa0]+$/g, trim = String.prototype.trim;
    var SPACE = ' ';
    var getElementsByTagName;
    getElementsByTagName = function (name, context) {
      return context.getElementsByTagName(name);
    };
    if (doc.createElement) {
      var div = doc.createElement('div');
      div.appendChild(document.createComment(''));
      if (div.getElementsByTagName('*').length) {
        getElementsByTagName = function (name, context) {
          var nodes = context.getElementsByTagName(name), needsFilter = name === '*';
          // <input id='length'>
          if (needsFilter || typeof nodes.length !== 'number') {
            var ret = [], i = 0, el;
            while (el = nodes[i++]) {
              if (!needsFilter || el.nodeType === 1) {
                ret.push(el);
              }
            }
            return ret;
          } else {
            return nodes;
          }
        };
      }
    }
    var compareNodeOrder = 'sourceIndex' in (doc && doc.documentElement || {}) ? function (a, b) {
      return a.sourceIndex - b.sourceIndex;
    } : function (a, b) {
      if (!a.compareDocumentPosition || !b.compareDocumentPosition) {
        return a.compareDocumentPosition ? -1 : 1;
      }
      var bit = a.compareDocumentPosition(b) & 4;
      return bit ? -1 : 1;
    };
    var util = exports = {
      ie: ie,
      unique: function () {
        var hasDuplicate, baseHasDuplicate = true;
        [
          0,
          0
        ].sort(function () {
          baseHasDuplicate = false;
          return 0;
        });
        function sortOrder(a, b) {
          if (a === b) {
            hasDuplicate = true;
            return 0;
          }
          return compareNodeOrder(a, b);
        }
        return function (elements) {
          hasDuplicate = baseHasDuplicate;
          elements.sort(sortOrder);
          if (hasDuplicate) {
            var i = 1, len = elements.length;
            while (i < len) {
              if (elements[i] === elements[i - 1]) {
                elements.splice(i, 1);
                --len;
              } else {
                i++;
              }
            }
          }
          return elements;
        };
      }(),
      getElementsByTagName: getElementsByTagName,
      getSimpleAttr: function (el, name) {
//  alert(el ? el.nodeName : "empty");
        var ret = el && el.getAttributeNode(name);
        if (ret && ret.specified) {
          return 'value' in ret ? ret.value : ret.nodeValue;
        }
        return undefined;
      },
      contains: ie ? function (a, b) {
        if (a.nodeType === 9) {
          a = a.documentElement;
        }
        b = b.parentNode;
        if (a === b) {
          return true;
        }
        if (b && b.nodeType === 1) {
          return a.contains && a.contains(b);
        } else {
          return false;
        }
      } : function (a, b) {
        return !!(a.compareDocumentPosition(b) & 16);
      },
      isTag: function (el, value) {
        return value === '*' || el.nodeName.toLowerCase() === value.toLowerCase();
      },
      hasSingleClass: function (el, cls) {
        var className = el && util.getSimpleAttr(el, 'class');
        return className && (className = className.replace(/[\r\t\n]/g, SPACE)) && (SPACE + className + SPACE).indexOf(SPACE + cls + SPACE) > -1;
      },
      startsWith: function (str, prefix) {
        return str.lastIndexOf(prefix, 0) === 0;
      },
      endsWith: function (str, suffix) {
        var ind = str.length - suffix.length;
        return ind >= 0 && str.indexOf(suffix, ind) === ind;
      },
      trim: trim ? function (str) {
        return str == null ? '' : trim.call(str);
      } : function (str) {
        return str == null ? '' : (str + '').replace(RE_TRIM, '');
      },
      attr: function (el, name) {
        var attrNormalizer, ret;
        name = name.toLowerCase();
        name = attrFix[name] || name;
        if (R_BOOLEAN.test(name)) {
          attrNormalizer = boolHook;
        } else if (R_INVALID_CHAR.test(name)) {
          attrNormalizer = attrNodeHook;
        } else {
          attrNormalizer = attrHooks[name];
        }
        if (el && el.nodeType === 1) {
          if (el.nodeName.toLowerCase() === 'form') {
            attrNormalizer = attrNodeHook;
          }
          if (attrNormalizer && attrNormalizer.get) {
            return attrNormalizer.get(el, name);
          }
          ret = el.getAttribute(name);
          if (ret === '') {
            var attrNode = el.getAttributeNode(name);
            if (!attrNode || !attrNode.specified) {
              return undefined;
            }
          }
          return ret === null ? undefined : ret;
        }
      }
    };
    return exports;
  }();
  querySelectorParser = function (exports) {
    var parser = function (undefined) {
      var parser = {}, GrammarConst = {
          'SHIFT_TYPE': 1,
          'REDUCE_TYPE': 2,
          'ACCEPT_TYPE': 0,
          'TYPE_INDEX': 0,
          'PRODUCTION_INDEX': 1,
          'TO_INDEX': 2
        };
      function mix(to, from) {
        for (var f in from) {
          to[f] = from[f];
        }
      }
      function isArray(obj) {
        return '[object Array]' === Object.prototype.toString.call(obj);
      }
      function each(object, fn, context) {
        if (object) {
          var key, val, length, i = 0;
          context = context || null;
          if (!isArray(object)) {
            for (key in object) {
              if (fn.call(context, object[key], key, object) === false) {
                break;
              }
            }
          } else {
            length = object.length;
            for (val = object[0]; i < length; val = object[++i]) {
              if (fn.call(context, val, i, object) === false) {
                break;
              }
            }
          }
        }
      }
      function inArray(item, arr) {
        for (var i = 0, l = arr.length; i < l; i++) {
          if (arr[i] === item) {
            return true;
          }
        }
        return false;
      }
      var Lexer = function Lexer(cfg) {
        var self = this;
        self.rules = [];
        mix(self, cfg);
        self.resetInput(self.input);
      };
      Lexer.prototype = {
        'resetInput': function (input) {
          mix(this, {
            input: input,
            matched: '',
            stateStack: [Lexer.STATIC.INITIAL],
            match: '',
            text: '',
            firstLine: 1,
            lineNumber: 1,
            lastLine: 1,
            firstColumn: 1,
            lastColumn: 1
          });
        },
        'getCurrentRules': function () {
          var self = this, currentState = self.stateStack[self.stateStack.length - 1], rules = [];
          if (self.mapState) {
            currentState = self.mapState(currentState);
          }
          each(self.rules, function (r) {
            var state = r.state || r[3];
            if (!state) {
              if (currentState === Lexer.STATIC.INITIAL) {
                rules.push(r);
              }
            } else if (inArray(currentState, state)) {
              rules.push(r);
            }
          });
          return rules;
        },
        'pushState': function (state) {
          this.stateStack.push(state);
        },
        'popState': function (num) {
          num = num || 1;
          var ret;
          while (num--) {
            ret = this.stateStack.pop();
          }
          return ret;
        },
        'showDebugInfo': function () {
          var self = this, DEBUG_CONTEXT_LIMIT = Lexer.STATIC.DEBUG_CONTEXT_LIMIT, matched = self.matched, match = self.match, input = self.input;
          matched = matched.slice(0, matched.length - match.length);
          var past = (matched.length > DEBUG_CONTEXT_LIMIT ? '...' : '') + matched.slice(0 - DEBUG_CONTEXT_LIMIT).replace(/\n/, ' '), next = match + input;
          next = next.slice(0, DEBUG_CONTEXT_LIMIT) + (next.length > DEBUG_CONTEXT_LIMIT ? '...' : '');
          return past + next + '\n' + new Array(past.length + 1).join('-') + '^';
        },
        'mapSymbol': function mapSymbolForCodeGen(t) {
          return this.symbolMap[t];
        },
        'mapReverseSymbol': function (rs) {
          var self = this, symbolMap = self.symbolMap, i, reverseSymbolMap = self.reverseSymbolMap;
          if (!reverseSymbolMap && symbolMap) {
            reverseSymbolMap = self.reverseSymbolMap = {};
            for (i in symbolMap) {
              reverseSymbolMap[symbolMap[i]] = i;
            }
          }
          if (reverseSymbolMap) {
            return reverseSymbolMap[rs];
          } else {
            return rs;
          }
        },
        'lex': function () {
          var self = this, input = self.input, i, rule, m, ret, lines, rules = self.getCurrentRules();
          self.match = self.text = '';
          if (!input) {
            return self.mapSymbol(Lexer.STATIC.END_TAG);
          }
          for (i = 0; i < rules.length; i++) {
            rule = rules[i];
            var regexp = rule.regexp || rule[1], token = rule.token || rule[0], action = rule.action || rule[2] || undefined;
            if (m = input.match(regexp)) {
              lines = m[0].match(/\n.*/g);
              if (lines) {
                self.lineNumber += lines.length;
              }
              mix(self, {
                firstLine: self.lastLine,
                lastLine: self.lineNumber + 1,
                firstColumn: self.lastColumn,
                lastColumn: lines ? lines[lines.length - 1].length - 1 : self.lastColumn + m[0].length
              });
              var match;
              match = self.match = m[0];
              self.matches = m;
              self.text = match;
              self.matched += match;
              ret = action && action.call(self);
              if (ret === undefined) {
                ret = token;
              } else {
                ret = self.mapSymbol(ret);
              }
              input = input.slice(match.length);
              self.input = input;
              if (ret) {
                return ret;
              } else {
                return self.lex();
              }
            }
          }
        }
      };
      Lexer.STATIC = {
        'INITIAL': 'I',
        'DEBUG_CONTEXT_LIMIT': 20,
        'END_TAG': '$EOF'
      };
      var lexer = new Lexer({
        'rules': [
          [
            'b',
            /^\[(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'c',
            /^(?:[\t\r\n\f\x20]*)\]/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'd',
            /^(?:[\t\r\n\f\x20]*)~=(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'e',
            /^(?:[\t\r\n\f\x20]*)\|=(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'f',
            /^(?:[\t\r\n\f\x20]*)\^=(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'g',
            /^(?:[\t\r\n\f\x20]*)\$=(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'h',
            /^(?:[\t\r\n\f\x20]*)\*=(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'i',
            /^(?:[\t\r\n\f\x20]*)\=(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'j',
            /^(?:(?:[\w]|[^\x00-\xa0]|(?:\\[^\n\r\f0-9a-f]))(?:[\w\d-]|[^\x00-\xa0]|(?:\\[^\n\r\f0-9a-f]))*)\(/,
            function () {
              this.text = this.yy.trim(this.text).slice(0, -1);
              this.pushState('fn');
            }
          ],
          [
            'k',
            /^[^\)]*/,
            function () {
              this.popState();
            },
            ['fn']
          ],
          [
            'l',
            /^(?:[\t\r\n\f\x20]*)\)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'm',
            /^:not\((?:[\t\r\n\f\x20]*)/i,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'n',
            /^(?:(?:[\w]|[^\x00-\xa0]|(?:\\[^\n\r\f0-9a-f]))(?:[\w\d-]|[^\x00-\xa0]|(?:\\[^\n\r\f0-9a-f]))*)/,
            function () {
              this.text = this.yy.unEscape(this.text);
            }
          ],
          [
            'o',
            /^"(\\"|[^"])*"/,
            function () {
              this.text = this.yy.unEscapeStr(this.text);
            }
          ],
          [
            'o',
            /^'(\\'|[^'])*'/,
            function () {
              this.text = this.yy.unEscapeStr(this.text);
            }
          ],
          [
            'p',
            /^#(?:(?:[\w\d-]|[^\x00-\xa0]|(?:\\[^\n\r\f0-9a-f]))+)/,
            function () {
              this.text = this.yy.unEscape(this.text.slice(1));
            }
          ],
          [
            'q',
            /^\.(?:(?:[\w]|[^\x00-\xa0]|(?:\\[^\n\r\f0-9a-f]))(?:[\w\d-]|[^\x00-\xa0]|(?:\\[^\n\r\f0-9a-f]))*)/,
            function () {
              this.text = this.yy.unEscape(this.text.slice(1));
            }
          ],
          [
            'r',
            /^(?:[\t\r\n\f\x20]*),(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            's',
            /^::?/,
            0
          ],
          [
            't',
            /^(?:[\t\r\n\f\x20]*)\+(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'u',
            /^(?:[\t\r\n\f\x20]*)>(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'v',
            /^(?:[\t\r\n\f\x20]*)~(?:[\t\r\n\f\x20]*)/,
            function () {
              this.text = this.yy.trim(this.text);
            }
          ],
          [
            'w',
            /^\*/,
            0
          ],
          [
            'x',
            /^(?:[\t\r\n\f\x20]+)/,
            0
          ],
          [
            'y',
            /^./,
            0
          ]
        ]
      });
      parser.lexer = lexer;
      lexer.symbolMap = {
        '$EOF': 'a',
        'LEFT_BRACKET': 'b',
        'RIGHT_BRACKET': 'c',
        'INCLUDES': 'd',
        'DASH_MATCH': 'e',
        'PREFIX_MATCH': 'f',
        'SUFFIX_MATCH': 'g',
        'SUBSTRING_MATCH': 'h',
        'ALL_MATCH': 'i',
        'FUNCTION': 'j',
        'PARAMETER': 'k',
        'RIGHT_PARENTHESES': 'l',
        'NOT': 'm',
        'IDENT': 'n',
        'STRING': 'o',
        'HASH': 'p',
        'CLASS': 'q',
        'COMMA': 'r',
        'COLON': 's',
        'PLUS': 't',
        'GREATER': 'u',
        'TILDE': 'v',
        'UNIVERSAL': 'w',
        'S': 'x',
        'INVALID': 'y',
        '$START': 'z',
        'selectors_group': 'aa',
        'selector': 'ab',
        'simple_selector_sequence': 'ac',
        'combinator': 'ad',
        'type_selector': 'ae',
        'id_selector': 'af',
        'class_selector': 'ag',
        'attrib_match': 'ah',
        'attrib': 'ai',
        'attrib_val': 'aj',
        'pseudo': 'ak',
        'negation': 'al',
        'negation_arg': 'am',
        'suffix_selector': 'an',
        'suffix_selectors': 'ao'
      };
      parser.productions = [
        [
          'z',
          ['aa']
        ],
        [
          'aa',
          ['ab'],
          function () {
            return [this.$1];
          }
        ],
        [
          'aa',
          [
            'aa',
            'r',
            'ab'
          ],
          function () {
            this.$1.push(this.$3);
          }
        ],
        [
          'ab',
          ['ac']
        ],
        [
          'ab',
          [
            'ab',
            'ad',
            'ac'
          ],
          function () {
            this.$1.nextCombinator = this.$3.prevCombinator = this.$2;
            var order;
            order = this.$1.order = this.$1.order || 0;
            this.$3.order = order + 1;
            this.$3.prev = this.$1;
            this.$1.next = this.$3;
            return this.$3;
          }
        ],
        [
          'ad',
          ['t']
        ],
        [
          'ad',
          ['u']
        ],
        [
          'ad',
          ['v']
        ],
        [
          'ad',
          ['x'],
          function () {
            return ' ';
          }
        ],
        [
          'ae',
          ['n'],
          function () {
            return {
              t: 'tag',
              value: this.$1
            };
          }
        ],
        [
          'ae',
          ['w'],
          function () {
            return {
              t: 'tag',
              value: this.$1
            };
          }
        ],
        [
          'af',
          ['p'],
          function () {
            return {
              t: 'id',
              value: this.$1
            };
          }
        ],
        [
          'ag',
          ['q'],
          function () {
            return {
              t: 'cls',
              value: this.$1
            };
          }
        ],
        [
          'ah',
          ['f']
        ],
        [
          'ah',
          ['g']
        ],
        [
          'ah',
          ['h']
        ],
        [
          'ah',
          ['i']
        ],
        [
          'ah',
          ['d']
        ],
        [
          'ah',
          ['e']
        ],
        [
          'ai',
          [
            'b',
            'n',
            'c'
          ],
          function () {
            return {
              t: 'attrib',
              value: { ident: this.$2 }
            };
          }
        ],
        [
          'aj',
          ['n']
        ],
        [
          'aj',
          ['o']
        ],
        [
          'ai',
          [
            'b',
            'n',
            'ah',
            'aj',
            'c'
          ],
          function () {
            return {
              t: 'attrib',
              value: {
                ident: this.$2,
                match: this.$3,
                value: this.$4
              }
            };
          }
        ],
        [
          'ak',
          [
            's',
            'j',
            'k',
            'l'
          ],
          function () {
            return {
              t: 'pseudo',
              value: {
                fn: this.$2.toLowerCase(),
                param: this.$3
              }
            };
          }
        ],
        [
          'ak',
          [
            's',
            'n'
          ],
          function () {
            return {
              t: 'pseudo',
              value: { ident: this.$2.toLowerCase() }
            };
          }
        ],
        [
          'al',
          [
            'm',
            'am',
            'l'
          ],
          function () {
            return {
              t: 'pseudo',
              value: {
                fn: 'not',
                param: this.$2
              }
            };
          }
        ],
        [
          'am',
          ['ae']
        ],
        [
          'am',
          ['af']
        ],
        [
          'am',
          ['ag']
        ],
        [
          'am',
          ['ai']
        ],
        [
          'am',
          ['ak']
        ],
        [
          'an',
          ['af']
        ],
        [
          'an',
          ['ag']
        ],
        [
          'an',
          ['ai']
        ],
        [
          'an',
          ['ak']
        ],
        [
          'an',
          ['al']
        ],
        [
          'ao',
          ['an'],
          function () {
            return [this.$1];
          }
        ],
        [
          'ao',
          [
            'ao',
            'an'
          ],
          function () {
            this.$1.push(this.$2);
          }
        ],
        [
          'ac',
          ['ae']
        ],
        [
          'ac',
          ['ao'],
          function () {
            return { suffix: this.$1 };
          }
        ],
        [
          'ac',
          [
            'ae',
            'ao'
          ],
          function () {
            return {
              t: 'tag',
              value: this.$1.value,
              suffix: this.$2
            };
          }
        ]
      ];
      parser.table = {
        'gotos': {
          '0': {
            'aa': 8,
            'ab': 9,
            'ae': 10,
            'af': 11,
            'ag': 12,
            'ai': 13,
            'ak': 14,
            'al': 15,
            'an': 16,
            'ao': 17,
            'ac': 18
          },
          '2': {
            'ae': 20,
            'af': 21,
            'ag': 22,
            'ai': 23,
            'ak': 24,
            'am': 25
          },
          '9': { 'ad': 33 },
          '10': {
            'af': 11,
            'ag': 12,
            'ai': 13,
            'ak': 14,
            'al': 15,
            'an': 16,
            'ao': 34
          },
          '17': {
            'af': 11,
            'ag': 12,
            'ai': 13,
            'ak': 14,
            'al': 15,
            'an': 35
          },
          '19': { 'ah': 43 },
          '28': {
            'ab': 46,
            'ae': 10,
            'af': 11,
            'ag': 12,
            'ai': 13,
            'ak': 14,
            'al': 15,
            'an': 16,
            'ao': 17,
            'ac': 18
          },
          '33': {
            'ae': 10,
            'af': 11,
            'ag': 12,
            'ai': 13,
            'ak': 14,
            'al': 15,
            'an': 16,
            'ao': 17,
            'ac': 47
          },
          '34': {
            'af': 11,
            'ag': 12,
            'ai': 13,
            'ak': 14,
            'al': 15,
            'an': 35
          },
          '43': { 'aj': 50 },
          '46': { 'ad': 33 }
        },
        'action': {
          '0': {
            'b': [
              1,
              undefined,
              1
            ],
            'm': [
              1,
              undefined,
              2
            ],
            'n': [
              1,
              undefined,
              3
            ],
            'p': [
              1,
              undefined,
              4
            ],
            'q': [
              1,
              undefined,
              5
            ],
            's': [
              1,
              undefined,
              6
            ],
            'w': [
              1,
              undefined,
              7
            ]
          },
          '1': {
            'n': [
              1,
              undefined,
              19
            ]
          },
          '2': {
            'b': [
              1,
              undefined,
              1
            ],
            'n': [
              1,
              undefined,
              3
            ],
            'p': [
              1,
              undefined,
              4
            ],
            'q': [
              1,
              undefined,
              5
            ],
            's': [
              1,
              undefined,
              6
            ],
            'w': [
              1,
              undefined,
              7
            ]
          },
          '3': {
            'a': [
              2,
              9
            ],
            'r': [
              2,
              9
            ],
            't': [
              2,
              9
            ],
            'u': [
              2,
              9
            ],
            'v': [
              2,
              9
            ],
            'x': [
              2,
              9
            ],
            'p': [
              2,
              9
            ],
            'q': [
              2,
              9
            ],
            'b': [
              2,
              9
            ],
            's': [
              2,
              9
            ],
            'm': [
              2,
              9
            ],
            'l': [
              2,
              9
            ]
          },
          '4': {
            'a': [
              2,
              11
            ],
            'r': [
              2,
              11
            ],
            't': [
              2,
              11
            ],
            'u': [
              2,
              11
            ],
            'v': [
              2,
              11
            ],
            'x': [
              2,
              11
            ],
            'p': [
              2,
              11
            ],
            'q': [
              2,
              11
            ],
            'b': [
              2,
              11
            ],
            's': [
              2,
              11
            ],
            'm': [
              2,
              11
            ],
            'l': [
              2,
              11
            ]
          },
          '5': {
            'a': [
              2,
              12
            ],
            'r': [
              2,
              12
            ],
            't': [
              2,
              12
            ],
            'u': [
              2,
              12
            ],
            'v': [
              2,
              12
            ],
            'x': [
              2,
              12
            ],
            'p': [
              2,
              12
            ],
            'q': [
              2,
              12
            ],
            'b': [
              2,
              12
            ],
            's': [
              2,
              12
            ],
            'm': [
              2,
              12
            ],
            'l': [
              2,
              12
            ]
          },
          '6': {
            'j': [
              1,
              undefined,
              26
            ],
            'n': [
              1,
              undefined,
              27
            ]
          },
          '7': {
            'a': [
              2,
              10
            ],
            'r': [
              2,
              10
            ],
            't': [
              2,
              10
            ],
            'u': [
              2,
              10
            ],
            'v': [
              2,
              10
            ],
            'x': [
              2,
              10
            ],
            'p': [
              2,
              10
            ],
            'q': [
              2,
              10
            ],
            'b': [
              2,
              10
            ],
            's': [
              2,
              10
            ],
            'm': [
              2,
              10
            ],
            'l': [
              2,
              10
            ]
          },
          '8': {
            'a': [0],
            'r': [
              1,
              undefined,
              28
            ]
          },
          '9': {
            'a': [
              2,
              1
            ],
            'r': [
              2,
              1
            ],
            't': [
              1,
              undefined,
              29
            ],
            'u': [
              1,
              undefined,
              30
            ],
            'v': [
              1,
              undefined,
              31
            ],
            'x': [
              1,
              undefined,
              32
            ]
          },
          '10': {
            'a': [
              2,
              38
            ],
            'r': [
              2,
              38
            ],
            't': [
              2,
              38
            ],
            'u': [
              2,
              38
            ],
            'v': [
              2,
              38
            ],
            'x': [
              2,
              38
            ],
            'b': [
              1,
              undefined,
              1
            ],
            'm': [
              1,
              undefined,
              2
            ],
            'p': [
              1,
              undefined,
              4
            ],
            'q': [
              1,
              undefined,
              5
            ],
            's': [
              1,
              undefined,
              6
            ]
          },
          '11': {
            'a': [
              2,
              31
            ],
            'r': [
              2,
              31
            ],
            't': [
              2,
              31
            ],
            'u': [
              2,
              31
            ],
            'v': [
              2,
              31
            ],
            'x': [
              2,
              31
            ],
            'p': [
              2,
              31
            ],
            'q': [
              2,
              31
            ],
            'b': [
              2,
              31
            ],
            's': [
              2,
              31
            ],
            'm': [
              2,
              31
            ]
          },
          '12': {
            'a': [
              2,
              32
            ],
            'r': [
              2,
              32
            ],
            't': [
              2,
              32
            ],
            'u': [
              2,
              32
            ],
            'v': [
              2,
              32
            ],
            'x': [
              2,
              32
            ],
            'p': [
              2,
              32
            ],
            'q': [
              2,
              32
            ],
            'b': [
              2,
              32
            ],
            's': [
              2,
              32
            ],
            'm': [
              2,
              32
            ]
          },
          '13': {
            'a': [
              2,
              33
            ],
            'r': [
              2,
              33
            ],
            't': [
              2,
              33
            ],
            'u': [
              2,
              33
            ],
            'v': [
              2,
              33
            ],
            'x': [
              2,
              33
            ],
            'p': [
              2,
              33
            ],
            'q': [
              2,
              33
            ],
            'b': [
              2,
              33
            ],
            's': [
              2,
              33
            ],
            'm': [
              2,
              33
            ]
          },
          '14': {
            'a': [
              2,
              34
            ],
            'r': [
              2,
              34
            ],
            't': [
              2,
              34
            ],
            'u': [
              2,
              34
            ],
            'v': [
              2,
              34
            ],
            'x': [
              2,
              34
            ],
            'p': [
              2,
              34
            ],
            'q': [
              2,
              34
            ],
            'b': [
              2,
              34
            ],
            's': [
              2,
              34
            ],
            'm': [
              2,
              34
            ]
          },
          '15': {
            'a': [
              2,
              35
            ],
            'r': [
              2,
              35
            ],
            't': [
              2,
              35
            ],
            'u': [
              2,
              35
            ],
            'v': [
              2,
              35
            ],
            'x': [
              2,
              35
            ],
            'p': [
              2,
              35
            ],
            'q': [
              2,
              35
            ],
            'b': [
              2,
              35
            ],
            's': [
              2,
              35
            ],
            'm': [
              2,
              35
            ]
          },
          '16': {
            'a': [
              2,
              36
            ],
            'r': [
              2,
              36
            ],
            't': [
              2,
              36
            ],
            'u': [
              2,
              36
            ],
            'v': [
              2,
              36
            ],
            'x': [
              2,
              36
            ],
            'p': [
              2,
              36
            ],
            'q': [
              2,
              36
            ],
            'b': [
              2,
              36
            ],
            's': [
              2,
              36
            ],
            'm': [
              2,
              36
            ]
          },
          '17': {
            'a': [
              2,
              39
            ],
            'r': [
              2,
              39
            ],
            't': [
              2,
              39
            ],
            'u': [
              2,
              39
            ],
            'v': [
              2,
              39
            ],
            'x': [
              2,
              39
            ],
            'b': [
              1,
              undefined,
              1
            ],
            'm': [
              1,
              undefined,
              2
            ],
            'p': [
              1,
              undefined,
              4
            ],
            'q': [
              1,
              undefined,
              5
            ],
            's': [
              1,
              undefined,
              6
            ]
          },
          '18': {
            'a': [
              2,
              3
            ],
            'r': [
              2,
              3
            ],
            't': [
              2,
              3
            ],
            'u': [
              2,
              3
            ],
            'v': [
              2,
              3
            ],
            'x': [
              2,
              3
            ]
          },
          '19': {
            'c': [
              1,
              undefined,
              36
            ],
            'd': [
              1,
              undefined,
              37
            ],
            'e': [
              1,
              undefined,
              38
            ],
            'f': [
              1,
              undefined,
              39
            ],
            'g': [
              1,
              undefined,
              40
            ],
            'h': [
              1,
              undefined,
              41
            ],
            'i': [
              1,
              undefined,
              42
            ]
          },
          '20': {
            'l': [
              2,
              26
            ]
          },
          '21': {
            'l': [
              2,
              27
            ]
          },
          '22': {
            'l': [
              2,
              28
            ]
          },
          '23': {
            'l': [
              2,
              29
            ]
          },
          '24': {
            'l': [
              2,
              30
            ]
          },
          '25': {
            'l': [
              1,
              undefined,
              44
            ]
          },
          '26': {
            'k': [
              1,
              undefined,
              45
            ]
          },
          '27': {
            'a': [
              2,
              24
            ],
            'r': [
              2,
              24
            ],
            't': [
              2,
              24
            ],
            'u': [
              2,
              24
            ],
            'v': [
              2,
              24
            ],
            'x': [
              2,
              24
            ],
            'p': [
              2,
              24
            ],
            'q': [
              2,
              24
            ],
            'b': [
              2,
              24
            ],
            's': [
              2,
              24
            ],
            'm': [
              2,
              24
            ],
            'l': [
              2,
              24
            ]
          },
          '28': {
            'b': [
              1,
              undefined,
              1
            ],
            'm': [
              1,
              undefined,
              2
            ],
            'n': [
              1,
              undefined,
              3
            ],
            'p': [
              1,
              undefined,
              4
            ],
            'q': [
              1,
              undefined,
              5
            ],
            's': [
              1,
              undefined,
              6
            ],
            'w': [
              1,
              undefined,
              7
            ]
          },
          '29': {
            'n': [
              2,
              5
            ],
            'w': [
              2,
              5
            ],
            'p': [
              2,
              5
            ],
            'q': [
              2,
              5
            ],
            'b': [
              2,
              5
            ],
            's': [
              2,
              5
            ],
            'm': [
              2,
              5
            ]
          },
          '30': {
            'n': [
              2,
              6
            ],
            'w': [
              2,
              6
            ],
            'p': [
              2,
              6
            ],
            'q': [
              2,
              6
            ],
            'b': [
              2,
              6
            ],
            's': [
              2,
              6
            ],
            'm': [
              2,
              6
            ]
          },
          '31': {
            'n': [
              2,
              7
            ],
            'w': [
              2,
              7
            ],
            'p': [
              2,
              7
            ],
            'q': [
              2,
              7
            ],
            'b': [
              2,
              7
            ],
            's': [
              2,
              7
            ],
            'm': [
              2,
              7
            ]
          },
          '32': {
            'n': [
              2,
              8
            ],
            'w': [
              2,
              8
            ],
            'p': [
              2,
              8
            ],
            'q': [
              2,
              8
            ],
            'b': [
              2,
              8
            ],
            's': [
              2,
              8
            ],
            'm': [
              2,
              8
            ]
          },
          '33': {
            'b': [
              1,
              undefined,
              1
            ],
            'm': [
              1,
              undefined,
              2
            ],
            'n': [
              1,
              undefined,
              3
            ],
            'p': [
              1,
              undefined,
              4
            ],
            'q': [
              1,
              undefined,
              5
            ],
            's': [
              1,
              undefined,
              6
            ],
            'w': [
              1,
              undefined,
              7
            ]
          },
          '34': {
            'a': [
              2,
              40
            ],
            'r': [
              2,
              40
            ],
            't': [
              2,
              40
            ],
            'u': [
              2,
              40
            ],
            'v': [
              2,
              40
            ],
            'x': [
              2,
              40
            ],
            'b': [
              1,
              undefined,
              1
            ],
            'm': [
              1,
              undefined,
              2
            ],
            'p': [
              1,
              undefined,
              4
            ],
            'q': [
              1,
              undefined,
              5
            ],
            's': [
              1,
              undefined,
              6
            ]
          },
          '35': {
            'a': [
              2,
              37
            ],
            'r': [
              2,
              37
            ],
            't': [
              2,
              37
            ],
            'u': [
              2,
              37
            ],
            'v': [
              2,
              37
            ],
            'x': [
              2,
              37
            ],
            'p': [
              2,
              37
            ],
            'q': [
              2,
              37
            ],
            'b': [
              2,
              37
            ],
            's': [
              2,
              37
            ],
            'm': [
              2,
              37
            ]
          },
          '36': {
            'a': [
              2,
              19
            ],
            'r': [
              2,
              19
            ],
            't': [
              2,
              19
            ],
            'u': [
              2,
              19
            ],
            'v': [
              2,
              19
            ],
            'x': [
              2,
              19
            ],
            'p': [
              2,
              19
            ],
            'q': [
              2,
              19
            ],
            'b': [
              2,
              19
            ],
            's': [
              2,
              19
            ],
            'm': [
              2,
              19
            ],
            'l': [
              2,
              19
            ]
          },
          '37': {
            'n': [
              2,
              17
            ],
            'o': [
              2,
              17
            ]
          },
          '38': {
            'n': [
              2,
              18
            ],
            'o': [
              2,
              18
            ]
          },
          '39': {
            'n': [
              2,
              13
            ],
            'o': [
              2,
              13
            ]
          },
          '40': {
            'n': [
              2,
              14
            ],
            'o': [
              2,
              14
            ]
          },
          '41': {
            'n': [
              2,
              15
            ],
            'o': [
              2,
              15
            ]
          },
          '42': {
            'n': [
              2,
              16
            ],
            'o': [
              2,
              16
            ]
          },
          '43': {
            'n': [
              1,
              undefined,
              48
            ],
            'o': [
              1,
              undefined,
              49
            ]
          },
          '44': {
            'a': [
              2,
              25
            ],
            'r': [
              2,
              25
            ],
            't': [
              2,
              25
            ],
            'u': [
              2,
              25
            ],
            'v': [
              2,
              25
            ],
            'x': [
              2,
              25
            ],
            'p': [
              2,
              25
            ],
            'q': [
              2,
              25
            ],
            'b': [
              2,
              25
            ],
            's': [
              2,
              25
            ],
            'm': [
              2,
              25
            ]
          },
          '45': {
            'l': [
              1,
              undefined,
              51
            ]
          },
          '46': {
            'a': [
              2,
              2
            ],
            'r': [
              2,
              2
            ],
            't': [
              1,
              undefined,
              29
            ],
            'u': [
              1,
              undefined,
              30
            ],
            'v': [
              1,
              undefined,
              31
            ],
            'x': [
              1,
              undefined,
              32
            ]
          },
          '47': {
            'a': [
              2,
              4
            ],
            'r': [
              2,
              4
            ],
            't': [
              2,
              4
            ],
            'u': [
              2,
              4
            ],
            'v': [
              2,
              4
            ],
            'x': [
              2,
              4
            ]
          },
          '48': {
            'c': [
              2,
              20
            ]
          },
          '49': {
            'c': [
              2,
              21
            ]
          },
          '50': {
            'c': [
              1,
              undefined,
              52
            ]
          },
          '51': {
            'a': [
              2,
              23
            ],
            'r': [
              2,
              23
            ],
            't': [
              2,
              23
            ],
            'u': [
              2,
              23
            ],
            'v': [
              2,
              23
            ],
            'x': [
              2,
              23
            ],
            'p': [
              2,
              23
            ],
            'q': [
              2,
              23
            ],
            'b': [
              2,
              23
            ],
            's': [
              2,
              23
            ],
            'm': [
              2,
              23
            ],
            'l': [
              2,
              23
            ]
          },
          '52': {
            'a': [
              2,
              22
            ],
            'r': [
              2,
              22
            ],
            't': [
              2,
              22
            ],
            'u': [
              2,
              22
            ],
            'v': [
              2,
              22
            ],
            'x': [
              2,
              22
            ],
            'p': [
              2,
              22
            ],
            'q': [
              2,
              22
            ],
            'b': [
              2,
              22
            ],
            's': [
              2,
              22
            ],
            'm': [
              2,
              22
            ],
            'l': [
              2,
              22
            ]
          }
        }
      };
      parser.parse = function parse(input, filename) {
        var self = this, lexer = self.lexer, state, symbol, action, table = self.table, gotos = table.gotos, tableAction = table.action, productions = self.productions, valueStack = [null], prefix = filename ? 'in file: ' + filename + ' ' : '', stack = [0];
        lexer.resetInput(input);
        while (1) {
          state = stack[stack.length - 1];
          if (!symbol) {
            symbol = lexer.lex();
          }
          if (symbol) {
            action = tableAction[state] && tableAction[state][symbol];
          } else {
            action = null;
          }
          if (!action) {
            var expected = [], error;
            if (tableAction[state]) {
              for (var symbolForState in tableAction[state]) {
                expected.push(self.lexer.mapReverseSymbol(symbolForState));
              }
            }
            error = prefix + 'syntax error at line ' + lexer.lineNumber + ':\n' + lexer.showDebugInfo() + '\n' + 'expect ' + expected.join(', ');
            throw new Error(error);
          }
          switch (action[GrammarConst.TYPE_INDEX]) {
          case GrammarConst.SHIFT_TYPE:
            stack.push(symbol);
            valueStack.push(lexer.text);
            stack.push(action[GrammarConst.TO_INDEX]);
            symbol = null;
            break;
          case GrammarConst.REDUCE_TYPE:
            var production = productions[action[GrammarConst.PRODUCTION_INDEX]], reducedSymbol = production.symbol || production[0], reducedAction = production.action || production[2], reducedRhs = production.rhs || production[1], len = reducedRhs.length, i = 0, ret, $$ = valueStack[valueStack.length - len];
            ret = undefined;
            self.$$ = $$;
            for (; i < len; i++) {
              self['$' + (len - i)] = valueStack[valueStack.length - 1 - i];
            }
            if (reducedAction) {
              ret = reducedAction.call(self);
            }
            if (ret !== undefined) {
              $$ = ret;
            } else {
              $$ = self.$$;
            }
            stack = stack.slice(0, -1 * len * 2);
            valueStack = valueStack.slice(0, -1 * len);
            stack.push(reducedSymbol);
            valueStack.push($$);
            var newState = gotos[stack[stack.length - 2]][stack[stack.length - 1]];
            stack.push(newState);
            break;
          case GrammarConst.ACCEPT_TYPE:
            return $$;
          }
        }
      };
      return parser;
    }();
    if (typeof module !== 'undefined') {
      exports = parser;
    }
    return exports;
  }();
  querySelector = function (exports) {
    var util = querySelectorUtil;
    var parser = querySelectorParser;
    var EXPANDO_SELECTOR_KEY = '_ks_data_selector_id_', caches = {}, isContextXML, uuid = 0, subMatchesCache = {}, getAttr = function (el, name) {
        if (isContextXML) {
          return util.getSimpleAttr(el, name);
        } else {
          return util.attr(el, name);
        }
      }, hasSingleClass = util.hasSingleClass, isTag = util.isTag, aNPlusB = /^(([+-]?(?:\d+)?)?n)?([+-]?\d+)?$/;
    var unescape = /\\([\da-fA-F]{1,6}[\x20\t\r\n\f]?|.)/g, unescapeFn = function (_, escaped) {
        var high = '0x' + escaped - 65536;
        return isNaN(high) ? escaped : high < 0 ? String.fromCharCode(high + 65536) : String.fromCharCode(high >> 10 | 55296, high & 1023 | 56320);
      };
    var matchExpr;
    var pseudoFnExpr = {
      'nth-child': function (el, param) {
        var ab = getAb(param), a = ab.a, b = ab.b;
        if (a === 0 && b === 0) {
          return 0;
        }
        var index = 0, parent = el.parentNode;
        if (parent) {
          var childNodes = parent.childNodes, count = 0, child, ret, len = childNodes.length;
          for (; count < len; count++) {
            child = childNodes[count];
            if (child.nodeType === 1) {
              index++;
              ret = matchIndexByAb(index, a, b, child === el);
              if (ret !== undefined) {
                return ret;
              }
            }
          }
        }
        return 0;
      },
      'nth-last-child': function (el, param) {
        var ab = getAb(param), a = ab.a, b = ab.b;
        if (a === 0 && b === 0) {
          return 0;
        }
        var index = 0, parent = el.parentNode;
        if (parent) {
          var childNodes = parent.childNodes, len = childNodes.length, count = len - 1, child, ret;
          for (; count >= 0; count--) {
            child = childNodes[count];
            if (child.nodeType === 1) {
              index++;
              ret = matchIndexByAb(index, a, b, child === el);
              if (ret !== undefined) {
                return ret;
              }
            }
          }
        }
        return 0;
      },
      'nth-of-type': function (el, param) {
        var ab = getAb(param), a = ab.a, b = ab.b;
        if (a === 0 && b === 0) {
          return 0;
        }
        var index = 0, parent = el.parentNode;
        if (parent) {
          var childNodes = parent.childNodes, elType = el.tagName, count = 0, child, ret, len = childNodes.length;
          for (; count < len; count++) {
            child = childNodes[count];
            if (child.tagName === elType) {
              index++;
              ret = matchIndexByAb(index, a, b, child === el);
              if (ret !== undefined) {
                return ret;
              }
            }
          }
        }
        return 0;
      },
      'nth-last-of-type': function (el, param) {
        var ab = getAb(param), a = ab.a, b = ab.b;
        if (a === 0 && b === 0) {
          return 0;
        }
        var index = 0, parent = el.parentNode;
        if (parent) {
          var childNodes = parent.childNodes, len = childNodes.length, elType = el.tagName, count = len - 1, child, ret;
          for (; count >= 0; count--) {
            child = childNodes[count];
            if (child.tagName === elType) {
              index++;
              ret = matchIndexByAb(index, a, b, child === el);
              if (ret !== undefined) {
                return ret;
              }
            }
          }
        }
        return 0;
      },
      lang: function (el, lang) {
        var elLang;
        lang = unEscape(lang.toLowerCase());
        do {
          if (elLang = isContextXML ? el.getAttribute('xml:lang') || el.getAttribute('lang') : el.lang) {
            elLang = elLang.toLowerCase();
            return elLang === lang || elLang.indexOf(lang + '-') === 0;
          }
        } while ((el = el.parentNode) && el.nodeType === 1);
        return false;
      },
      not: function (el, negationArg) {
        return !matchExpr[negationArg.t](el, negationArg.value);
      }
    };
    var pseudoIdentExpr = {
      empty: function (el) {
        var childNodes = el.childNodes, index = 0, len = childNodes.length - 1, child, nodeType;
        for (; index < len; index++) {
          child = childNodes[index];
          nodeType = child.nodeType;
          if (nodeType === 1 || nodeType === 3 || nodeType === 4 || nodeType === 5) {
            return 0;
          }
        }
        return 1;
      },
      root: function (el) {
        if (el.nodeType === 9) {
          return true;
        }
        return el.ownerDocument && el === el.ownerDocument.documentElement;
      },
      'first-child': function (el) {
        return pseudoFnExpr['nth-child'](el, 1);
      },
      'last-child': function (el) {
        return pseudoFnExpr['nth-last-child'](el, 1);
      },
      'first-of-type': function (el) {
        return pseudoFnExpr['nth-of-type'](el, 1);
      },
      'last-of-type': function (el) {
        return pseudoFnExpr['nth-last-of-type'](el, 1);
      },
      'only-child': function (el) {
        return pseudoIdentExpr['first-child'](el) && pseudoIdentExpr['last-child'](el);
      },
      'only-of-type': function (el) {
        return pseudoIdentExpr['first-of-type'](el) && pseudoIdentExpr['last-of-type'](el);
      },
      focus: function (el) {
        var doc = el.ownerDocument;
        return doc && el === doc.activeElement && (!doc.hasFocus || doc.hasFocus()) && !!(el.type || el.href || el.tabIndex >= 0);
      },
      target: function (el) {
        var hash = location.hash;
        return hash && hash.slice(1) === getAttr(el, 'id');
      },
      enabled: function (el) {
        return !el.disabled;
      },
      disabled: function (el) {
        return el.disabled;
      },
      checked: function (el) {
        var nodeName = el.nodeName.toLowerCase();
        return nodeName === 'input' && el.checked || nodeName === 'option' && el.selected;
      }
    };
    var attributeExpr = {
      '~=': function (elValue, value) {
        if (!value || value.indexOf(' ') > -1) {
          return 0;
        }
        return (' ' + elValue + ' ').indexOf(' ' + value + ' ') !== -1;
      },
      '|=': function (elValue, value) {
        return (' ' + elValue).indexOf(' ' + value + '-') !== -1;
      },
      '^=': function (elValue, value) {
        return value && util.startsWith(elValue, value);
      },
      '$=': function (elValue, value) {
        return value && util.endsWith(elValue, value);
      },
      '*=': function (elValue, value) {
        return value && elValue.indexOf(value) !== -1;
      },
      '=': function (elValue, value) {
        return elValue === value;
      }
    };
    var relativeExpr = {
      '>': {
        dir: 'parentNode',
        immediate: 1
      },
      ' ': { dir: 'parentNode' },
      '+': {
        dir: 'previousSibling',
        immediate: 1
      },
      '~': { dir: 'previousSibling' }
    };
    matchExpr = {
      tag: isTag,
      cls: hasSingleClass,
      id: function (el, value) {
        return getAttr(el, 'id') === value;
      },
      attrib: function (el, value) {
        var name = value.ident;
        if (!isContextXML) {
          name = name.toLowerCase();
        }
        var elValue = getAttr(el, name);
        var match = value.match;
        if (!match && elValue !== undefined) {
          return 1;
        } else if (match) {
          if (elValue === undefined) {
            return 0;
          }
          var matchFn = attributeExpr[match];
          if (matchFn) {
            return matchFn(elValue + '', value.value + '');
          }
        }
        return 0;
      },
      pseudo: function (el, value) {
        var fn, fnStr, ident;
        if (fnStr = value.fn) {
          if (!(fn = pseudoFnExpr[fnStr])) {
            throw new SyntaxError('Syntax error: not support pseudo: ' + fnStr);
          }
          return fn(el, value.param);
        }
        if (ident = value.ident) {
          if (!pseudoIdentExpr[ident]) {
            throw new SyntaxError('Syntax error: not support pseudo: ' + ident);
          }
          return pseudoIdentExpr[ident](el);
        }
        return 0;
      }
    };
    function unEscape(str) {
      return str.replace(unescape, unescapeFn);
    }
    parser.lexer.yy = {
      trim: util.trim,
      unEscape: unEscape,
      unEscapeStr: function (str) {
        return this.unEscape(str.slice(1, -1));
      }
    };
    function resetStatus() {
      subMatchesCache = {};
    }
    function dir(el, direction) {
      do {
        el = el[direction];
      } while (el && el.nodeType !== 1);
      return el;
    }
    function getAb(param) {
      var a = 0, match, b = 0;
      if (typeof param === 'number') {
        b = param;
      } else if (param === 'odd') {
        a = 2;
        b = 1;
      } else if (param === 'even') {
        a = 2;
        b = 0;
      } else if (match = param.replace(/\s/g, '').match(aNPlusB)) {
        if (match[1]) {
          a = parseInt(match[2], 10);
          if (isNaN(a)) {
            if (match[2] === '-') {
              a = -1;
            } else {
              a = 1;
            }
          }
        } else {
          a = 0;
        }
        b = parseInt(match[3], 10) || 0;
      }
      return {
        a: a,
        b: b
      };
    }
    function matchIndexByAb(index, a, b, eq) {
      if (a === 0) {
        if (index === b) {
          return eq;
        }
      } else {
        if ((index - b) / a >= 0 && (index - b) % a === 0 && eq) {
          return 1;
        }
      }
      return undefined;
    }
    function isXML(elem) {
      var documentElement = elem && (elem.ownerDocument || elem).documentElement;
      return documentElement ? documentElement.nodeName.toLowerCase() !== 'html' : false;
    }
    function matches(str, seeds) {
      return select(str, null, seeds);
    }
    function singleMatch(el, match) {
      if (!match) {
        return true;
      }
      if (!el) {
        return false;
      }
      if (el.nodeType === 9) {
        return false;
      }
      var matched = 1, matchSuffix = match.suffix, matchSuffixLen, matchSuffixIndex;
      if (match.t === 'tag') {
        matched &= matchExpr.tag(el, match.value);
      }
      if (matched && matchSuffix) {
        matchSuffixLen = matchSuffix.length;
        matchSuffixIndex = 0;
        for (; matched && matchSuffixIndex < matchSuffixLen; matchSuffixIndex++) {
          var singleMatchSuffix = matchSuffix[matchSuffixIndex], singleMatchSuffixType = singleMatchSuffix.t;
          if (matchExpr[singleMatchSuffixType]) {
            matched &= matchExpr[singleMatchSuffixType](el, singleMatchSuffix.value);
          }
        }
      }
      return matched;
    }
    function matchImmediate(el, match) {
      var matched = 1, startEl = el, relativeOp, startMatch = match;
      do {
        matched &= singleMatch(el, match);
        if (matched) {
          match = match && match.prev;
          if (!match) {
            return true;
          }
          relativeOp = relativeExpr[match.nextCombinator];
          el = dir(el, relativeOp.dir);
          if (!relativeOp.immediate) {
            return {
              el: el,
              match: match
            };
          }
        } else {
          relativeOp = relativeExpr[match.nextCombinator];
          if (relativeOp.immediate) {
            return {
              el: dir(startEl, relativeExpr[startMatch.nextCombinator].dir),
              match: startMatch
            };
          } else {
            return {
              el: el && dir(el, relativeOp.dir),
              match: match
            };
          }
        }
      } while (el);
      return {
        el: dir(startEl, relativeExpr[startMatch.nextCombinator].dir),
        match: startMatch
      };
    }
    function findFixedMatchFromHead(el, head) {
      var relativeOp, cur = head;
      do {
        if (!singleMatch(el, cur)) {
          return null;
        }
        cur = cur.prev;
        if (!cur) {
          return true;
        }
        relativeOp = relativeExpr[cur.nextCombinator];
        el = dir(el, relativeOp.dir);
      } while (el && relativeOp.immediate);
      if (!el) {
        return null;
      }
      return {
        el: el,
        match: cur
      };
    }
    function genId(el) {
      var selectorId;
      if (isContextXML) {
        if (!(selectorId = el.getAttribute(EXPANDO_SELECTOR_KEY))) {
          el.setAttribute(EXPANDO_SELECTOR_KEY, selectorId = +new Date() + '_' + ++uuid);
        }
      } else {
        if (!(selectorId = el[EXPANDO_SELECTOR_KEY])) {
          selectorId = el[EXPANDO_SELECTOR_KEY] = +new Date() + '_' + ++uuid;
        }
      }
      return selectorId;
    }
    function matchSub(el, match) {
      var selectorId = genId(el), matchKey;
      matchKey = selectorId + '_' + (match.order || 0);
      if (matchKey in subMatchesCache) {
        return subMatchesCache[matchKey];
      }
      subMatchesCache[matchKey] = matchSubInternal(el, match);
      return subMatchesCache[matchKey];
    }
    function matchSubInternal(el, match) {
      var matchImmediateRet = matchImmediate(el, match);
      if (matchImmediateRet === true) {
        return true;
      } else {
        el = matchImmediateRet.el;
        match = matchImmediateRet.match;
        while (el) {
          if (matchSub(el, match)) {
            return true;
          }
          el = dir(el, relativeExpr[match.nextCombinator].dir);
        }
        return false;
      }
    }
    function select(str, context, seeds) {
      if (!caches[str]) {
        caches[str] = parser.parse(str);
      }
      var selector = caches[str], groupIndex = 0, groupLen = selector.length, contextDocument, group, ret = [];
      if (seeds) {
        context = context || seeds[0].ownerDocument;
      }
      contextDocument = context && context.ownerDocument || typeof document !== 'undefined' && document;
      if (context && context.nodeType === 9 && !contextDocument) {
        contextDocument = context;
      }
      context = context || contextDocument;
      isContextXML = isXML(context);
      for (; groupIndex < groupLen; groupIndex++) {
        resetStatus();
        group = selector[groupIndex];
        var suffix = group.suffix, suffixIndex, suffixLen, seedsIndex, mySeeds = seeds, seedsLen, id = null;
        if (!mySeeds) {
          if (suffix && !isContextXML) {
            suffixIndex = 0;
            suffixLen = suffix.length;
            for (; suffixIndex < suffixLen; suffixIndex++) {
              var singleSuffix = suffix[suffixIndex];
              if (singleSuffix.t === 'id') {
                id = singleSuffix.value;
                break;
              }
            }
          }
          if (id) {
            var doesNotHasById = !context.getElementById, contextInDom = util.contains(contextDocument, context), tmp = doesNotHasById ? contextInDom ? contextDocument.getElementById(id) : null : context.getElementById(id);
            if (!tmp && doesNotHasById || tmp && getAttr(tmp, 'id') !== id) {
              var tmps = util.getElementsByTagName('*', context), tmpLen = tmps.length, tmpI = 0;
              for (; tmpI < tmpLen; tmpI++) {
                tmp = tmps[tmpI];
                if (getAttr(tmp, 'id') === id) {
                  mySeeds = [tmp];
                  break;
                }
              }
              if (tmpI === tmpLen) {
                mySeeds = [];
              }
            } else {
              if (contextInDom && tmp && context !== contextDocument) {
                tmp = util.contains(context, tmp) ? tmp : null;
              }
              mySeeds = tmp ? [tmp] : [];
            }
          } else {
            mySeeds = util.getElementsByTagName(group.value || '*', context);
          }
        }
        seedsIndex = 0;
        seedsLen = mySeeds.length;
        if (!seedsLen) {
          continue;
        }
        for (; seedsIndex < seedsLen; seedsIndex++) {
          var seed = mySeeds[seedsIndex];
          var matchHead = findFixedMatchFromHead(seed, group);
          if (matchHead === true) {
            ret.push(seed);
          } else if (matchHead) {
            if (matchSub(matchHead.el, matchHead.match)) {
              ret.push(seed);
            }
          }
        }
      }
      if (groupLen > 1) {
        ret = util.unique(ret);
      }
      return ret;
    }
    exports = select;
    select.parse = function (str) {
      return parser.parse(str);
    };
    select.matches = matches;
    select.util = util;
    select.version = '1.0.6';
    return exports;
  }();
  exports = querySelector;
  return exports;
}();
return _querySelector_;
})();

cssGather();
cssApply();

return true;
}

// Now put it all together!
function cssGather()
{
cssParser = new cssjs;
cssList = [];

// <style> tags in the html.
var a = document.getElementsByTagName("style");
var i, t;
for(i=0; i<a.length; ++i) {
t = a[i];
if(t.data)
cssList = cssList.concat(cssParser.parseCSS(t.data));
}

// <link type=text/css> tags in the html.
a = document.getElementsByTagName("link");
for(i=0; i<a.length; ++i) {
t = a[i];
if(t.type && t.type.toLowerCase() === "text/css" && t.data)
cssList = cssList.concat(cssParser.parseCSS(t.data));
}

// Keep cssList around in case we create new nodes,
// which should inherit these css attributes.
}

function cssApply(e, destination)
{
var i, j, k;
var a, t, d;
for(i=0; i<cssList.length; ++i) {
d = cssList[i]; // css descriptor
var sel = d.selector;
// certain modifiers not supported in this static view.
// a:link is the same as a.
sel = sel.replace(/:link$/, "");
// :hover :visited etc are dynamic an not relevant here.
if(sel.match(/:(hover|visited|active)$/))
continue;
a = querySelectorAll(sel);
for(j=0; j<a.length; ++j) {
t = a[j];
// If an element is specified then we only key on that.
if(e && e != t) continue;
for(k=0; k<d.rules.length; ++k) {
if(destination) {
if(!destination.hasAttribute(d.rules[k].directive))
destination.setAttribute(d.rules[k].directive, d.rules[k].value);
} else {
if(!t.style.hasAttribute(d.rules[k].directive))
t.style.setAttribute(d.rules[k].directive, d.rules[k].value);
}
}
}
}
}

