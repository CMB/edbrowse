/* startwindow.c: this file is machine generated; */
/* please edit startwindow.js instead. */

const char startWindowJS[] = "\
/*********************************************************************\n\
This file contains support javascript functions used by a browser.\n\
They are much easier to write here, in javascript,\n\
then in C using the js api.\n\
And it is portable amongst all js engines.\n\
This file is converted into a C string and compiled and run\n\
at the start of each javascript window.\n\
Please take advantage of this machinery and put functions here,\n\
even prototypes and getter / setter support functions,\n\
whenever it makes sense to do so.\n\
The classes are created first, so that you can write meaningful prototypes here.\n\
*********************************************************************/\n\
\n\
/* Some visual attributes of the window.\n\
 * These are just guesses.\n\
 * Better to have something than nothing at all. */\n\
height = 768;\n\
width = 1024;\n\
status = 0;\n\
defaultStatus = 0;\n\
returnValue = true;\n\
menubar = true;\n\
scrollbars = true;\n\
toolbar = true;\n\
resizable = true;\n\
directories = false;\n\
name = \"unspecifiedFrame\";\n\
\n\
document.bgcolor = \"white\";\n\
document.readyState = \"loading\";\n\
\n\
screen = new Object;\n\
screen.height = 768;\n\
screen.width = 1024;\n\
screen.availHeight = 768;\n\
screen.availWidth = 1024;\n\
screen.availTop = 0;\n\
screen.availLeft = 0;\n\
\n\
/* holds our lists of tags, uses javascript's hash lookup */\n\
document.tag$$map = new Object;\n\
/* some base arrays - lists of things we'll probably need */\n\
document.tag$$map.form = new Array;\n\
document.forms = document.tag$$map.form;\n\
document.tag$$map.table = new Array;\n\
document.tables = document.tag$$map.table; \n\
document.tag$$map.div = new Array;\n\
document.divs = document.tag$$map.div; \n\
document.tag$$map.a = new Array;\n\
document.anchors = document.tag$$map.a; \n\
document.tag$$map.link = new Array;\n\
document.links = document.tag$$map.link; \n\
document.tag$$map.head = new Array;\n\
document.heads = document.tag$$map.head; \n\
document.tag$$map.body = new Array;\n\
document.bodies = document.tag$$map.body; \n\
document.tag$$map.html = new Array;\n\
document.htmls = document.tag$$map.html; \n\
document.tag$$map.base = new Array;\n\
document.bases = document.tag$$map.base; \n\
document.tag$$map.image = new Array;\n\
document.images = document.tag$$map.image; \n\
document.tag$$map.img = new Array;\n\
document.images = document.tag$$map.img; \n\
document.tag$$map.span = new Array;\n\
document.spans = document.tag$$map.span; \n\
document.tag$$map.meta = new Array;\n\
document.metas = document.tag$$map.meta; \n\
document.tag$$map.script = new Array;\n\
document.tag$$map.p = new Array;\n\
document.scripts = document.tag$$map.script; \n\
document.paras = document.tag$$map.p; \n\
\n\
document.idMaster = new Object;\n\
document.getElementsByTagName = function(s) { \n\
/* this function should really return a node list, whatever one of those is\n\
* but I guess an array is good enough for now */\n\
/* apparently \"*\" gives you all the elements in the document */\n\
var ret = new Array;\n\
if (s === \"*\") {\n\
Object.keys(document.tag$$map).forEach(function (key) {\n\
ret.concat(document.tag$$map[key]);\n\
});\n\
}\n\
else {\n\
var t = s.toLowerCase();\n\
if (typeof(document.tag$$map[t]) !== \"undefined\") {\n\
ret = document.tag$$map[t];\n\
}\n\
}\n\
return ret;\n\
} \n\
\n\
document.getElementById = function(s) { \n\
/* take advantage of the js hash lookup */\n\
return document.idMaster[s]; \n\
}\n\
\n\
/* originally ms extension pre-DOM, we don't fully support it\n\
* but offer the document.all.tags method because that was here already */\n\
document.all = new Object;\n\
document.all.tags = function(s) { \n\
return document.getElementsByTagName(s);\n\
}\n\
\n\
/* run queue for our script elements */\n\
document.script$$queue = new Array;\n\
document.createElement = function(s) { \n\
var c;\n\
var t = s.toLowerCase();\n\
switch(t) { \n\
case \"a\":\n\
c = new Anchor();\n\
break;\n\
case \"image\":\n\
case \"img\":\n\
c = new Image();\n\
break;\n\
case \"script\":\n\
c = new Script();\n\
/* add the script to the queue to be ran */\n\
document.script$$queue.push(c);\n\
break;\n\
case \"div\":\n\
c = new Div();\n\
break;\n\
default:\n\
/* alert(\"createElement default \" + s); */\n\
c = new Element();\n\
} \n\
/* create an array in our tag map for this tag if it's not there\n\
* we don't push it if it is since we don't know where it should fit in the DOM\n\
*/\n\
if (!document.tag$$map.hasOwnProperty(t)) {\n\
document.tag$$map[t] = new Array;\n\
}\n\
/* ok, for some element types this perhaps doesn't make sense,\n\
* but for most visible ones it does and I doubt it matters much */\n\
c.style = new Object;\n\
return c;\n\
} \n\
\n\
document.createTextNode = function(t) {\n\
return new TextNode(t); }\n\
\n\
/* window.open is the same as new window, just pass the args through */\n\
function open() {\n\
return Window.apply(this, arguments);\n\
}\n\
\n\
var $urlpro = URL.prototype;\n\
\n\
/* rebuild the href string from its components.\n\
 * Call this when a component changes.\n\
 * All components are strings, except for port,\n\
 * and all should be defined, even if they are empty. */\n\
$urlpro.rebuild = function() {\n\
var h = \"\";\n\
if(this.protocol$val.length) {\n\
// protocol includes the colon\n\
h = this.protocol$val;\n\
var plc = h.toLowerCase();\n\
if(plc != \"mailto:\" && plc != \"telnet:\" && plc != \"javascript:\")\n\
h += \"//\";\n\
}\n\
if(this.host$val.length) {\n\
h += this.host$val;\n\
} else if(this.hostname$val.length) {\n\
h += this.hostname$val;\n\
if(this.port$val != 0)\n\
h += \":\" + this.port$val;\n\
}\n\
if(this.pathname$val.length) {\n\
// pathname should always begin with /, should we check for that?\n\
if(!this.pathname$val.match(/^\\//))\n\
h += \"/\";\n\
h += this.pathname$val;\n\
}\n\
if(this.search$val.length) {\n\
// search should always begin with ?, should we check for that?\n\
h += this.search$val;\n\
}\n\
if(this.hash$val.length) {\n\
// hash should always begin with #, should we check for that?\n\
h += this.hash$val;\n\
}\n\
this.href$val = h;\n\
};\n\
\n\
// No idea why we can't just assign the property directly.\n\
// $urlpro.protocol = { ... };\n\
Object.defineProperty($urlpro, \"protocol\", {\n\
  get: function() {return this.protocol$val; },\n\
  set: function(v) { this.protocol$val = v; this.rebuild(); }\n\
});\n\
\n\
Object.defineProperty($urlpro, \"pathname\", {\n\
  get: function() {return this.pathname$val; },\n\
  set: function(v) { this.pathname$val = v; this.rebuild(); }\n\
});\n\
\n\
Object.defineProperty($urlpro, \"search\", {\n\
  get: function() {return this.search$val; },\n\
  set: function(v) { this.search$val = v; this.rebuild(); }\n\
});\n\
\n\
Object.defineProperty($urlpro, \"hash\", {\n\
  get: function() {return this.hash$val; },\n\
  set: function(v) { this.hash$val = v; this.rebuild(); }\n\
});\n\
\n\
Object.defineProperty($urlpro, \"port\", {\n\
  get: function() {return this.port$val; },\n\
  set: function(v) { this.port$val = v;\n\
if(this.hostname$val.length)\n\
this.host$val = this.hostname$val + \":\" + v;\n\
this.rebuild(); }\n\
});\n\
\n\
Object.defineProperty($urlpro, \"hostname\", {\n\
  get: function() {return this.hostname$val; },\n\
  set: function(v) { this.hostname$val = v;\n\
if(this.port$val)\n\
this.host$val = v + \":\" +  this.port$val;\n\
this.rebuild(); }\n\
});\n\
\n\
Object.defineProperty($urlpro, \"host\", {\n\
  get: function() {return this.host$val; },\n\
  set: function(v) { this.host$val = v;\n\
if(v.match(/:/)) {\n\
this.hostname$val = v.replace(/:.*/, \"\");\n\
this.port$val = v.replace(/^.*:/, \"\");\n\
/* port has to be an integer */\n\
this.port$val = parseInt(this.port$val);\n\
} else {\n\
this.hostname$val = v;\n\
this.port$val = 0;\n\
}\n\
this.rebuild(); }\n\
});\n\
\n\
var prot$port = {\n\
http: 80,\n\
https: 443,\n\
pop3: 110,\n\
pop3s: 995,\n\
imap: 220,\n\
imaps: 993,\n\
smtp: 25,\n\
submission: 587,\n\
smtps: 465,\n\
proxy: 3128,\n\
ftp: 21,\n\
sftp: 22,\n\
scp: 22,\n\
ftps: 990,\n\
tftp: 69,\n\
gopher: 70,\n\
finger: 79,\n\
telnet: 23,\n\
smb: 139\n\
};\n\
\n\
/* returns default port as an integer, based on protocol */\n\
function default$port(p) {\n\
var port = 0;\n\
p = p.toLowerCase().replace(/:/, \"\");\n\
if(prot$port.hasOwnProperty(p))\n\
port = parseInt(prot$port[p]);\n\
return port;\n\
}\n\
\n\
Object.defineProperty($urlpro, \"href\", {\n\
  get: function() {return this.href$val; },\n\
  set: function(v) { this.href$val = v;\n\
// initialize components to empty,\n\
// then fill them in from href if they are present */\n\
this.protocol$val = \"\";\n\
this.hostname$val = \"\";\n\
this.port$val = 0;\n\
this.host$val = \"\";\n\
this.pathname$val = \"\";\n\
this.search$val = \"\";\n\
this.hash$val = \"\";\n\
if(v.match(/^[a-zA-Z]*:/)) {\n\
this.protocol$val = v.replace(/:.*/, \"\");\n\
this.protocol$val += \":\";\n\
v = v.replace(/^[a-zA-z]*:\\/*/, \"\");\n\
}\n\
if(v.match(/[/#?]/)) {\n\
/* contains / ? or # */\n\
this.host$val = v.replace(/[/#?].*/, \"\");\n\
v = v.replace(/^[^/#?]*/, \"\");\n\
} else {\n\
/* no / ? or #, the whole thing is the host, www.foo.bar */\n\
this.host$val = v;\n\
v = \"\";\n\
}\n\
if(this.host$val.match(/:/)) {\n\
this.hostname$val = this.host$val.replace(/:.*/, \"\");\n\
this.port$val = this.host$val.replace(/^.*:/, \"\");\n\
/* port has to be an integer */\n\
this.port$val = parseInt(this.port$val);\n\
} else {\n\
this.hostname$val = this.host$val;\n\
// should we be filling in a default port here?\n\
this.port$val = default$port(this.protocol$val);\n\
}\n\
if(v.match(/[#?]/)) {\n\
this.pathname$val = v.replace(/[#?].*/, \"\");\n\
v = v.replace(/^[^#?]*/, \"\");\n\
} else {\n\
this.pathmname$val = v;\n\
v = \"\";\n\
}\n\
if(this.pathname$val == \"\")\n\
this.pathname$val = \"/\";\n\
if(v.match(/#/)) {\n\
this.search$val = v.replace(/#.*/, \"\");\n\
this.hash$val = v.replace(/^[^#]*/, \"\");\n\
} else {\n\
this.search$val = v;\n\
}\n\
}\n\
});\n\
\n\
URL.prototype.toString = function() { \n\
return this.href$val;\n\
}\n\
URL.prototype.indexOf = function(s) { \n\
return this.toString().indexOf(s);\n\
}\n\
URL.prototype.lastIndexOf = function(s) { \n\
return this.toString().lastIndexOf(s);\n\
}\n\
URL.prototype.substring = function(from, to) { \n\
return this.toString().substring(from, to);\n\
}\n\
URL.prototype.toLowerCase = function() { \n\
return this.toString().toLowerCase();\n\
}\n\
URL.prototype.toUpperCase = function() { \n\
return this.toString().toUpperCase();\n\
}\n\
URL.prototype.match = function(s) { \n\
return this.toString().match(s);\n\
}\n\
URL.prototype.replace = function(s, t) { \n\
return this.toString().replace(s, t);\n\
}\n\
\n\
/*********************************************************************\n\
This is our addEventListener function.\n\
It is bound to window, which is ok because window has such a function\n\
to listen to load and unload.\n\
Later on we will bind it to document and to other elements via\n\
element.addEventListener = addEventListener\n\
Or maybe URL.prototype.addEventListener = addEventListener\n\
to cover all the hyperlinks in one go.\n\
first arg is a string like click, second arg is a js handler,\n\
Third arg is not used cause I don't understand it.\n\
*********************************************************************/\n\
\n\
function addEventListener(ev, handler, notused)\n\
{\n\
ev = \"on\" + ev;\n\
var evarray = ev + \"$$array\"; // array of handlers\n\
var evorig = ev + \"$$orig\"; // original handler from html\n\
if(!this[evarray]) {\n\
/* attaching the first handler */\n\
var a = new Array();\n\
/* was there already a function from before? */\n\
if(this[ev]) {\n\
this[evorig] = this[ev];\n\
this[ev] = undefined;\n\
}\n\
this[evarray] = a;\n\
this[ev] = function(){\n\
var a = this[evarray]; /* should be an array */\n\
if(this[evorig]) this[evorig]();\n\
for(var i = 0; i<a.length; ++i) a[i]();\n\
};\n\
}\n\
this[evarray].push(handler);\n\
}\n\
\n\
/* For grins let's put in the other standard. */\n\
\n\
function attachEvent(ev, handler)\n\
{\n\
var evarray = ev + \"$$array\"; // array of handlers\n\
var evorig = ev + \"$$orig\"; // original handler from html\n\
if(!this[evarray]) {\n\
/* attaching the first handler */\n\
var a = new Array();\n\
/* was there already a function from before? */\n\
if(this[ev]) {\n\
this[evorig] = this[ev];\n\
this[ev] = undefined;\n\
}\n\
this[evarray] = a;\n\
this[ev] = function(){\n\
var a = this[evarray]; /* should be an array */\n\
if(this[evorig]) this[evorig]();\n\
for(var i = 0; i<a.length; ++i) a[i]();\n\
};\n\
}\n\
this[evarray].push(handler);\n\
}\n\
\n\
document.addEventListener = window.addEventListener;\n\
document.attachEvent = window.attachEvent;\n\
Body.prototype.addEventListener = window.addEventListener;\n\
Body.prototype.attachEvent = window.attachEvent;\n\
Form.prototype.addEventListener = window.addEventListener;\n\
Form.prototype.attachEvent = window.attachEvent;\n\
Element.prototype.addEventListener = window.addEventListener;\n\
Element.prototype.attachEvent = window.attachEvent;\n\
Anchor.prototype.addEventListener = window.addEventListener;\n\
Anchor.prototype.attachEvent = window.attachEvent;\n\
\n\
/* an array in an html document uses appendchild like push */\n\
Array.prototype.appendChild = function(child) { this.push(child); }\n\
/* insertBefore maps to splice, but we have to find the element. */\n\
/* This prototype assumes all elements are objects. */\n\
Array.prototype.insertBefore = function(newobj, item) {\n\
for(var i=0; i<this.length; ++i)\n\
if(this[i] == item) {\n\
this.splice(i-1, 0, newobj);\n\
return;\n\
}\n\
}\n\
Array.prototype.firstChild = function() { return (this.length ? this[0] : undefined); }\n\
Array.prototype.lastChild = function() { return (this.length ? this[this.length-1] : undefined); }\n\
\n\
/* Of course most of the html objects are not arrays. */\n\
/* Still they need appendChild and insertBefore */\n\
Body.prototype.appendChild = function(o) { this.$kids$.appendChild(o); }\n\
Body.prototype.insertBefore = function(o, b) { this.$kids$.insertBefore(o, b); }\n\
Body.prototype.firstChild = function() { return this.$kids$.firstChild(); }\n\
Body.prototype.lastChild = function() { return this.$kids$.lastChild(); }\n\
Body.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }\n\
Body.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }\n\
Head.prototype.appendChild = function(o) { this.$kids$.appendChild(o); }\n\
"
"Head.prototype.insertBefore = function(o, b) { this.$kids$.insertBefore(o, b); }\n\
Head.prototype.firstChild = function() { return this.$kids$.firstChild(); }\n\
Head.prototype.lastChild = function() { return this.$kids$.lastChild(); }\n\
Head.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }\n\
Head.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }\n\
Form.prototype.appendChild = function(o) { this.$kids$.appendChild(o); }\n\
Form.prototype.insertBefore = function(o, b) { this.$kids$.insertBefore(o, b); }\n\
Form.prototype.firstChild = function() { return this.$kids$.firstChild(); }\n\
Form.prototype.lastChild = function() { return this.$kids$.lastChild(); }\n\
Form.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }\n\
Form.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }\n\
Element.prototype.appendChild = function(o) { this.$kids$.appendChild(o); }\n\
Element.prototype.insertBefore = function(o, b) { this.$kids$.insertBefore(o, b); }\n\
Element.prototype.firstChild = function() { return this.$kids$.firstChild(); }\n\
Element.prototype.lastChild = function() { return this.$kids$.lastChild(); }\n\
Element.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }\n\
Element.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }\n\
Div.prototype.appendChild = function(o) { this.$kids$.appendChild(o); }\n\
Div.prototype.insertBefore = function(o, b) { this.$kids$.insertBefore(o, b); }\n\
Div.prototype.firstChild = function() { return this.$kids$.firstChild(); }\n\
Div.prototype.lastChild = function() { return this.$kids$.lastChild(); }\n\
Div.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }\n\
Div.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }\n\
Script.prototype.setAttribute = function(name, val) { this[name.toLowerCase()] = val; }\n\
Script.prototype.getAttribute = function(name) { return this[name.toLowerCase()]; }\n\
\n\
/* navigator; some parameters are filled in by the buildstartwindow script. */\n\
navigator.appName = \"edbrowse\";\n\
navigator[\"appCode Name\"] = \"edbrowse C/mozjs\";\n\
/* not sure what product is about */\n\
navigator.product = \"mozjs\";\n\
navigator.productSub = \"2.4\";\n\
navigator.vendor = \"Karl Dahlke\";\n\
/* language is determined in C, at runtime, by $LANG */\n\
navigator.javaEnabled = function() { return false; }\n\
navigator.taintEnabled = function() { return false; }\n\
navigator.cookieEnabled = true;\n\
navigator.onLine = true;\n\
\n\
/* There's no history in edbrowse. */\n\
/* Only the current file is known, hence length is 1. */\n\
history.length = 1;\n\
history.next = \"\";\n\
history.previous = \"\";\n\
history.back = function() { return null; }\n\
history.forward = function() { return null; }\n\
history.go = function() { return null; }\n\
history.toString = function() {\n\
 return \"Sorry, edbrowse does not maintain a browsing history.\";\n\
} \n\
\n\
/* The web console, just the log method for now, and only one arg. */\n\
console = new Object;\n\
console.log = function(obj) {\n\
var today=new Date();\n\
var h=today.getHours();\n\
var m=today.getMinutes();\n\
var s=today.getSeconds();\n\
// add a zero in front of numbers<10\n\
if(h < 10) h = \"0\" + h;\n\
if(m < 10) m = \"0\" + m;\n\
if(s < 10) s = \"0\" + s;\n\
alert(\"[\" + h + \":\" + m + \":\" + s + \"] \" + obj);\n\
}\n\
console.info = console.log;\n\
console.warn = console.log;\n\
console.error = console.log;\n\
\n\
/* My own function to return the next script that has not been run\n\
 * and is ready to run. Easier to do this here than in C. */\n\
document.script$$pending = function() {\n\
for(var i=0; i<document.script$$queue.length; ++i) {\n\
var s = document.script$$queue[i];\n\
if(s.exec$$ed) continue; // already run\n\
if(s.src || s.data) return s;\n\
}\n\
return null;\n\
}\n\
\n\
";

