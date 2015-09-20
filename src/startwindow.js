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

/* holds our lists of tags, uses javascript's hash lookup */
document.tag$$map = new Object;
/* some base arrays - lists of things we'll probably need */
document.tag$$map.form = new Array;
document.forms = document.tag$$map.form;
document.tag$$map.table = new Array;
document.tables = document.tag$$map.table; 
document.tag$$map.div = new Array;
document.divs = document.tag$$map.div; 
document.tag$$map.a = new Array;
document.anchors = document.tag$$map.a; 
document.tag$$map.link = new Array;
document.links = document.tag$$map.link; 
document.tag$$map.head = new Array;
document.heads = document.tag$$map.head; 
document.tag$$map.body = new Array;
document.bodies = document.tag$$map.body; 
document.tag$$map.html = new Array;
document.htmls = document.tag$$map.html; 
document.tag$$map.base = new Array;
document.bases = document.tag$$map.base; 
document.tag$$map.image = new Array;
document.images = document.tag$$map.image; 
document.tag$$map.img = new Array;
document.images = document.tag$$map.img; 
document.tag$$map.span = new Array;
document.spans = document.tag$$map.span; 
document.tag$$map.meta = new Array;
document.metas = document.tag$$map.meta; 
document.tag$$map.script = new Array;
document.tag$$map.p = new Array;
document.scripts = document.tag$$map.script; 
document.paras = document.tag$$map.p; 

document.idMaster = new Object;
document.getElementsByTagName = function(s) { 
/* this function should really return a node list, whatever one of those is
* but I guess an array is good enough for now */
/* apparently "*" gives you all the elements in the document */
var ret = new Array;
if (s === "*") {
Object.keys(document.tag$$map).forEach(function (key) {
ret.concat(document.tag$$map[key]);
});
}
else {
var t = s.toLowerCase();
if (typeof(document.tag$$map[t]) !== "undefined") {
ret = document.tag$$map[t];
}
}
return ret;
} 

document.getElementById = function(s) { 
/* take advantage of the js hash lookup */
return document.idMaster[s]; 
}

/* originally ms extension pre-DOM, we don't fully support it
* but offer the document.all.tags method because that was here already */
document.all = new Object;
document.all.tags = function(s) { 
return document.getElementsByTagName(s);
}

document.createElement = function(s) { 
var c;
var t = s.toLowerCase();
switch(t) { 
case "a":
c = new Anchor();
break;
case "image":
s = "img";
case "img":
c = new Image();
break;
case "script":
c = new Script();
break;
case "div":
c = new Div();
break;
default:
/* alert("createElement default " + s); */
c = new Element();
} 
/* create an array in our tag map for this tag if it's not there
* we don't push it if it is since we don't know where it should fit in the DOM
*/
if (!document.tag$$map.hasOwnProperty(t)) {
document.tag$$map[t] = new Array;
}
/* ok, for some element types this perhaps doesn't make sense,
* but for most visible ones it does and I doubt it matters much */
c.style = new Object;
c.innerHTML = "";
c.nodeName = s;
return c;
} 

document.createTextNode = function(t) {
return new TextNode(t); }

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
var a = new Array();
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
var a = new Array();
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

/* an array in an html document uses appendchild like push */
Array.prototype.appendChild = function(child) { this.push(child); }
/* insertBefore maps to splice, but we have to find the element. */
/* This prototype assumes all elements are objects. */
Array.prototype.insertBefore = function(newobj, item) {
for(var i=0; i<this.length; ++i)
if(this[i] == item) {
this.splice(i-1, 0, newobj);
return;
}
}
Array.prototype.firstChild = function() { return (this.length ? this[0] : undefined); }
Array.prototype.lastChild = function() { return (this.length ? this[this.length-1] : undefined); }

/*********************************************************************
Of course most of the html objects are not arrays.
Still they need appendChild and insertBefore
Here's how easy in js, just one line.
Body.prototype.appendChild = function(o) { this.elements.appendChild(o); }
But I can't do it in js because there are side effects.
The parent child linkage has to pass back to the html tree in edbrowse.
So it has to be native code.
See appendChild() in jseng*.cpp.
Other calls must also be native, but that isn't done yet.
*********************************************************************/

Body.prototype.insertBefore = function(o, b) { this.elements.insertBefore(o, b); }
Body.prototype.firstChild = function() { return this.elements.firstChild(); }
Body.prototype.lastChild = function() { return this.elements.lastChild(); }
Body.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }
Body.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Head.prototype.insertBefore = function(o, b) { this.elements.insertBefore(o, b); }
Head.prototype.firstChild = function() { return this.elements.firstChild(); }
Head.prototype.lastChild = function() { return this.elements.lastChild(); }
Head.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }
Head.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Form.prototype.insertBefore = function(o, b) { this.elements.insertBefore(o, b); }
Form.prototype.firstChild = function() { return this.elements.firstChild(); }
Form.prototype.lastChild = function() { return this.elements.lastChild(); }
Form.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }
Form.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Element.prototype.insertBefore = function(o, b) { this.elements.insertBefore(o, b); }
Element.prototype.firstChild = function() { return this.elements.firstChild(); }
Element.prototype.lastChild = function() { return this.elements.lastChild(); }
Element.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }
Element.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Div.prototype.insertBefore = function(o, b) { this.elements.insertBefore(o, b); }
Div.prototype.firstChild = function() { return this.elements.firstChild(); }
Div.prototype.lastChild = function() { return this.elements.lastChild(); }
Div.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }
Div.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }
Script.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }
Script.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }

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

