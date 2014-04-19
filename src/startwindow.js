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
see URL below.
*********************************************************************/

document.all.tags = function(s) { 
switch(s.toLowerCase()) { 
case "form": return document.forms; 
case "table": return document.tables; 
case "div": return document.divs; 
case "a": return document.links; 
case "img": case "image": return document.images; 
case "span": return document.spans; 
case "head": return document.heads; 
case "meta": return document.metas; 
case "body": return document.bodies; 
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
switch(s.toLowerCase()) { 
case "link": return new Link();
case "image": case "img": return new Image();
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

history.toString = function() { 
return "Sorry, edbrowse does not maintain a browsing history.";
} 

/* This is our addEventListener function. */
/* first arg is a string like click, second arg is a js handler */
/* Third arg is not used, cause I don't understand it. */
function event$$listen(ev, handler, notused)
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

/* to use this, set element.addEventListener = event$$listen
 * Or maybe URL.prototype.addEventListener = event$$listen
 * to cover all the hyperlinks in one go.
 * I don't know, this is very experimental, and not activated yet.
 * For grins let's put in the "other" standard. */

function event$$attach(ev, handler)
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

