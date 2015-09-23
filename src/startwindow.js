/*********************************************************************
This file contains support javascript functions used by a browser.
They are much easier to write here, in javascript,
then in C using the js api.
And it is portable amongst all js engines.
This file is converted into a C string and compiled and run
at the start of each javascript window.
Please take advantage of this machinery and put functions here,
even prototypes and getter / setter support functions,
whenever it makes sense to do so.
The classes are created first, so that you can write meaningful prototypes here.
*********************************************************************/

/* Some visual attributes of the window.
 * These are just guesses.
 * Better to have something than nothing at all. */
height = 768;
width = 1024;
status = 0;
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

screen = new Object;
screen.height = 768;
screen.width = 1024;
screen.availHeight = 768;
screen.availWidth = 1024;
screen.availTop = 0;
screen.availLeft = 0;

/* some base arrays - lists of things we'll probably need */
document.heads = new Array;
document.bases = new Array;
document.links = new Array;
document.metas = new Array;
document.bodies = new Array;
document.forms = new Array;
document.elements = new Array;
document.anchors = new Array;
document.divs = new Array;
document.scripts = new Array;
document.paragraphs = new Array;
document.tables = new Array;
document.spans = new Array;
document.images = new Array;
document.areas = new Array;
frames = new Array;

document.getElementsByTagName = function(s) { 
s = s.toLowerCase();
return document.gebtn$(this, s);
}
document.gebtn$ = function(top, s) { 
var a = new Array;
if(s === '*' || (top.nodeName && top.nodeName === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
c = top.childNodes[i];
a = a.concat(document.gebtn$(c, s));
}
}
return a;
}

document.getElementsByName = function(s) { 
s = s.toLowerCase();
return document.gebn$(this, s);
}
document.gebn$ = function(top, s) { 
var a = new Array;
if(s === '*' || (top.name && top.name === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
c = top.childNodes[i];
a = a.concat(document.gebn$(c, s));
}
}
return a;
}

document.getElementsByClassName = function(s) { 
s = s.toLowerCase();
return document.gebcn$(this, s);
}
document.gebcn$ = function(top, s) { 
var a = new Array;
if(s === '*' || (top.className && top.className === s))
a.push(top);
if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
c = top.childNodes[i];
a = a.concat(document.gebcn$(c, s));
}
}
return a;
}

Head.prototype.getElementsByTagName = document.getElementsByTagName;
Head.prototype.getElementsByName = document.getElementsByName;
Head.prototype.getElementsByClassName = document.getElementsByClassName;
Body.prototype.getElementsByTagName = document.getElementsByTagName;
Body.prototype.getElementsByName = document.getElementsByName;
Body.prototype.getElementsByClassName = document.getElementsByClassName;
Form.prototype.getElementsByTagName = document.getElementsByTagName;
Form.prototype.getElementsByName = document.getElementsByName;
Form.prototype.getElementsByClassName = document.getElementsByClassName;
Element.prototype.getElementsByTagName = document.getElementsByTagName;
Element.prototype.getElementsByName = document.getElementsByName;
Element.prototype.getElementsByClassName = document.getElementsByClassName;
Anchor.prototype.getElementsByTagName = document.getElementsByTagName;
Anchor.prototype.getElementsByName = document.getElementsByName;
Anchor.prototype.getElementsByClassName = document.getElementsByClassName;
Div.prototype.getElementsByTagName = document.getElementsByTagName;
Div.prototype.getElementsByName = document.getElementsByName;
Div.prototype.getElementsByClassName = document.getElementsByClassName;
Script.prototype.getElementsByTagName = document.getElementsByTagName;
Script.prototype.getElementsByName = document.getElementsByName;
Script.prototype.getElementsByClassName = document.getElementsByClassName;
P.prototype.getElementsByTagName = document.getElementsByTagName;
P.prototype.getElementsByName = document.getElementsByName;
P.prototype.getElementsByClassName = document.getElementsByClassName;
Table.prototype.getElementsByTagName = document.getElementsByTagName;
Table.prototype.getElementsByName = document.getElementsByName;
Table.prototype.getElementsByClassName = document.getElementsByClassName;
Tbody.prototype.getElementsByTagName = document.getElementsByTagName;
Tbody.prototype.getElementsByName = document.getElementsByName;
Tbody.prototype.getElementsByClassName = document.getElementsByClassName;
Trow.prototype.getElementsByTagName = document.getElementsByTagName;
Trow.prototype.getElementsByName = document.getElementsByName;
Trow.prototype.getElementsByClassName = document.getElementsByClassName;
Cell.prototype.getElementsByTagName = document.getElementsByTagName;
Cell.prototype.getElementsByName = document.getElementsByName;
Cell.prototype.getElementsByClassName = document.getElementsByClassName;
Span.prototype.getElementsByTagName = document.getElementsByTagName;
Span.prototype.getElementsByName = document.getElementsByName;
Span.prototype.getElementsByClassName = document.getElementsByClassName;

