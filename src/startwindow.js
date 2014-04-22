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
see URL.prototype below.
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

/* some base arrays - lists of things */
frames = new Array;
document.anchors = new Array;
document.links = new Array;
document.applets = new Array;
document.embeds = new Array;
document.tables = new Array;
document.divs = new Array;
document.spans = new Array;
document.forms = new Array;
document.images = new Array;
document.areas = new Array;
document.metas = new Array;
document.scripts = new Array;

document.idMaster = new Object;
document.all = new Object;
document.all.tags = function(s) { 
switch(s.toLowerCase()) { 
case "form": return document.forms; 
case "table": return document.tables; 
case "div": return document.divs; 
case "a": return document.anchors; 
case "link": return document.link; 
case "img": case "image": return document.images; 
case "span": return document.spans; 
case "meta": return document.metas; 
default:
/* Should print an error and bail out, but I want js to march on. */
 /* alert("all.tags default " + s); */
return new Array();
} 
} 

document.getElementById = function(s) { 
/* take advantage of the js hash lookup */
return document.idMaster[s]; 
}

document.getElementsByTagName = function(s) { 
return document.all.tags(s);
}

document.createElement = function(s) { 
var c;
switch(s.toLowerCase()) { 
case "anchor": c = new Anchor(); document.anchors.push(c); return c;
case "image": case "img": c = new Image(); document.images.push(c); return c;
case "script": c = new Script(); document.scripts.push(c); return c;
default:
/* alert("createElement default " + s); */
return new Object();
} 
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

/*********************************************************************
This is our addEventListener function.
It is bound to window, which is ok because window has such a function
to listen to load and unload.
Later on we will bind it to document and to other elements via
element.addEventListener = addEventListener
Or maybe URL.prototype.addEventListener = addEventListener
to cover all the hyperlinks in one go.
All this is very experimental and mostly not activated yet.
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

/* navigator; some parameters are filled in by the buildstartwindow script. */
navigator.appName = "edbrowse";
navigator["appCode Name"] = "edbrowse C/mozjs";
navigator.appVersion = "$ebv";
navigator.userAgent = "edbrowse/$ebv";
navigator.oscpu = "$kname";
navigator.platform = "$plat";
/* not sure what product is about */
navigator.product = "mozjs";
navigator.productSub = "2.4";
navigator.vendor = "Karl Dahlke";
navigator.vendorSub = "$ebv";
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