document.idMaster = new Object;
document.getElementById = function(s) { 
/* take advantage of the js hash lookup */
return document.idMaster[s]; 
}

/* originally ms extension pre-DOM, we don't fully support it
* but offer the document.all.tags method because that was here already */
document.all = new Object;
document.all.tags = function(s) { 
return document.gebtn$(document.body, s.toLowerCase());
}

/* document.createElement is a native wrapper around this function */
document.crel$$ = function(s) { 
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
case "script":
c = new Script();
break;
case "div":
c = new Div();
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
default:
/* alert("createElement default " + s); */
c = new Element();
}
/* ok, for some element types this perhaps doesn't make sense,
* but for most visible ones it does and I doubt it matters much */
c.style = new Object;
c.childNodes = new Array;
c.nodeName = t;
return c;
} 

document.createTextNode = function(t) {
var c = new TextNode(t);
c.nodeName = "text";
return c;
}

/* window.open is the same as new window, just pass the args through */
function open() {
return Window.apply(this, arguments);
}

var $urlpro = URL.prototype;

/* rebuild the href string from its components.
 * Call this when a component changes.
 * All components are strings, except for port,
 * and all should be defined, even if they are empty. */
$urlpro.rebuild = function() {
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
// $urlpro.protocol = { ... };
Object.defineProperty($urlpro, "protocol", {
  get: function() {return this.protocol$val; },
  set: function(v) { this.protocol$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "pathname", {
  get: function() {return this.pathname$val; },
  set: function(v) { this.pathname$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "search", {
  get: function() {return this.search$val; },
  set: function(v) { this.search$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "hash", {
  get: function() {return this.hash$val; },
  set: function(v) { this.hash$val = v; this.rebuild(); }
});

Object.defineProperty($urlpro, "port", {
  get: function() {return this.port$val; },
  set: function(v) { this.port$val = v;
if(this.hostname$val.length)
this.host$val = this.hostname$val + ":" + v;
this.rebuild(); }
});

Object.defineProperty($urlpro, "hostname", {
  get: function() {return this.hostname$val; },
  set: function(v) { this.hostname$val = v;
if(this.port$val)
this.host$val = v + ":" +  this.port$val;
this.rebuild(); }
});

Object.defineProperty($urlpro, "host", {
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

var prot$port = {
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
function default$port(p) {
var port = 0;
p = p.toLowerCase().replace(/:/, "");
if(prot$port.hasOwnProperty(p))
port = parseInt(prot$port[p]);
return port;
}

Object.defineProperty($urlpro, "href", {
  get: function() {return this.href$val; },
  set: function(v) { this.href$val = v;
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
this.port$val = default$port(this.protocol$val);
}
if(v.match(/[#?]/)) {
this.pathname$val = v.replace(/[#?].*/, "");
v = v.replace(/^[^#?]*/, "");
} else {
this.pathmname$val = v;
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

/*********************************************************************
This is our addEventListener function.
It is bound to window, which is ok because window has such a function
to listen to load and unload.
Later on we will bind it to document and to other elements via
element.addEventListener = addEventListener
Or maybe URL.prototype.addEventListener = addEventListener
to cover all the hyperlinks in one go.
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
this[ev] = function(){
var a = this[evarray]; /* should be an array */
if(this[evorig]) this[evorig]();
for(var i = 0; i<a.length; ++i) a[i]();
};
}
this[evarray].push(handler);
}

/* For grins let's put in the other standard. */

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
this[ev] = function(){
var a = this[evarray]; /* should be an array */
if(this[evorig]) this[evorig]();
for(var i = 0; i<a.length; ++i) a[i]();
};
}
this[evarray].push(handler);
}

document.addEventListener = window.addEventListener;
document.attachEvent = window.attachEvent;
Body.prototype.addEventListener = window.addEventListener;
Body.prototype.attachEvent = window.attachEvent;
Form.prototype.addEventListener = window.addEventListener;
Form.prototype.attachEvent = window.attachEvent;
Element.prototype.addEventListener = window.addEventListener;
Element.prototype.attachEvent = window.attachEvent;
Anchor.prototype.addEventListener = window.addEventListener;
Anchor.prototype.attachEvent = window.attachEvent;

/* document.appendChild and document.apch$ are native */
document.childNodes = new Array;
document.firstChild = function() { return (this.childNodes.length > 0 ? this.childNodes[0] : undefined); }
document.lastChild = function() { return (this.childNodes.length > 0 ? this.childNodes[this.childNodes.length-1] : undefined); }
document.hasChildNodes = function() { return (this.childNodes.length > 0); }
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

Head.prototype.appendChild = document.appendChild;
Head.prototype.apch$ = document.apch$;
Head.prototype.insertBefore = document.insertBefore;
Head.prototype.firstChild = document.firstChild;
Head.prototype.lastChild = document.lastChild;
Head.prototype.hasChildNodes = document.hasChildNodes;
Head.prototype.removeChild = document.removeChild;
Head.prototype.replaceChild = document.replaceChild;
Head.prototype.setAttribute = document.setAttribute;
Head.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Body.prototype.appendChild = document.appendChild;
Body.prototype.apch$ = document.apch$;
Body.prototype.insertBefore = document.insertBefore;
Body.prototype.firstChild = document.firstChild;
Body.prototype.lastChild = document.lastChild;
Body.prototype.hasChildNodes = document.hasChildNodes;
Body.prototype.removeChild = document.removeChild;
Body.prototype.replaceChild = document.replaceChild;
Body.prototype.setAttribute = document.setAttribute;
Body.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Form.prototype.appendChild = document.appendChild;
Form.prototype.apch$ = document.apch$;
Form.prototype.insertBefore = document.insertBefore;
Form.prototype.firstChild = document.firstChild;
Form.prototype.lastChild = document.lastChild;
Form.prototype.hasChildNodes = document.hasChildNodes;
Form.prototype.removeChild = document.removeChild;
Form.prototype.replaceChild = document.replaceChild;
Form.prototype.setAttribute = document.setAttribute;
Form.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Element.prototype.appendChild = document.appendChild;
Element.prototype.apch$ = document.apch$;
Element.prototype.insertBefore = document.insertBefore;
Element.prototype.firstChild = document.firstChild;
Element.prototype.lastChild = document.lastChild;
Element.prototype.hasChildNodes = document.hasChildNodes;
Element.prototype.removeChild = document.removeChild;
Element.prototype.replaceChild = document.replaceChild;
Element.prototype.setAttribute = document.setAttribute;
Element.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Element.prototype.focus = document.focus;
Element.prototype.blur = document.blur;
Anchor.prototype.appendChild = document.appendChild;
Anchor.prototype.apch$ = document.apch$;
Anchor.prototype.focus = document.focus;
Anchor.prototype.blur = document.blur;
Div.prototype.appendChild = document.appendChild;
Div.prototype.apch$ = document.apch$;
Div.prototype.insertBefore = document.insertBefore;
Div.prototype.firstChild = document.firstChild;
Div.prototype.lastChild = document.lastChild;
Div.prototype.hasChildNodes = document.hasChildNodes;
Div.prototype.removeChild = document.removeChild;
Div.prototype.replaceChild = document.replaceChild;
Div.prototype.setAttribute = document.setAttribute;
Div.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Script.prototype.setAttribute = document.setAttribute;
Script.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
P.prototype.appendChild = document.appendChild;
P.prototype.apch$ = document.apch$;
P.prototype.setAttribute = document.setAttribute;
P.prototype.insertBefore = document.insertBefore;
P.prototype.firstChild = document.firstChild;
P.prototype.lastChild = document.lastChild;
P.prototype.hasChildNodes = document.hasChildNodes;
P.prototype.removeChild = document.removeChild;
P.prototype.replaceChild = document.replaceChild;
Table.prototype.appendChild = document.appendChild;
Table.prototype.apch$ = document.apch$;
Table.prototype.setAttribute = document.setAttribute;
Table.prototype.insertBefore = document.insertBefore;
Table.prototype.firstChild = document.firstChild;
Table.prototype.lastChild = document.lastChild;
Table.prototype.hasChildNodes = document.hasChildNodes;
Table.prototype.removeChild = document.removeChild;
Table.prototype.replaceChild = document.replaceChild;
Tbody.prototype.appendChild = document.appendChild;
Tbody.prototype.apch$ = document.apch$;
Tbody.prototype.setAttribute = document.setAttribute;
Tbody.prototype.insertBefore = document.insertBefore;
Tbody.prototype.firstChild = document.firstChild;
Tbody.prototype.lastChild = document.lastChild;
Tbody.prototype.hasChildNodes = document.hasChildNodes;
Tbody.prototype.removeChild = document.removeChild;
Tbody.prototype.replaceChild = document.replaceChild;
Trow.prototype.appendChild = document.appendChild;
Trow.prototype.apch$ = document.apch$;
Trow.prototype.setAttribute = document.setAttribute;
Trow.prototype.insertBefore = document.insertBefore;
Trow.prototype.firstChild = document.firstChild;
Trow.prototype.lastChild = document.lastChild;
Trow.prototype.hasChildNodes = document.hasChildNodes;
Trow.prototype.removeChild = document.removeChild;
Trow.prototype.replaceChild = document.replaceChild;
Cell.prototype.appendChild = document.appendChild;
Cell.prototype.apch$ = document.apch$;
Cell.prototype.setAttribute = document.setAttribute;
Cell.prototype.insertBefore = document.insertBefore;
Cell.prototype.firstChild = document.firstChild;
Cell.prototype.lastChild = document.lastChild;
Cell.prototype.hasChildNodes = document.hasChildNodes;
Cell.prototype.removeChild = document.removeChild;
Cell.prototype.replaceChild = document.replaceChild;
Span.prototype.appendChild = document.appendChild;
Span.prototype.apch$ = document.apch$;
Span.prototype.setAttribute = document.setAttribute;
Span.prototype.insertBefore = document.insertBefore;
Span.prototype.firstChild = document.firstChild;
Span.prototype.lastChild = document.lastChild;
Span.prototype.hasChildNodes = document.hasChildNodes;
Span.prototype.removeChild = document.removeChild;
Span.prototype.replaceChild = document.replaceChild;

/* navigator; some parameters are filled in by the buildstartwindow script. */
navigator.appName = "edbrowse";
navigator["appCode Name"] = "edbrowse C/mozjs";
/* not sure what product is about */
navigator.product = "mozjs";
navigator.productSub = "2.4";
navigator.vendor = "Karl Dahlke";
/* language is determined in C, at runtime, by $LANG */
navigator.javaEnabled = function() { return false; }
navigator.taintEnabled = function() { return false; }
navigator.cookieEnabled = true;
navigator.onLine = true;

/* There's no history in edbrowse. */
/* Only the current file is known, hence length is 1. */
history.length = 1;
history.next = "";
history.previous = "";
history.back = function() { return null; }
history.forward = function() { return null; }
history.go = function() { return null; }
history.toString = function() {
 return "Sorry, edbrowse does not maintain a browsing history.";
} 

/* The web console, just the log method for now, and only one arg. */
console = new Object;
console.log = function(obj) {
var today=new Date();
var h=today.getHours();
var m=today.getMinutes();
var s=today.getSeconds();
// add a zero in front of numbers<10
if(h < 10) h = "0" + h;
if(m < 10) m = "0" + m;
if(s < 10) s = "0" + s;
alert("[" + h + ":" + m + ":" + s + "] " + obj);
}
console.info = console.log;
console.warn = console.log;
console.error = console.log;

