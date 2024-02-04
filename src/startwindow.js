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
eb$ctx = 77;
// Stubs for native methods that are normally provided by edbrowse.
// Example: alert, which we can replace with print,
// or console.log, or anything present in the command line js interpreter.
if(!window.print) print = console.log;
alert = print;
eb$nullfunction = function() { return null}
eb$voidfunction = function() { }
eb$truefunction = function() { return true}
eb$falsefunction = function() { return false}
db$flags = eb$falsefunction;
eb$newLocation = function (s) { print("new location " + s)}
eb$logElement = function(o, tag) { print("pass tag " + tag + " to edbrowse")}
eb$playAudio = eb$voidfunction;
eb$getcook = function() { return "cookies"}
eb$setcook = function(value) { print(" new cookie " + value)}
eb$parent = function() { return this}
eb$top = function() { return this}
eb$frameElement = function() { return this}
eb$getter_cd = function() { return null}
eb$getter_cw = function() { return null}
eb$formSubmit = function() { print("submit")}
eb$formReset = function() { print("reset")}
eb$listen = eb$unlisten = addEventListener = removeEventListener = eb$voidfunction;
my$win = function() { return window}
my$doc = function() { return document}
eb$hasFocus = eb$write = eb$writeln = eb$apch1 = eb$apch2 = eb$rmch2 = eb$insbf = eb$voidfunction;
// document.eb$apch2 = function(c) { alert("append " + c.nodeName  + " to " + this.nodeName); this.childNodes.push(c); }
// other browsers don't have querySelectorAll under window
querySelectorAll = function() { return [] }
querySelector = function() { return {} }
querySelector0 = function() { return false}
eb$cssText = function(){}
}

// the third party deminimization stuff is in mw$, the master window.
// Other stuff too, that can be shared.
// The window should just be there from C, but in case it isn't.
if(!window.mw$)
mw$ = {share:false, URL:{}};

// set window member, unseen, unchanging
swm = function(k, v) { Object.defineProperty(window, k, {value:v})}
// visible, but still protected
swm1 = function(k, v) { Object.defineProperty(window, k, {value:v,enumerable:true})}
// unseen, but changeable
swm2 = function(k, v) { Object.defineProperty(window, k, {value:v, writable:true, configurable:true})}

// this is called as each html element is built
// establish the prototype for inheritance, then set dom$class
// If our made-up class is z$Foo, dom$class becomes Foo
spdc = function(c, inherit) { var v = c.replace(/^z\$/, "");
if(inherit) Object.defineProperty(window[c], "prototype", {value:new inherit})
Object.defineProperty(window[c].prototype, "dom$class", {value:v})}

// The first DOM class is Node, at the head of all else.
swm("Node", function(){})
spdc("Node", null)

// a node list is and isn't an array; I don't really understand it.
// I'll just have it inherit from array, until someone tells me I'm wrong.
swm("NodeList", function(){})
spdc("NodeList", Array)

swm("eb$listen", mw$.eb$listen)
swm("eb$unlisten", mw$.eb$unlisten)
// make sure to wrap global dispatchEvent, so this becomes this window,
// and not the shared window.
swm("dispatchEvent", function(e) { return mw$.dispatchEvent.call(window, e)})
swm("addEventListener", function(ev, handler, iscapture) { this.eb$listen(ev,handler, iscapture, true)})
swm("removeEventListener", function(ev, handler, iscapture) { this.eb$unlisten(ev,handler, iscapture, true)})

swm("EventTarget", function() {})
spdc("EventTarget", Node)
EventTarget.prototype.eb$listen = eb$listen;
EventTarget.prototype.eb$unlisten = eb$unlisten;
EventTarget.prototype.addEventListener = addEventListener;
EventTarget.prototype.removeEventListener = removeEventListener;
EventTarget.prototype.dispatchEvent = mw$.dispatchEvent;

swm("Document", function(){this.children=[]})
spdc("Document", EventTarget)
Document.prototype.activeElement = null;
Object.defineProperty(Document.prototype, "childElementCount", {get:function(){return this.children.length}})
Object.defineProperty(Document.prototype, "firstElementChild", {get:function(){return this.children.length?this.children[0]:null}})
Document.prototype.querySelector = querySelector;
Document.prototype.querySelectorAll = querySelectorAll;

swm1("document", new Document)

// set document member, analogs of the set window member functions
sdm = function(k, v) { Object.defineProperty(document, k, {value:v})}
sdm1 = function(k, v) { Object.defineProperty(document, k, {value:v,enumerable:true})}
sdm2 = function(k, v) { Object.defineProperty(document, k, {value:v, writable:true, configurable:true})}

if(mw$.share) { // point to native methods in the master window
swm("my$win", mw$.my$win)
swm("my$doc", mw$.my$doc)
swm("natok", mw$.natok)
swm("db$flags", mw$.db$flags)
swm("eb$voidfunction", mw$.eb$voidfunction)
swm("eb$nullfunction", mw$.eb$nullfunction)
swm("eb$truefunction", mw$.eb$truefunction)
swm("eb$falsefunction", mw$.eb$falsefunction)
swm1("close", mw$.win$close)
swm("eb$visible", mw$.eb$visible)
swm1("atob", mw$.atob)
swm1("btoa", mw$.btoa)
swm1("prompt", mw$.prompt)
swm1("confirm", mw$.confirm)
swm("eb$newLocation", mw$.eb$newLocation)
swm("eb$logElement", mw$.eb$logElement)
swm1("alert", mw$.alert)
swm("alert3", mw$.alert3)
swm("alert4", mw$.alert4)
print = function() { alert("javascript is trying to print this document")}
stop = function() { alert("javascript is trying to stop the browse process")}
swm("dumptree", mw$.dumptree)
swm("uptrace", mw$.uptrace)
swm("by_esn", mw$.by_esn)
swm("showscripts", mw$.showscripts)
swm("searchscripts", mw$.searchscripts)
swm("showframes", mw$.showframes)
swm("snapshot", mw$.snapshot)
swm("aloop", mw$.aloop)
swm("showarg", mw$.showarg)
swm("showarglist", mw$.showarglist)
swm("set_location_hash", mw$.set_location_hash)
sdm("getRootNode", mw$.getRootNode)
sdm("getElementsByTagName", mw$.getElementsByTagName)
sdm("getElementsByName", mw$.getElementsByName)
sdm("getElementsByClassName", mw$.getElementsByClassName)
sdm("getElementById", mw$.getElementById)
sdm("nodeContains", mw$.nodeContains)
sdm("dispatchEvent", mw$.dispatchEvent)
swm("NodeFilter", mw$.NodeFilter)
sdm2("createNodeIterator", mw$.createNodeIterator)
sdm2("createTreeWalker", mw$.createTreeWalker)
swm("rowReindex", mw$.rowReindex)
swm1("getComputedStyle", mw$.getComputedStyle.bind(window))
swm("mutFixup", mw$.mutFixup)
swm("makeSheets", mw$.makeSheets)
}

swm("dom$class", "Window")
sdm("dom$class", "HTMLDocument")
// use dom$class to make our own toString function, so that
// document.createElement("div").toString() says "[object HTMLDiv?Element]" as it should
// This is important to some websites!
swm("toString$nat", toString);
// toString has to be replaceable by other websites,
// this happens more often than you think.
toString = Object.prototype.toString = function() { return this.dom$class ? "[object "+this.dom$class+"]" : toString$nat.call(this);}
Object.defineProperty(window, "toString", {enumerable:false})

swm1("scroll", eb$voidfunction)
swm1("scrollTo", eb$voidfunction)
swm1("scrollBy", eb$voidfunction)
swm1("scrollByLines", eb$voidfunction)
swm1("scrollByPages", eb$voidfunction)
sdm("close", eb$voidfunction)
sdm("blur", function(){document.activeElement=null})
sdm("focus", function(){document.activeElement=document.body})
swm1("blur", document.blur)
swm1("focus", document.focus)

swm1("self", window)
Object.defineProperty(window, "parent", {get: eb$parent,enumerable:true});
Object.defineProperty(window, "top", {get: eb$top,enumerable:true});
Object.defineProperty(window, "frameElement", {get: eb$frameElement,enumerable:true});

sdm("write", eb$write)
sdm("writeln", eb$writeln)
sdm("hasFocus", eb$hasFocus)
sdm("eb$apch1", eb$apch1)
sdm("eb$apch2", eb$apch2)
sdm("eb$insbf", eb$insbf)
sdm("eb$rmch2", eb$rmch2)
sdm("eb$ctx", eb$ctx)
sdm("eb$seqno", 0)

/* An ok (object keys) function for javascript/dom debugging.
 * This is in concert with the jdb command in edbrowse.
 * I know, it doesn't start with eb$ but I wanted an easy,
 * mnemonic command that I could type in quickly.
 * If a web page creates an ok function it will override this one.
And that does happen, e.g. the react system, so $ok is an alias for this. */
swm2("ok", Object.keys)
swm2("$ok", ok)

swm("nodeName", "WINDOW") // in case you want to start at the top.
sdm("nodeName", "#document") // in case you want to start at document.
sdm("tagName", "document")
sdm("nodeType", 9)
sdm2("ownerDocument", null)
sdm("defaultView", window)

// produce a stack for debugging purposes
swm("step$stack", function(){
var s = "you shouldn't see this";
try { 'use strict'; eval("yyz$"); } catch(e) { s = e.stack; }
// Lop off some leading lines that don't mean anything.
for(var i = 0; i<5; ++i)
s = s.replace(/^.*\n/, "");
return s;
})

if(top == window) {
swm2("step$l", 0)
swm2("step$go", "")
// First line of js in the base file of your snapshot might be
// step$l = 0, step$go = "c275";
// to start tracing at c275
} else {
// step$l should control the entire session, all frames.
// This is a trick to have a global variable across all frames.
Object.defineProperty(window, "step$l", {get:function(){return top.step$l}, set:function(x){top.step$l=x}});
Object.defineProperty(window, "step$go", {get:function(){return top.step$go}, set:function(x){top.step$go=x}});
// I don't use this trick on step$exp, because an expression should really live within its frame
}

swm("$zct", {}) // counters for trace points

sdm("open", function() { return this })

/* Some visual attributes of the window.
 * These are simulations as edbrowse has no screen.
 * Better to have something than nothing at all. */
swm("height", 768)
swm("width", 1024)
swm1("pageXOffset", 0)
swm1("scrollX", 0)
swm1("pageYOffset", 0)
swm1("scrollY", 0)
swm1("devicePixelRatio", 1.0)
// document.status is removed because it creates a conflict with
// the status property of the XMLHttpRequest implementation
swm("defaultStatus", 0)
swm("returnValue", true)
swm1("menubar", mw$.generalbar)
swm1("statusbar", mw$.generalbar)
swm1("scrollbars", mw$.generalbar)
swm1("toolbar", mw$.generalbar)
swm1("personalbar", mw$.generalbar)
swm("resizable", true)
swm("directories", false)
if(window == top) {
swm1("name", "unspecifiedFrame")
} else {
Object.defineProperty(window, "name", {get:function(){return frameElement.name}});
// there is no setter here, should there be? Can we set name to something?
// Should it propagate back up to the frame element name?
}

sdm("bgcolor", "white")
sdm("contentType", "text/html")
sdm("visibilityState", "visible")
sdm2("readyState", "interactive");
function readyStateComplete() { document.readyState = "complete"; document.activeElement = document.body;
if(document.onreadystatechange$$fn) {
var e = new Event;
e.initEvent("readystatechange", true, true);
e.target = e.currentTarget = document;
e.eventPhase = 2;
document.onreadystatechange$$fn(e);
}
}

swm1("screen", {
height: 768, width: 1024,
availHeight: 768, availWidth: 1024, availTop: 0, availLeft: 0,
colorDepth: 24})

swm("console", {
debug: function(obj) { mw$.logtime(3, "debug", obj)},
log: function(obj) { mw$.logtime(3, "log", obj)},
info: function(obj) { mw$.logtime(3, "info", obj)},
warn: function(obj) { mw$.logtime(3, "warn", obj)},
error: function(obj) { mw$.logtime(3, "error", obj)},
timeStamp: function(label) { if(label === undefined) label = "x"; return label.toString() + (new Date).getTime(); }
})

Object.defineProperty(document, "cookie", {
get: eb$getcook, set: eb$setcook});

Object.defineProperty(document, "documentElement", {get: mw$.getElement});
Object.defineProperty(document, "head", {get: mw$.getHead,set:mw$.setHead});
Object.defineProperty(document, "body", {get: mw$.getBody,set:mw$.setBody});
// scrollingElement makes no sense in edbrowse, I think body is our best bet
Object.defineProperty(document, "scrollingElement", {get: mw$.getBody});
// document should always have children, but...
sdm("hasChildNodes", mw$.hasChildNodes)
// This is set to body after browse.
sdm2("activeElement", null)

swm1("navigator", {})
navigator.appName = "edbrowse";
navigator["appCode Name"] = "edbrowse C/quickjs";
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

// There's no history in edbrowse.
// Only the current file is known, hence length is 1.
swm1("history", {
length: 1,
next: "",
previous: "",
back: eb$voidfunction,
forward: eb$voidfunction,
go: eb$voidfunction,
pushState: eb$voidfunction,
replaceState: eb$voidfunction,
toString: function() {  return "Sorry, edbrowse does not maintain a browsing history."}
})

// some base arrays - lists of things we'll probably need
sdm("heads", [])
sdm("bases", [])
sdm("links", [])
sdm("metas", [])
sdm("styles", [])
sdm("bodies", [])
sdm("forms", [])
sdm("elements", [])
sdm("divs", [])
sdm("labels", [])
sdm("htmlobjs", [])
sdm("scripts", [])
sdm("paragraphs", [])
sdm("headers", [])
sdm("footers", [])
sdm("tables", [])
sdm("spans", [])
sdm("images", [])
// styleSheets is a placeholder for now; I don't know what to do with it.
sdm("styleSheets", [])

swm2("frames$2", []);
swm1("frames", {})
Object.defineProperty(frames, "length", {get:function(){return frames$2.length}})
Object.defineProperty(window, "length", {get:function(){return frames$2.length},enumerable:true})

// to debug a.href = object or other weird things.
swm("hrefset$p", [])
swm("hrefset$a", [])
// pending jobs, mostly to debug promise functions.
swm("$pjobs", [])

// symbolic constants for compareDocumentPosition
swm("DOCUMENT_POSITION_DISCONNECTED", 1)
swm("DOCUMENT_POSITION_PRECEDING", 2)
swm("DOCUMENT_POSITION_FOLLOWING", 4)
swm("DOCUMENT_POSITION_CONTAINS", 8)
swm("DOCUMENT_POSITION_CONTAINED_BY", 16)
sdm("compareDocumentPosition", mw$.compareDocumentPosition)

// This should be in the native string class
String.prototype.at = function(n) {
if(typeof n != "number") return undefined;
var l = this.length;
if(n >= 0) {
if(n >= l) return undefined;
return this.charAt(n);
}
n = -n;
if(n > l) return undefined;
return this.charAt(l-n);
}

/*********************************************************************
The URL class is head-spinning in its complexity and its side effects.
Almost all of these can be handled in JS,
except for setting window.location or document.location to a new url,
which replaces the web page you are looking at.
You'll see me call the native method eb$newLocation to do that.
Now, here's one reason, perhaps not the only reason, we can't share the
URL class. Why it has to stay here in startwindow.js.
This may apply to every other DOM class as well.
There are websites that replace URL.prototype.toString with their own function.
They want to change the way URLs stringify, or whatever. I can't
prevent sites from doing that, things might not work properly without it!
So, if site A does that in the shared window, and site B invokes
a.href.toString, directly or indirectly, B is calling a function from
the unrelated website A.
This could really screw things up, or worse, site A could use it to hack into
site B, hoping site B is your banking site or something important.
So I can't define URL over there and say URL = mw$.url over here.
Ok, but what if I say URL = function(){}; URL.prototype = new mw$.URL;
That puts all those methods and getters and setters,
and there are a lot of them, in the prototype chain.
Course I have to lock them down in the shared window, as described above.
They all have to be readonly.
Well, If this window wants to create its own URL.prototype.toString function,
it can't, because the toString that is next in the prototype chain is readonly.
I don't know if this is javascript standard or quick js.
It sort of makes sense if you think about it, but it means
I don't have any practical way to share this class. So here we go.
Note the use of swm2, people will replace with their own URL class.
If they do, I remember this URL class and all its methods with z$URL.
I don't know if that's a good idea or not.
*********************************************************************/

swm2("URL", function() {
var h = "";
if(arguments.length == 1) h= arguments[0];
if(arguments.length == 2) h= mw$.resolveURL(arguments[1], arguments[0]);
this.href = h;
})
swm("z$URL", URL)
spdc("URL", null)
Object.defineProperty(URL.prototype, "rebuild", {value:mw$.url_rebuild})

Object.defineProperty(URL.prototype, "protocol", {
  get: function() {return this.protocol$val; },
  set: function(v) { this.protocol$val = v; this.rebuild(); },
enumerable:true});
Object.defineProperty(URL.prototype, "pathname", {
  get: function() {return this.pathname$val; },
  set: function(v) { this.pathname$val = v; this.rebuild(); },
enumerable:true});
Object.defineProperty(URL.prototype, "search", {
  get: function() {return this.search$val; },
  set: function(v) { this.search$val = v; this.rebuild(); },
enumerable:true});
Object.defineProperty(URL.prototype, "searchParams", {
  get: function() {return new URLSearchParams(this.search$val); },
// is there a setter?
enumerable:true});
Object.defineProperty(URL.prototype, "hash", {
  get: function() {return this.hash$val; },
  set: function(v) { if(typeof v != "string") return; if(!v.match(/^#/)) v = '#'+v; this.hash$val = v; this.rebuild(); },
enumerable:true});
Object.defineProperty(URL.prototype, "port", {
  get: function() {return this.port$val; },
  set: function(v) { this.port$val = v;
if(this.hostname$val.length)
this.host$val = this.hostname$val + ":" + v;
this.rebuild(); },
enumerable:true});
Object.defineProperty(URL.prototype, "hostname", {
  get: function() {return this.hostname$val; },
  set: function(v) { this.hostname$val = v;
if(this.port$val)
this.host$val = v + ":" +  this.port$val;
this.rebuild(); },
enumerable:true});
Object.defineProperty(URL.prototype, "host", {
  get: function() {return this.host$val; },
  set: function(v) { this.host$val = v;
if(v.match(/:/)) {
this.hostname$val = v.replace(/:.*/, "");
this.port$val = v.replace(/^.*:/, "");
} else {
this.hostname$val = v;
this.port$val = "";
}
this.rebuild(); },
enumerable:true});
Object.defineProperty(URL.prototype, "href", {
  get: function() {return this.href$val; },
  set: mw$.url_hrefset,
enumerable:true});

URL.prototype.toString = function() {  return this.href$val; }
Object.defineProperty(URL.prototype, "toString", {enumerable:false});
// use toString in the following - in case they replace toString with their own function.
// Don't just grab href$val, tempting as that is.
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
Can't turn URL.search into String.search, because search is already a
property of URL, that is, the search portion of the URL.
URL.prototype.search = function(s) { return this.toString().search(s); }
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

// Is Element a synonym for HTMLElement? nasa.gov acts like it is.
swm2("HTMLElement", function(){})
swm2("Element", HTMLElement)
spdc("HTMLElement", Node)
Object.defineProperty(HTMLElement.prototype, "name", {
get: function() {
var isinput = (this.dom$class == "HTMLInputElement" || this.dom$class == "HTMLButtonElement" || this.dom$class == "HTMLSelectElement");
if(!isinput) return this.name$2 ;
// name property is automatically in the getAttribute system, acid test 53
var t = this.getAttribute("name");
return typeof t == "string" ? t : undefined}, 
set: function(n) {
var isinput = (this.dom$class == "HTMLInputElement" || this.dom$class == "HTMLButtonElement" || this.dom$class == "HTMLSelectElement");
if(!isinput) { this.name$2 = n; return; }
var f = this.form;
if(f && f.dom$class == "HTMLFormElement") {
var oldname = this.getAttribute("name");
if(oldname && f[oldname] == this) delete f[oldname];
if(oldname && f.elements[oldname] == this) delete f.elements[oldname];
if(!f[n]) f[n] = this;
if(!f.elements[n]) f.elements[n] = this;
}
this.setAttribute("name", n);
}});
swm("id$hash", {})
Object.defineProperty(HTMLElement.prototype, "id", {
get:function(){ var t = this.getAttribute("id");
return typeof t == "string" ? t : undefined; },
set:function(v) { this.setAttribute("id", v)}});
Object.defineProperty(HTMLElement.prototype, "title", {
get:function(){ var t = this.getAttribute("title");
// in the real world this is always a string, but acid test 3 has numbers for titles
var y = typeof t;
return y == "string" || y == "number" ? t : undefined; },
set:function(v) { this.setAttribute("title", v);}});
// almost anything can be disabled, an entire div section, etc
Object.defineProperty(HTMLElement.prototype, "disabled", {
get:function(){ var t = this.getAttribute("disabled");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("disabled", v);}});
Object.defineProperty(HTMLElement.prototype, "hidden", {
get:function(){ var t = this.getAttribute("hidden");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("hidden", v);}});
HTMLElement.prototype.ownerDocument = document;
HTMLElement.prototype.nodeType = 1;
HTMLElement.prototype.attachShadow = function(o){
// I should have a list of allowed tags here, but custom tags are allowed,
// and I don't know how to determine that,
// so I'll just reject a few tags.
var nn = this.nodeName;
if(nn == "A" || nn == "FRAME" || nn == "IFRAME" | nn == "#document" || nn == "#text" || nn == "#comment" ||
nn == "TABLE" || nn == "TH" || nn == "TD" || nn == "TR" || nn == "FORM" || nn == "INPUT" ||
nn == "SHADOWROOT") // no shadow root within a shadow root
return null;
var r = document.createElement("ShadowRoot");
this.appendChild(r); // are we suppose to do this?
r.mode = "open";
r.delegatesFocus = false;
r.slotAssignment = "";
if(typeof o == "object") {
if(o.mode) r.mode = o.mode;
if(o.delegatesFocus) r.delegatesFocus = o.delegatesFocus;
if(o.slotAssignment) r.slotAssignment = o.slotAssignment;
}
return r;
}
Object.defineProperty(HTMLElement.prototype, "shadowRoot", {
get:function(){
var r = this.firstChild;
if(r && r.nodeName == "SHADOWROOT" && r.mode == "open") return r;
return null;
}});

swm("z$HTML", function(){})
spdc("z$HTML", HTMLElement)
Object.defineProperty(z$HTML.prototype, "eb$win", {get: function(){return this.parentNode ? this.parentNode.defaultView : undefined}});
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
swm("z$DocType", function(){ this.nodeType = 10, this.nodeName = "DOCTYPE"})
spdc("z$DocType", HTMLElement)
swm("DocumentType", function(){})
spdc("DocumentType", HTMLElement)
swm("CharacterData", function(){})
spdc("CharacterData", null)
swm("HTMLHeadElement", function(){})
spdc("HTMLHeadElement", HTMLElement)
swm("HTMLMetaElement", function(){})
spdc("HTMLMetaElement", HTMLElement)
swm("z$Title", function(){})
spdc("z$Title", HTMLElement)
Object.defineProperty(z$Title.prototype, "text", {
get: function(){ return this.firstChild && this.firstChild.nodeName == "#text" && this.firstChild.data || "";}
// setter should change the title of the document, not yet implemented
});
swm("HTMLLinkElement", function(){})
spdc("HTMLLinkElement", HTMLElement)
// It's a list but why would it ever be more than one?
Object.defineProperty(HTMLLinkElement.prototype, "relList", {
get: function() { var a = this.rel ? [this.rel] : [];
// edbrowse only supports stylesheet
a.supports = function(s) { return s === "stylesheet"; }
return a;
}});

swm("HTMLBodyElement", function(){})
spdc("HTMLBodyElement", HTMLElement)
HTMLBodyElement.prototype.doScroll = eb$voidfunction;
HTMLBodyElement.prototype.clientHeight = 768;
HTMLBodyElement.prototype.clientWidth = 1024;
HTMLBodyElement.prototype.offsetHeight = 768;
HTMLBodyElement.prototype.offsetWidth = 1024;
HTMLBodyElement.prototype.scrollHeight = 768;
HTMLBodyElement.prototype.scrollWidth = 1024;
HTMLBodyElement.prototype.scrollTop = 0;
HTMLBodyElement.prototype.scrollLeft = 0;
// document.body.innerHTML =
HTMLBodyElement.prototype.eb$dbih = function(s){this.innerHTML = s}

swm("ShadowRoot", function(){})
spdc("ShadowRoot", HTMLElement)

swm("HTMLBaseElement", function(){})
spdc("HTMLBaseElement", HTMLElement)

swm("HTMLFormElement", function(){this.elements = []})
spdc("HTMLFormElement", HTMLElement)
HTMLFormElement.prototype.submit = eb$formSubmit;
HTMLFormElement.prototype.reset = eb$formReset;
Object.defineProperty(HTMLFormElement.prototype, "length", { get: function() { return this.elements.length;}});

swm("Validity", function(){})
spdc("Validity", null)
/*********************************************************************
All these should be getters, or should they?
Consider the tooLong attribute.
tooLong could compare the length of the input with the maxLength attribute,
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

swm("textarea$html$crossover", function(t) {
if(!t || t.dom$class != "HTMLElement" || t.type != "textarea")
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
})

swm("HTMLSelectElement", function() { this.selectedIndex = -1; this.value = ""; this.selectedOptions=[]; this.options=[];this.validity = new Validity, this.validity.owner = this})
spdc("HTMLSelectElement", HTMLElement)
Object.defineProperty(HTMLSelectElement.prototype, "value", {
get: function() {
var a = this.options;
var n = this.selectedIndex;
return (this.multiple || n < 0 || n >= a.length) ? "" : a[n].value;
}});
Object.defineProperty(HTMLSelectElement.prototype, "type", {
get:function(){ return this.multiple ? "select-multiple" : "select-one"}});
Object.defineProperty(HTMLSelectElement.prototype, "multiple", {
get:function(){ var t = this.getAttribute("multiple");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("multiple", v);}});
Object.defineProperty(HTMLSelectElement.prototype, "size", {
get:function(){ var t = this.getAttribute("size");
if(typeof t == "number") return t;
return this.multiple ? 4 : 1;},
set:function(v) { this.setAttribute("size", v);}});
Object.defineProperty(HTMLSelectElement.prototype, "required", {
get:function(){ var t = this.getAttribute("required");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("required", v);}});

HTMLSelectElement.prototype.eb$bso = function() { // build selected options array
// do not replace the array with a new one, this is suppose to be a live array
var a = this.selectedOptions;
var o = this.options;
a.length = o.length = 0;
var cn = this.childNodes;
for(var i=0; i<cn.length; ++i) {
if(cn[i].nodeName == "OPTION") {
o.push(cn[i]);
if(cn[i].selected) a.push(cn[i]);
}
if(cn[i].nodeName != "OPTGROUP") continue;
var og = cn[i];
var cn2 = og.childNodes;
for(var j=0; j<cn2.length; ++j)
if(cn2[j].nodeName == "OPTION") {
o.push(cn2[j]);
if(cn2[j].selected) a.push(cn2[j]);
}
}
}

swm("HTMLInputElement", function(){this.validity = new Validity, this.validity.owner = this})
spdc("HTMLInputElement", HTMLElement)
HTMLInputElement.prototype.selectionStart = 0;
HTMLInputElement.prototype.selectionEnd = -1;
HTMLInputElement.prototype.selectionDirection = "none";
// I really don't know what this function does, something visual I think.
HTMLInputElement.prototype.setSelectionRange = function(s, e, dir) {
if(typeof s == "number") this.selectionStart = s;
if(typeof e == "number") this.selectionEnd = e;
if(typeof dir == "string") this.selectionDirection = dir;
}
HTMLInputElement.prototype.select = eb$voidfunction;
HTMLInputElement.prototype.click = mw$.clickfn;
// We only need this in the rare case of setting click and clearing
// the other radio buttons. acid test 43
Object.defineProperty(HTMLInputElement.prototype, "checked", {
get: function() { return this.checked$2 ? true : false; },
set: mw$.checkset});
// type property is automatically in the getAttribute system, acid test 53
Object.defineProperty(HTMLInputElement.prototype, "type", {
get:function(){ var t = this.getAttribute("type");
// input type is special, tidy converts it to lower case, so I will too.
// Also acid test 54 requires it.
return typeof t == "string" ? this.eb$xml ? t : t.toLowerCase() : undefined; },
set:function(v) { this.setAttribute("type", v);
if(v.toLowerCase() == "checkbox" && !this.value) this.value = "on";
}});
Object.defineProperty(HTMLInputElement.prototype, "placeholder", {
get:function(){ var t = this.getAttribute("placeholder");
var y = typeof t;
return y == "string" || y == "number" ? t : ""; },
set:function(v) { this.setAttribute("placeholder", v);}});
Object.defineProperty(HTMLInputElement.prototype, "multiple", {
get:function(){ var t = this.getAttribute("multiple");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("multiple", v);}});
Object.defineProperty(HTMLInputElement.prototype, "required", {
get:function(){ var t = this.getAttribute("required");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("required", v);}});
Object.defineProperty(HTMLInputElement.prototype, "readOnly", {
get:function(){ var t = this.getAttribute("readonly");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("readonly", v);}});
Object.defineProperty(HTMLInputElement.prototype, "step", {
get:function(){ var t = this.getAttribute("step");
var y = typeof t;
return y == "number" || y == "string" ? t : undefined},
set:function(v) { this.setAttribute("step", v);}});
Object.defineProperty(HTMLInputElement.prototype, "minLength", {
get:function(){ var t = this.getAttribute("minlength");
var y = typeof t;
return y == "number" || y == "string" ? t : undefined},
set:function(v) { this.setAttribute("minlength", v);}});
Object.defineProperty(HTMLInputElement.prototype, "maxLength", {
get:function(){ var t = this.getAttribute("maxlength");
var y = typeof t;
return y == "number" || y == "string" ? t : undefined},
set:function(v) { this.setAttribute("maxlength", v);}});
Object.defineProperty(HTMLInputElement.prototype, "size", {
get:function(){ var t = this.getAttribute("size");
var y = typeof t;
return y == "number" || y == "string" ? t : undefined},
set:function(v) { this.setAttribute("size", v);}});

swm("HTMLButtonElement", function(){})
spdc("HTMLButtonElement", HTMLElement)
HTMLButtonElement.prototype.click = mw$.clickfn;
// type property is automatically in the getAttribute system, acid test 59
Object.defineProperty(HTMLButtonElement.prototype, "type", {
get:function(){ var t = this.getAttribute("type");
// default is submit, acid test 59
return typeof t == "string" ? t.toLowerCase() : "submit"; },
set:function(v) { this.setAttribute("type", v);}});

swm("HTMLTextAreaElement", function(){})
spdc("HTMLTextAreaElement", HTMLElement)
Object.defineProperty(HTMLTextAreaElement.prototype, "innerText", {
get: function() { return this.value},
set: function(t) { this.value = t }});
Object.defineProperty(HTMLTextAreaElement.prototype, "type", {
get: function() { return "textarea"}});
Object.defineProperty(HTMLTextAreaElement.prototype, "placeholder", {
get:function(){ var t = this.getAttribute("placeholder");
var y = typeof t;
return y == "string" || y == "number" ? t : ""; },
set:function(v) { this.setAttribute("placeholder", v);}});
Object.defineProperty(HTMLTextAreaElement.prototype, "required", {
get:function(){ var t = this.getAttribute("required");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("required", v);}});
Object.defineProperty(HTMLTextAreaElement.prototype, "readOnly", {
get:function(){ var t = this.getAttribute("readonly");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("readonly", v);}});

swm("z$Datalist", function() {})
spdc("z$Datalist", HTMLElement)
Object.defineProperty(z$Datalist.prototype, "multiple", {
get:function(){ var t = this.getAttribute("multiple");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("multiple", v);}});

swm("HTMLImageElement", function(){})
swm2("Image", HTMLImageElement)
spdc("HTMLImageElement", HTMLElement)
Object.defineProperty(HTMLImageElement.prototype, "alt", {
get:function(){ var t = this.getAttribute("alt");
return typeof t == "string" ? t : undefined},
set:function(v) { this.setAttribute("alt", v);
}});

swm("HTMLFrameElement", function(){})
spdc("HTMLFrameElement", HTMLElement)
HTMLFrameElement.prototype.is$frame = true;
Object.defineProperty(HTMLFrameElement.prototype, "contentDocument", { get: eb$getter_cd});
Object.defineProperty(HTMLFrameElement.prototype, "contentWindow", { get: eb$getter_cw});
// These may be different but for now I'm calling them the same.
swm("HTMLIFrameElement", function(){})
spdc("HTMLIFrameElement", HTMLFrameElement)

swm("HTMLAnchorElement", function(){})
spdc("HTMLAnchorElement", HTMLElement)
swm("HTMLOListElement", function(){})
spdc("HTMLOListElement", HTMLElement)
swm("HTMLUListElement", function(){})
spdc("HTMLUListElement", HTMLElement)
swm("HTMLDListElement", function(){})
spdc("HTMLDListElement", HTMLElement)
swm("HTMLLIElement", function(){})
spdc("HTMLLIElement", HTMLElement)

swm("HTMLTableSectionElement", function(){})
spdc("HTMLTableSectionElement", HTMLElement)
swm("z$tBody", function(){ this.rows = []})
spdc("z$tBody", HTMLTableSectionElement)
swm("z$tHead", function(){ this.rows = []})
spdc("z$tHead", HTMLTableSectionElement)
swm("z$tFoot", function(){ this.rows = []})
spdc("z$tFoot", HTMLTableSectionElement)

swm("z$tCap", function(){})
spdc("z$tCap", HTMLElement)
swm("HTMLTableElement", function(){ this.rows = []; this.tBodies = []})
spdc("HTMLTableElement", HTMLElement)
swm("HTMLTableRowElement", function(){ this.cells = []})
spdc("HTMLTableRowElement", HTMLElement)
swm("HTMLTableCellElement", function(){})
spdc("HTMLTableCellElement", HTMLElement)
swm("HTMLDivElement", function(){})
spdc("HTMLDivElement", HTMLElement)
HTMLDivElement.prototype.doScroll = eb$voidfunction;
HTMLDivElement.prototype.align = "left";
HTMLDivElement.prototype.click = function() {
// as though the user had clicked on this
var e = new Event;
e.initEvent("click", true, true);
this.dispatchEvent(e);
}

swm("HTMLLabelElement", function(){})
spdc("HTMLLabelElement", HTMLElement)
Object.defineProperty(HTMLLabelElement.prototype, "htmlFor", { get: function() { return this.getAttribute("for"); }, set: function(h) { this.setAttribute("for", h); }});
swm("HTMLUnknownElement", function(){})
spdc("HTMLUnknownElement", HTMLElement)
swm("HTMLObjectElement", function(){})
spdc("HTMLObjectElement", HTMLElement)
swm("HTMLAreaElement", function(){})
spdc("HTMLAreaElement", HTMLElement)

swm("HTMLSpanElement", function(){})
spdc("HTMLSpanElement", HTMLElement)
HTMLSpanElement.prototype.doScroll = eb$voidfunction;
// should this click be on HTMLElement?
HTMLSpanElement.prototype.click = HTMLDivElement.prototype.click;

swm("HTMLParagraphElement", function(){})
spdc("HTMLParagraphElement", HTMLElement)

swm("HTMLHeadingElement", function(){})
spdc("HTMLHeadingElement", HTMLElement)
swm("z$Header", function(){})
spdc("z$Header", HTMLElement)
swm("z$Footer", function(){})
spdc("z$Footer", HTMLElement)
swm("HTMLScriptElement", function(){})
spdc("HTMLScriptElement", HTMLElement)
Object.defineProperty(HTMLScriptElement.prototype, "async", {
get:function(){ var t = this.getAttribute("async");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("async", v);}});
Object.defineProperty(HTMLScriptElement.prototype, "defer", {
get:function(){ var t = this.getAttribute("defer");
return t === null || t === false || t === "false" || t === 0 || t === '0' ? false : true},
set:function(v) { this.setAttribute("defer", v);}});
HTMLScriptElement.prototype.type = "";
HTMLScriptElement.prototype.text = "";

swm("z$Timer", function(){this.nodeName = "TIMER"})
spdc("z$Timer", null)
swm("HTMLMediaElement", function(){})
spdc("HTMLMediaElement", HTMLElement)
HTMLMediaElement.prototype.autoplay = false;
HTMLMediaElement.prototype.muted = false;
HTMLMediaElement.prototype.defaultMuted = false;
HTMLMediaElement.prototype.paused = false;
HTMLMediaElement.prototype.audioTracks = [];
HTMLMediaElement.prototype.videoTracks = [];
HTMLMediaElement.prototype.textTracks = [];
HTMLMediaElement.prototype.controls = false;
HTMLMediaElement.prototype.controller = null;
HTMLMediaElement.prototype.volume = 1.0;
HTMLMediaElement.prototype.play = eb$playAudio;
HTMLMediaElement.prototype.load = eb$voidfunction;
HTMLMediaElement.prototype.pause = eb$voidfunction;
swm("HTMLAudioElement", function(t){
// arg to constructor is the url of the audio
if(typeof t == "string") this.src = t;
if(typeof t == "object") this.src = t.toString();
})
swm("Audio", HTMLAudioElement)
spdc("HTMLAudioElement", HTMLMediaElement)

swm("HTMLTemplateElement", function(){})
spdc("HTMLTemplateElement", HTMLElement)
// I'm doing the content fudging here, on demand; it's easier than in C.
Object.defineProperty(HTMLTemplateElement.prototype, "content", {
get: function() {
if(this.content$2) return this.content$2;
var c, frag = document.createDocumentFragment();
while(c = this.firstChild) frag.appendChild(c);
this.content$2 = frag;
return frag;
}});

// the performance registry
swm("pf$registry", {mark:{},measure:{},measure0:{},resourceTiming:{}})
Object.defineProperty(pf$registry, "measure0", {enumerable:false});
swm1("Performance", function(){})
Performance.prototype = {
// timeOrigin is the start time of this window, I guess
timeOrigin: Date.now(),
now:function(){ return Date.now()},
mark: function(name) { pf$registry.mark[name] = Date.now()},
clearMarks: function(e) { var m = pf$registry.mark; if(e) delete m[e]; else for(var i in m) delete m[i];},
measure:function(name,s,e) { var m = pf$registry.mark,  n = m[s] && m[e] ? m[e]-m[s] : 0; pf$registry.measure[name] = n; pf$registry.measure0[name] = this.now();},
clearMeasures: function(e) { var m = pf$registry.measure, m0 = pf$registry.measure0; if(e) delete m[e],delete m0[e]; else for(var i in m) delete m[i],delete m0[i];},
clearResourceTimings: function(e) { var m = pf$registry.resourceTiming; if(e) delete m[e]; else for(var i in m) delete m[i];},
getEntriesByType:function(type){var top = pf$registry[type];
var list = []; if(!top) return list;
for(var i in top) list.push({name:i, entryType:type, timeStamp:(type==="measure"?pf$registry.measure0[i]:top[i]), duration:(type==="measure"?top[i]:0)})
mw$.sortTime(list);
return list;
},
getEntriesByName:function(name,type){
var list = [];
if(type) {
var top = pf$registry[type];
if(top && top[name])
list.push({name:name, entryType:type, timeStamp:(type==="measure"?pf$registry.measure0[name]:top[name]), duration:(type==="measure"?top[name]:0)})
} else {
for(type in pf$registry) {
var m = pf$registry[type];
if(m[name])
list.push({name:name, entryType:type, timeStamp:(type==="measure"?pf$registry.measure0[name]:m[name]), duration:(type==="measure"?m[name]:0)})
}
mw$.sortTime(list);
}
return list;
},
getEntries:function(){
var list = [], r = pf$registry;
for(var type in r) {
var m = r[type];
for(var i in m) list.push({name:i, entryType:type, timeStamp:(type==="measure"?r.measure0[i]:m[i]), duration:(type==="measure"?m[i]:0)})
}
mw$.sortTime(list);
return list;
},
// at least have the object, even if it doesn't have any timestamps in it
timing:{},
}
Object.defineProperty(window, "performance", {get: function(){return new Performance}});

// this is a stub, I hope I don't have to implement this stuff.
swm("PerformanceObserver", {
supportedEntryTypes: {
// no types are supported
includes: eb$falsefunction
}
})

swm("cel$registry", {}) // custom elements registry
Object.defineProperty(window, "customElements", {get:function(){ return {
define:mw$.cel_define,
get:mw$.cel_get,
}},enumerable:true});

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
set: function(h) { if(h === null || h === undefined) h = ""; \
if(h instanceof URL || h.dom$class == "URL") h = h.toString(); \
var w = my$win(); \
if(typeof h != "string") { alert3("hrefset " + typeof h); \
w.hrefset$p.push("' + cn + '"); \
w.hrefset$a.push(h); \
return; } \
/* h is a string version of the url. Dont know what to do if h is empty. */ \
if(!h) return; \
var last_href = (this.href$2 ? this.href$2.toString() : null); \
this.setAttribute("' + u +'",h); \
/* special code for setting frame.src, redirect to a new page. */ \
h = this.href$2.href$val; \
if(this.is$frame && this.eb$expf && last_href != h) { \
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
var cnlist = ["HTMLFormElement", "HTMLImageElement", "HTMLScriptElement", "HTMLBaseElement", "HTMLLinkElement", "HTMLMediaElement"];
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
this.setAttribute("' + u +'",h) \
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
Watch out! If the script has inline text, it is a proper child of the script,
and should not be moved. Check for eb$nomove.
*********************************************************************/

swm("eb$uplift", function(s) {
var p = s.parentNode;
if(!p) return; // should never happen
var before = s.nextSibling;
var c = s.firstChild;
if(c && c.nodeType == 3 && c.eb$nomove) c = c.nextSibling;
while(c) {
var hold = c.nextSibling;
if(before) p.insertBefore(c, before);
else p.appendChild(c);
c = hold;
}
})

// Canvas method draws a picture. That's meaningless for us,
// but it still has to be there.
// Because of the canvas element, I can't but the monster getContext function
// into the prototype, I have to set it in the constructor.
swm("HTMLCanvasElement", function() {
this.getContext = function(x) { return {
canvas: this,
 addHitRegion: eb$nullfunction,
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
translate: eb$nullfunction }}})
spdc("HTMLCanvasElement", HTMLElement)
HTMLCanvasElement.prototype.toDataURL = function() {
if(this.height === 0  || this.width === 0) return "data:,";
// this is just a stub
return "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAADElEQVQImWNgoBMAAABpAAFEI8ARAAAAAElFTkSuQmCC";
}

swm("onmessage$$queue", [])
swm1("postMessage", function (message,target_origin, transfer) {
var locstring = this.location.protocol + "//" + this.location.hostname + ":" + this.location.port;
if(!this.location.port) {
// paste on a default port
var standard_port = mw$.setDefaultPort(this.location.protocol);
locstring += standard_port;
}
if(target_origin != '*' && !target_origin.match(/:\d*$/)) {
// paste on a default port
var target_protocol = target_origin.replace(/:.*/, ":");
var standard_port = mw$.setDefaultPort(target_protocol);
target_origin += ":" + standard_port;
}
if (target_origin == locstring || target_origin == "*") {
var me = new Event;
me.name = me.type = "message";
var l = my$win().location;
me.origin = l.protocol + "//" + l.hostname;
me.data = message;
me.source = my$win();
if(transfer) {
me.ports = transfer;
// If these objects had a context, they are now owned by this context.
for(var i = 0; i < transfer.length; ++i)
if(transfer[i].eb$ctx) transfer[i].eb$ctx = eb$ctx;
}
this.onmessage$$queue.push(me);
alert3("posting message of length " + message.length + " to window context " + this.eb$ctx + " ↑" +
(message.length >= 200 ? "long" : message)
+ "↑");
} else {
alert3("postMessage mismatch " + locstring + " | " + target_origin + " carrying ↑" +
(message.length >= 200 ? "long" : message)
+ "↑");
}
})
swm("onmessage$$running", mw$.onmessage$$running)

/*********************************************************************
AudioContext, for playing music etc.
This one we could implement, but I'm not sure if we should.
If speech comes out of the same speakers as music, as it often does,
you might not want to hear it, you might rather see the url, or have a button
to push, and then you call up the music only if / when you want it.
Not sure what to do, so it's pretty much stubs for now.
*********************************************************************/
swm("AudioContext", function() {
this.outputLatency = 1.0;
this.createMediaElementSource = eb$voidfunction;
this.createMediaStreamSource = eb$voidfunction;
this.createMediaStreamDestination = eb$voidfunction;
this.createMediaStreamTrackSource = eb$voidfunction;
this.suspend = eb$voidfunction;
this.close = eb$voidfunction;
})
spdc("AudioContext", null)

swm("DocumentFragment", function(){})
spdc("DocumentFragment", HTMLElement)
DocumentFragment.prototype.nodeType = 11;
DocumentFragment.prototype.nodeName = DocumentFragment.prototype.tagName = "#document-fragment";
DocumentFragment.prototype.querySelector = querySelector;
DocumentFragment.prototype.querySelectorAll = querySelectorAll;

swm("CSSRule", function(){this.cssText=""})
CSSRule.prototype.toString = function(){return this.cssText}

swm("CSSRuleList", function(){})
// This isn't really right, but it's easy
CSSRuleList.prototype = new Array;

swm("CSSStyleSheet", function() { this.cssRules = new CSSRuleList})
spdc("CSSStyleSheet", null)
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

swm("CSSStyleDeclaration", function(){
        this.element = null;
        this.style = this;
})
spdc("CSSStyleDeclaration", HTMLElement)
// sheet on demand
Object.defineProperty(CSSStyleDeclaration.prototype, "sheet", { get: function(){ if(!this.sheet$2) this.sheet$2 = new CSSStyleSheet; return this.sheet$2; }});

// these are default properties of a style object
CSSStyleDeclaration.prototype.textTransform = "none", // acid test 46
CSSStyleDeclaration.prototype.borderImageSource = "none";
;(function(){
var list =[
"accentColor","alignContent","alignItems","alignSelf","all",
"animation","animationDelay","animationDuration","animationFillMode","animationIterationCount","animationName","animationPlayState","animationTimingFunction",
"appearance","aspectRatio",
"backfaceVisibility","background","backgroundAttachment","backgroundBlendMode","backgroundClip","backgroundColor","backgroundImage",
"backgroundOrigin","backgroundPosition","backgroundPositionX","backgroundPositionY","backgroundRepeat","backgroundSize",
"blockSize","borderBlock","borderBlockColor","borderBlockEnd","borderBlockEndColor","borderBlockEndStyle","borderBlockEndWidth",
"borderBlockStart","borderBlockStartColor","borderBlockStartStyle","borderBlockStartWidth","borderBlockStyle","borderBlockWidth",
"borderBottomLeftRadius","borderBottomRightRadius","borderCollapse",
"borderEndEndRadius","borderEndStartRadius","borderInline","borderInlineColor","borderInlineEnd","borderInlineEndColor","borderInlineEndStyle","borderInlineEndWidth","borderInlineStart","borderInlineStartColor","borderInlineStartStyle","borderInlineStartWidth","borderInlineStyle","borderInlineWidth",
"borderRadius","borderSpacing","borderStartEndRadius","borderStartStartRadius","borderTopLeftRadius","borderTopRightRadius",
"bottom","boxDecorationBreak","boxShadow","boxSizing",
"breakAfter","breakBefore","breakInside",
"captionSide","caretColor","clear","clip","clipPath","clipRule",
"color","colorAdjust","colorInterpolation","colorInterpolationFilters",
"columnCount","columnFill","columnGap","columnRule","columnRuleColor","columnRuleStyle","columnRuleWidth","columns","columnSpan","columnWidth",
"contain","content","counterIncrement","counterReset","counterSet",
"cssFloat","cursor","cx","cy",
"direction","display","dominantBaseline",
"emptyCells","fill","fillOpacity","fillRule","filter",
"flex","flexBasis","flexDirection","flexFlow","flexGrow","flexShrink","flexWrap",
"float","floodColor","floodOpacity",
"font","fontFamily","fontFeatureSettings","fontKerning","fontLanguageOverride","fontSize","fontSizeAdjust","fontStretch","fontStyle","fontSynthesis","fontVariant","fontVariantAlternates","fontVariantCaps","fontVariantEastAsian","fontVariantLigatures","fontVariantNumeric","fontVariantPosition","fontWeight",
"gap","grid","gridArea","gridAutoColumns","gridAutoFlow","gridAutoRows","gridColumn","gridColumnEnd","gridColumnGap","gridColumnStart",
"gridGap","gridRow","gridRowEnd","gridRowGap","gridRowStart","gridTemplate","gridTemplateAreas","gridTemplateColumns","gridTemplateRows",
"hyphens","imageOrientation","imageRendering","imeMode","inlineSize",
"inset","insetBlock","insetBlockEnd","insetBlockStart","insetInline","insetInlineEnd","insetInlineStart","isolation",
"justifyContent","justifyItems","justifySelf",
"left","letterSpacing","lightingColor","lineBreak","lineHeight","listStyle","listStyleImage","listStylePosition","listStyleType",
"margin","marginBlock","marginBlockEnd","marginBlockStart","marginBottom","marginInline","marginInlineEnd","marginInlineStart","marginLeft","marginRight","marginTop",
"marker","markerEnd","markerMid","markerStart",
"mask","maskClip","maskComposite","maskImage","maskMode","maskOrigin","maskPosition","maskPositionX","maskPositionY","maskRepeat","maskSize","maskType",
"maxBlockSize","maxHeight","maxInlineSize","maxWidth",
"minBlockSize","minHeight","minInlineSize","minWidth","mixBlendMode",
"MozAnimation","MozAnimationDelay","MozAnimationDirection","MozAnimationDuration","MozAnimationFillMode","MozAnimationIterationCount","MozAnimationName","MozAnimationPlayState","MozAnimationTimingFunction",
"MozAppearance",
"MozBackfaceVisibility","MozBorderEnd","MozBorderEndColor","MozBorderEndStyle","MozBorderEndWidth","MozBorderStart","MozBorderStartColor","MozBorderStartStyle","MozBorderStartWidth",
"MozBoxAlign","MozBoxDirection","MozBoxFlex","MozBoxOrdinalGroup","MozBoxOrient","MozBoxPack","MozBoxSizing",
"MozFloatEdge","MozFontFeatureSettings","MozFontLanguageOverride","MozForceBrokenImageIcon",
"MozHyphens","MozImageRegion","MozMarginEnd","MozMarginStart","MozOrient",
"MozPaddingEnd","MozPaddingStart","MozPerspective","MozPerspectiveOrigin",
"MozTabSize","MozTextSizeAdjust","MozTransform","MozTransformOrigin","MozTransformStyle","MozTransition","MozTransitionDelay","MozTransitionDuration","MozTransitionProperty","MozTransitionTimingFunction",
"MozUserFocus","MozUserInput","MozUserModify","MozUserSelect","MozWindowDragging",
"objectFit","objectPosition",
"offset","offsetAnchor","offsetDistance","offsetPath","offsetRotate",
"opacity","order","outline","outlineColor","outlineOffset","outlineStyle","outlineWidth",
"overflow","overflowAnchor","overflowBlock","overflowInline","overflowWrap","overflowX","overflowY",
"overscrollBehavior","overscrollBehaviorBlock","overscrollBehaviorInline","overscrollBehaviorX","overscrollBehaviorY",
"padding","paddingBlock","paddingBlockEnd","paddingBlockStart","paddingBottom","paddingInline","paddingInlineEnd","paddingInlineStart","paddingLeft","paddingRight","paddingTop",
"pageBreakAfter","pageBreakBefore","pageBreakInside","paintOrder","perspective","perspectiveOrigin",
"placeContent","placeItems","placeSelf","pointerEvents","position",
"quotes",
"r","resize","right","rotate","rowGap","rubyAlign","rubyPosition","rx","ry",
"scale","scrollbarColor","scrollbarWidth","scrollBehavior","scrollMargin","scrollMarginBlock","scrollMarginBlockEnd","scrollMarginBlockStart","scrollMarginBottom","scrollMarginInline","scrollMarginInlineEnd","scrollMarginInlineStart","scrollMarginLeft","scrollMarginRight","scrollMarginTop",
"scrollPadding","scrollPaddingBlock","scrollPaddingBlockEnd","scrollPaddingBlockStart","scrollPaddingBottom","scrollPaddingInline","scrollPaddingInlineEnd","scrollPaddingInlineStart","scrollPaddingLeft","scrollPaddingRight","scrollPaddingTop",
"scrollSnapAlign","scrollSnapType",
"shapeImageThreshold","shapeMargin","shapeOutside","shapeRendering",
"stopColor","stopOpacity",
"stroke","strokeDasharray","strokeDashoffset","strokeLinecap","strokeLinejoin","strokeMiterlimit","strokeOpacity","strokeWidth",
"tableLayout","tabSize","textAlign","textAlignLast","textAnchor","textCombineUpright",
"textDecoration","textDecorationColor","textDecorationLine","textDecorationSkipInk","textDecorationStyle","textDecorationThickness",
"textEmphasis","textEmphasisColor","textEmphasisPosition","textEmphasisStyle","textIndent","textJustify",
"textOrientation","textOverflow","textRendering","textShadow","textUnderlineOffset","textUnderlinePosition",
"top","touchAction","transform","transformBox","transformOrigin","transformStyle",
"transition","transitionDelay","transitionDuration","transitionProperty","transitionTimingFunction","translate",
"unicodeBidi","userSelect","vectorEffect","verticalAlign","visibility",
"webkitAlignContent","WebkitAlignContent","webkitAlignItems","WebkitAlignItems","webkitAlignSelf","WebkitAlignSelf",
"webkitAnimation","WebkitAnimation","webkitAnimationDelay","WebkitAnimationDelay","webkitAnimationDirection","WebkitAnimationDirection","webkitAnimationDuration","WebkitAnimationDuration","webkitAnimationFillMode","WebkitAnimationFillMode","webkitAnimationIterationCount","WebkitAnimationIterationCount",
"webkitAnimationName","WebkitAnimationName","webkitAnimationPlayState","WebkitAnimationPlayState","webkitAnimationTimingFunction","WebkitAnimationTimingFunction",
"webkitAppearance","WebkitAppearance",
"webkitBackfaceVisibility","WebkitBackfaceVisibility","webkitBackgroundClip","WebkitBackgroundClip","webkitBackgroundOrigin","WebkitBackgroundOrigin","webkitBackgroundSize","WebkitBackgroundSize",
"webkitBorderBottomLeftRadius","WebkitBorderBottomLeftRadius","webkitBorderBottomRightRadius","WebkitBorderBottomRightRadius","webkitBorderRadius","WebkitBorderRadius","webkitBorderTopLeftRadius","WebkitBorderTopLeftRadius","webkitBorderTopRightRadius","WebkitBorderTopRightRadius",
"webkitBoxAlign","WebkitBoxAlign","webkitBoxDirection","WebkitBoxDirection","webkitBoxFlex","WebkitBoxFlex","webkitBoxOrdinalGroup","WebkitBoxOrdinalGroup","webkitBoxOrient","WebkitBoxOrient",
"webkitBoxPack","WebkitBoxPack","webkitBoxShadow","WebkitBoxShadow","webkitBoxSizing","WebkitBoxSizing",
"webkitFilter","WebkitFilter","webkitFlex","WebkitFlex","webkitFlexBasis","WebkitFlexBasis","webkitFlexDirection","WebkitFlexDirection","webkitFlexFlow","WebkitFlexFlow","webkitFlexGrow","WebkitFlexGrow",
"webkitFlexShrink","WebkitFlexShrink","webkitFlexWrap","WebkitFlexWrap",
"webkitJustifyContent","WebkitJustifyContent","webkitLineClamp","WebkitLineClamp",
"webkitMask","WebkitMask","webkitMaskClip","WebkitMaskClip","webkitMaskComposite","WebkitMaskComposite","webkitMaskImage","WebkitMaskImage","webkitMaskOrigin","WebkitMaskOrigin",
"webkitMaskPosition","WebkitMaskPosition","webkitMaskPositionX","WebkitMaskPositionX","webkitMaskPositionY","WebkitMaskPositionY","webkitMaskRepeat","WebkitMaskRepeat","webkitMaskSize","WebkitMaskSize",
"webkitOrder","WebkitOrder","webkitPerspective","WebkitPerspective","webkitPerspectiveOrigin","WebkitPerspectiveOrigin",
"webkitTextFillColor","WebkitTextFillColor","webkitTextSizeAdjust","WebkitTextSizeAdjust","webkitTextStroke","WebkitTextStroke","webkitTextStrokeColor","WebkitTextStrokeColor","webkitTextStrokeWidth","WebkitTextStrokeWidth",
"webkitTransform","WebkitTransform","webkitTransformOrigin","WebkitTransformOrigin","webkitTransformStyle","WebkitTransformStyle",
"webkitTransition","WebkitTransition","webkitTransitionDelay","WebkitTransitionDelay","webkitTransitionDuration","WebkitTransitionDuration","webkitTransitionProperty","WebkitTransitionProperty","webkitTransitionTimingFunction","WebkitTransitionTimingFunction",
"webkitUserSelect","WebkitUserSelect",
"whiteSpace","willChange","wordBreak","wordSpacing","wordWrap","writingMode",
"x",
"y",
"zIndex",];
for(var i = 0; i < list.length; ++i)
CSSStyleDeclaration.prototype[list[i]] = "";
})();
CSSStyleDeclaration.prototype.borderImageOutset = "0";
CSSStyleDeclaration.prototype.borderImageWidth = "1";
CSSStyleDeclaration.prototype.borderImageSlice = "100%";
CSSStyleDeclaration.prototype.border = CSSStyleDeclaration.prototype.borderBottom = CSSStyleDeclaration.prototype.borderLeft = CSSStyleDeclaration.prototype.borderRight = CSSStyleDeclaration.prototype.borderTop = "1px solid rgb(193, 193, 193)";
CSSStyleDeclaration.prototype.borderBottomWidth = CSSStyleDeclaration.prototype.borderLeftWidth = CSSStyleDeclaration.prototype.borderRightWidth = CSSStyleDeclaration.prototype.borderTopWidth = CSSStyleDeclaration.prototype.borderWidth = "1px";
CSSStyleDeclaration.prototype.width = "250px";
CSSStyleDeclaration.prototype.height = "40px";
CSSStyleDeclaration.prototype.borderImage = CSSStyleDeclaration.prototype.MozBorderImage = CSSStyleDeclaration.prototype.webkitBorderImage = CSSStyleDeclaration.prototype.WebkitBorderImage = "none 100% / 1 / 0 stretch";
CSSStyleDeclaration.prototype.borderBottomColor = CSSStyleDeclaration.prototype.borderColor = CSSStyleDeclaration.prototype.borderLeftColor = CSSStyleDeclaration.prototype.borderRightColor = CSSStyleDeclaration.prototype.borderTopColor = "rgb(193, 193, 193)";
CSSStyleDeclaration.prototype.borderBottomStyle = CSSStyleDeclaration.prototype.borderLeftStyle = CSSStyleDeclaration.prototype.borderRightStyle = CSSStyleDeclaration.prototype.borderStyle = CSSStyleDeclaration.prototype.borderTopStyle = "solid";
CSSStyleDeclaration.prototype.borderImageRepeat = "stretch";
CSSStyleDeclaration.prototype.parentRule = null;

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

swm("HTMLStyleElement", function(){})
spdc("HTMLStyleElement", HTMLElement)
// Kind of a hack to make this like the link element
Object.defineProperty(HTMLStyleElement.prototype, "css$data", {
get: function() { var s = ""; for(var i=0; i<this.childNodes.length; ++i) if(this.childNodes[i].nodeName == "#text") s += this.childNodes[i].data; return s; }});
Object.defineProperty(HTMLStyleElement.prototype, "sheet", { get: function(){ if(!this.sheet$2) this.sheet$2 = new CSSStyleSheet; return this.sheet$2; }});

HTMLTableElement.prototype.insertRow = mw$.insertRow;
HTMLTableSectionElement.prototype.insertRow = mw$.insertRow;
HTMLTableElement.prototype.deleteRow = mw$.deleteRow;
HTMLTableSectionElement.prototype.deleteRow = mw$.deleteRow;
HTMLTableRowElement.prototype.insertCell = mw$.insertCell;
HTMLTableRowElement.prototype.deleteCell = mw$.deleteCell;

// rows under a table section
HTMLTableSectionElement.prototype.appendChildNative = mw$.appendChild;
HTMLTableSectionElement.prototype.appendChild = function(newobj) {
if(!newobj) return null;
if(newobj.nodeType == 11) return mw$.appendFragment(this, newobj);
this.appendChildNative(newobj);
if(newobj.dom$class == "HTMLTableRowElement") // shouldn't be anything other than TR
this.rows.push(newobj), rowReindex(this);
return newobj;
}
HTMLTableSectionElement.prototype.insertBeforeNative = mw$.insertBefore;
HTMLTableSectionElement.prototype.insertBefore = function(newobj, item) {
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
HTMLTableSectionElement.prototype.removeChildNative = mw$.removeChild;
HTMLTableSectionElement.prototype.removeChild = function(item) {
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

swm("TextNode", function() {
this.data$2 = "";
if(arguments.length > 0) {
// data always has to be a string
this.data$2 += arguments[0];
}
})
spdc("TextNode", HTMLElement)
TextNode.prototype.nodeName = TextNode.prototype.tagName = "#text";
TextNode.prototype.nodeType = 3;

// setter insures data is always a string, because roving javascript might
// node.data = 7;  ...  if(node.data.match(/x/) ...
// and boom! It blows up because Number doesn't have a match function.
Object.defineProperty(TextNode.prototype, "data", {
get: function() { return this.data$2; },
set: function(s) { this.data$2 = s + ""; }});

sdm2("createTextNode", function(t) {
if(t == undefined) t = "";
var c = new TextNode(t);
/* A text node chould never have children, and does not need childNodes array,
 * but there is improper html out there <text> <stuff> </text>
 * which has to put stuff under the text node, so against this
 * unlikely occurence, I have to create the array.
 * I have to treat a text node like an html node. */
c.childNodes = [];
c.parentNode = null;
if(this.eb$xml) c.eb$xml = true;
eb$logElement(c, "text");
return c;
})

swm("Comment", function(t) {
this.data = t;
})
spdc("Comment", HTMLElement)
Comment.prototype.nodeName = Comment.prototype.tagName = "#comment";
Comment.prototype.nodeType = 8;

swm("XMLCdata", function(t) {})
spdc("XMLCdata", HTMLElement)
XMLCdata.prototype.nodeName = XMLCdata.prototype.tagName = "#cdata-section";
XMLCdata.prototype.nodeType = 4;

sdm2("createComment", function(t) {
if(t == undefined) t = "";
var c = new Comment(t);
c.childNodes = [];
c.parentNode = null;
eb$logElement(c, "comment");
return c;
})

// The Option class, these are choices in a dropdown list.
swm("HTMLOptionElement", function() {
if(arguments.length > 0)
this.text = arguments[0];
if(arguments.length > 1)
this.value = arguments[1];
})
spdc("HTMLOptionElement", HTMLElement)
swm2("Option", HTMLOptionElement)
Option.prototype.selected = false;
Option.prototype.defaultSelected = false;
Option.prototype.nodeName = Option.prototype.tagName = "OPTION";
Option.prototype.text = Option.prototype.value = "";

swm("HTMLOptGroupElement", function() {})
spdc("HTMLOptGroupElement", HTMLElement)
HTMLOptGroupElement.prototype.nodeName = HTMLOptGroupElement.prototype.tagName = "OPTGROUP";

sdm("getBoundingClientRect", function(){
return {
top: 0, bottom: 0, left: 0, right: 0,
x: 0, y: 0,
width: 0, height: 0
}
})

// The Attr class and getAttributeNode().
swm("Attr", function(){ this.owner = null; this.name = ""})
spdc("Attr", null)
Attr.prototype.isId = function() { return this.name === "id"; }

// this is sort of an array and sort of not.
// For one thing, you can call setAttribute("length", "snork"), so I can't use length.
swm("NamedNodeMap", function() { this.length = 0})
spdc("NamedNodeMap", null)
NamedNodeMap.prototype.push = function(s) { this[this.length++] = s; }
NamedNodeMap.prototype.item = function(n) { return this[n]; }
NamedNodeMap.prototype.getNamedItem = function(name) { return this[name.toLowerCase()]; }
NamedNodeMap.prototype.setNamedItem = function(name, v) { this.owner.setAttribute(name, v);}
NamedNodeMap.prototype.removeNamedItem = function(name) { this.owner.removeAttribute(name);}

sdm("getAttribute", mw$.getAttribute)
sdm("hasAttribute", mw$.hasAttribute)
sdm("getAttributeNames", mw$.getAttributeNames)
sdm("getAttributeNS", mw$.getAttributeNS)
sdm("hasAttributeNS", mw$.hasAttributeNS)
sdm("setAttribute", mw$.setAttribute)
sdm("setAttributeNS", mw$.setAttributeNS)
sdm("removeAttribute", mw$.removeAttribute)
sdm("removeAttributeNS", mw$.removeAttributeNS)
sdm("getAttributeNode", mw$.getAttributeNode)

sdm("cloneNode", function(deep) {
cloneRoot1 = this;
return mw$.clone1 (this,deep);
})

/*********************************************************************
importNode seems to be the same as cloneNode, except it is copying a tree
of objects from another context into the current context.
But this is how quickjs works by default.
foo.s = cloneNode(bar.s);
If bar is in another context that's ok, we read those objects and create
copies of them in the current context.
*********************************************************************/

sdm("importNode", function(src, deep) { return src.cloneNode(deep)})

swm1("Event", function(etype){
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
})
spdc("Event", null)

Event.prototype.preventDefault = function(){ this.defaultPrevented = true; }

Event.prototype.stopPropagation = function(){ if(this.cancelable)this.cancelled = true; }

// deprecated - I guess - but a lot of people still use it.
Event.prototype.initEvent = function(t, bubbles, cancel) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel; this.defaultPrevented = false; }

Event.prototype.initUIEvent = function(t, bubbles, cancel, unused, detail) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel, this.detail = detail; this.defaultPrevented = false; }
Event.prototype.initCustomEvent = function(t, bubbles, cancel, detail) {
this.type = t, this.bubbles = bubbles, this.cancelable = cancel, this.detail = detail; }

sdm2("createEvent", function(unused) { return new Event; })

swm("HashChangeEvent", function(){
    this.currentTarget =     this.target = null;
    this.eventPhase = 0;
    this.timeStamp = new Date().getTime();
this.type = "hashchange";
})
// says we inherit from event but I can't think of any event methods we would want to use
spdc("HashChangeEvent", null)

swm("MouseEvent", function(etype){
    this.bubbles =     this.cancelable = true;
    this.cancelled = this.defaultPrevented = false;
    this.currentTarget =     this.target = null;
    this.eventPhase = 0;
    this.timeStamp = new Date().getTime();
if(typeof etype == "string") this.type = etype;
})
MouseEvent.prototype = new Event;
MouseEvent.prototype.altKey = false;
MouseEvent.prototype.ctrlKey = false;
MouseEvent.prototype.shiftKey = false;
MouseEvent.prototype.metaKey = false;
MouseEvent.prototype.initMouseEvent = function() { this.initEvent.apply(this, arguments)}

swm("PromiseRejectionEvent", function(etype){
    this.bubbles =     this.cancelable = true;
    this.cancelled = this.defaultPrevented = false;
    this.currentTarget =     this.target = null;
    this.eventPhase = 0;
    this.timeStamp = new Date().getTime();
if(typeof etype == "string") this.type = etype;
})
PromiseRejectionEvent.prototype = new Event;

swm("CustomEvent", function(etype, o){
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
})
CustomEvent.prototype = new Event;

swm("MediaQueryList", function() {
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
})
spdc("MediaQueryList", null)

swm1("matchMedia", function(s) {
var q = new MediaQueryList;
q.media = s;
q.matches = eb$media(s);
return q;
})

sdm("insertAdjacentHTML", mw$.insertAdjacentHTML)

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
p.getRootNode = mw$.getRootNode;
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
p.eb$rmch2 = document.eb$rmch2;
p.eb$insbf = document.eb$insbf;
p.removeChild = mw$.removeChild;
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
p.getAttribute = mw$.getAttribute;
p.getAttributeNS = mw$.getAttributeNS;
p.getAttributeNames = mw$.getAttributeNames;
p.setAttribute = mw$.setAttribute;
p.setAttributeNS = mw$.setAttributeNS;
p.removeAttribute = mw$.removeAttribute;
p.removeAttributeNS = mw$.removeAttributeNS;
Object.defineProperty(p, "className", { get: function() { return this.getAttribute("class"); }, set: function(h) { this.setAttribute("class", h); }});
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
get: function() { return this.nodeType == 3 ? this.data : this.nodeType == 4 ? this.text : null;},
set: function(h) { if(this.nodeType == 3) this.data = h; if (this.nodeType == 4) this.text = h }});
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

HTMLFormElement.prototype.appendChildNative = mw$.appendChild;
HTMLFormElement.prototype.appendChild = mw$.formAppendChild;
HTMLFormElement.prototype.insertBeforeNative = mw$.insertBefore;
HTMLFormElement.prototype.insertBefore = mw$.formInsertBefore;
HTMLFormElement.prototype.removeChildNative = mw$.removeChild;
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
Actually we shouldn't be calling this routine at all; should be calling add(),
so I don't even know if this makes sense.
*********************************************************************/

HTMLSelectElement.prototype.appendChild = function(newobj) {
if(!newobj) return null;
// should only be options!
if(!(newobj.dom$class == "HTMLOptionElement")) return newobj;
mw$.isabove(newobj, this);
if(newobj.parentNode) newobj.parentNode.removeChild(newobj);
var l = this.childNodes.length;
if(newobj.defaultSelected) newobj.selected = true, this.selectedIndex = l;
this.childNodes.push(newobj); newobj.parentNode = this;
this.eb$bso();
mutFixup(this, false, newobj, null);
return newobj;
}
HTMLSelectElement.prototype.insertBefore = function(newobj, item) {
var i;
if(!newobj) return null;
if(!item) return this.appendChild(newobj);
if(!(newobj.dom$class == "HTMLOptionElement")) return newobj;
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
this.eb$bso();
mutFixup(this, false, newobj, null);
return newobj;
}
HTMLSelectElement.prototype.removeChild = function(item) {
var i;
if(!item) return null;
for(i=0; i<this.childNodes.length; ++i)
if(this.childNodes[i] == item) break;
if(i == this.childNodes.length) return null;
this.childNodes.splice(i, 1);
item.parentNode = null;
this.eb$bso();
mutFixup(this, false, i, item);
return item;
}

// these routines do not account for optgroups
HTMLSelectElement.prototype.add = function(o, idx) {
var n = this.options.length;
if(typeof idx != "number" || idx < 0 || idx > n) idx = n;
if(idx == n) this.appendChild(o);
else this.insertBefore(o, this.childNodes[idx]);
}
HTMLSelectElement.prototype.remove = function(idx) {
var n = this.options.length;
if(typeof idx == "number" && idx >= 0 && idx < n)
this.removeChild(this.options[idx]);
}

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
HTMLTableElement.prototype.removeChildNative = mw$.removeChild;
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
HTMLTableRowElement.prototype.removeChildNative = mw$.removeChild;
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
eval('Object.defineProperty(' + cn + ', "' + evname + '$$watch", {value:true})');
eval('Object.defineProperty(' + cn + ', "' + evname + '", { \
get: function() { return this.' + evname + '$2; }, \
set: function(f) { if(db$flags(1)) alert3((this.'+evname+'?"clobber ":"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".' + evname + '"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.' + evname + '$2 = f}}})')
}}})();

// onhashchange from certain places
; (function() {
// also HTMLFrameSetElement and SVGEElement, which we have not yet implemented
var cnlist = ["HTMLBodyElement.prototype", "window"];
for(var i=0; i<cnlist.length; ++i) {
var cn = cnlist[i];
eval('Object.defineProperty(' + cn + ', "onhashchange$$watch", {value:true})');
eval('Object.defineProperty(' + cn + ', "onhashchange", { \
get: function() { return this.onhashchange$2; }, \
set: function(f) { if(db$flags(1)) alert3((this.onhashchange?"clobber ":"create ") + (this.nodeName ? this.nodeName : "+"+this.dom$class) + ".onhashchange"); \
if(typeof f == "string") f = my$win().handle$cc(f, this); \
if(typeof f == "function") { this.onhashchange$2 = f}}})')
}})();

// Some website expected an onhashchange handler from the get-go.
// Don't know what website, and didn't write it down, but it makes no sense to me!
// Handlers aren't there unless the website puts them there.
// onhashchange = eb$voidfunction;
// If we do need a default handler don't create it as above,
// that leads to confusion; use the get  method.
// get: function() { return this.onhashchange$2 ? this.onhashchange$2 : eb$voidfunction; }

sdm2("createElementNS", function(nsurl,s) {
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
})

sdm2("createElement", function(s) {
var c;
if(!s) { // a null or missing argument
alert3("bad createElement( type" + typeof s + ')');
return null;
}
var t = s.toLowerCase();

// check for custom elements first
var x = cel$registry[s];
if(x) { // here we go
c = new x.construct;
if(c instanceof HTMLElement) {
c.childNodes = [];
c.parentNode = null;
c.nodeName = c.tagName = this.eb$xml ? s : s.toUpperCase();
if(this.eb$xml) c.eb$xml = true;
}
eb$logElement(c, t);
return c;
}

if(!t.match(/^[a-z:\d_-]+$/) || t.match(/^[\d_-]/)) {
alert3("bad createElement(" + t + ')');
// acid3 says we should throw an exception here.
// But we get these kinds of strings from www.oranges.com all the time.
// I'll just return null and tweak acid3 accordingly.
// throw error code 5
return null;
}

var unknown = false;
switch(t) {
case "shadowroot": c = new ShadowRoot; break;
case "body": c = new HTMLBodyElement; break;
// is it ok that head isn't here?
case "object": c = new HTMLObjectElement; break;
case "a": c = new HTMLAnchorElement; break;
case "area": c = new HTMLAreaElement; break;
case "image": t = "img";
case "img": c = new HTMLImageElement; break;
case "link": c = new HTMLLinkElement; break;
case "meta": c = new HTMLMetaElement; break;
case "cssstyledeclaration":
c = new CSSStyleDeclaration; c.element = null; break;
case "style": c = new HTMLStyleElement; break;
case "script": c = new HTMLScriptElement; break;
case "template": c = new HTMLTemplateElement; break;
case "div": c = new HTMLDivElement; break;
case "span": c = new HTMLSpanElement; break;
case "label": c = new HTMLLabelElement; break;
case "p": c = new HTMLParagraphElement; break;
case "ol": c = new HTMLOListElement; break;
case "ul": c = new HTMLUListElement; break;
case "dl": c = new HTMLDListElement; break;
case "li": c = new HTMLLIElement; break;
case "h1": case "h2": case "h3": case "h4": case "h5": case "H6": c = new HTMLHeadingElement; break;
case "header": c = new z$Header; break;
case "footer": c = new z$Footer; break;
case "table": c = new HTMLTableElement; break;
case "tbody": c = new z$tBody; break;
case "tr": c = new HTMLTableRowElement; break;
case "td": c = new HTMLTableCellElement; break;
case "caption": c = new z$tCap; break;
case "thead": c = new z$tHead; break;
case "tfoot": c = new z$tFoot; break;
case "canvas": c = new HTMLCanvasElement; break;
case "audio": case "video": c = new HTMLAudioElement; break;
case "fragment": c = new DocumentFragment; break;
case "frame": c = new HTMLFrameElement; break;
case "iframe": c = new HTMLIFrameElement; break;
case "select": c = new HTMLSelectElement; break;
case "option":
c = new Option;
c.childNodes = [];
c.parentNode = null;
if(this.eb$xml) c.eb$xml = true;
c.selected = true; // jquery says we should do this
// we don't log options because rebuildSelectors() checks
// the dropdown lists after every js run.
return c;
case "form": c = new HTMLFormElement; break;
case "input": c = new HTMLInputElement; break;
case "textarea": c = new HTMLTextAreaElement; break;
case "element": c = new HTMLElement; break;
case "button": c = new HTMLButtonElement; break;
default:
unknown = true;
// alert("createElement default " + s);
c = new HTMLUnknownElement;
}

c.childNodes = [];
c.parentNode = null;
if(this.eb$xml && !(c instanceof HTMLFrameElement) && !(c instanceof HTMLIFrameElement)) c.eb$xml = true;
// Split on : if this comes from a name space
var colon = t.split(':');
if(colon.length == 2) {
c.nodeName = c.tagName = t;
c.prefix = colon[0], c.localName = colon[1];
} else if(c.nodeType == 1)
c.nodeName = c.tagName = (unknown || this.eb$xml) ? s : s.toUpperCase();
if(t == "input") { // name and type are automatic attributes acid test 53
c.name = c.type = "";
}
eb$logElement(c, s);
return c;
} )

sdm2("createDocumentFragment", function() {
var c = this.createElement("fragment");
return c;
})

sdm("implementation", {
owner: document,
/*********************************************************************
This is my tentative implementation of hasFeature:
hasFeature: function(mod, v) {
// I'll say edbrowse supports html5
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
})

swm("XMLHttpRequestEventTarget", function(){})
XMLHttpRequestEventTarget.prototype = new EventTarget;

swm("XMLHttpRequestUpload", function(){})
XMLHttpRequestUpload.prototype = new XMLHttpRequestEventTarget;

// @author Originally implemented by Yehuda Katz
// And since then, from envjs, by Thatcher et al

swm("XMLHttpRequest", function(){
    this.headers = {};
    this.responseHeaders = {};
    this.aborted = false;//non-standard
    this.withCredentials = true;
this.upload = new XMLHttpRequestUpload;
})
spdc("XMLHttpRequest", null)
// this form of XMLHttpRequest is deprecated, but still used in places.
XDomainRequest = XMLHttpRequest;

// defined by the standard: http://www.w3.org/TR/XMLHttpRequest/#xmlhttprequest
// but not provided by Firefox.  Safari and others do define it.
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPEN = 1;
XMLHttpRequest.HEADERS_RECEIVED = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;
// see shared.js for these methods
XMLHttpRequest.prototype.open = mw$.xml_open;
XMLHttpRequest.prototype.setRequestHeader = mw$.xml_srh;
XMLHttpRequest.prototype.getResponseHeader = mw$.xml_grh;
XMLHttpRequest.prototype.getAllResponseHeaders = mw$.xml_garh;
XMLHttpRequest.prototype.send = mw$.xml_send;
XMLHttpRequest.prototype.parseResponse = mw$.xml_parse;
XMLHttpRequest.prototype.abort = function(){ this.aborted = true}
XMLHttpRequest.prototype.onreadystatechange = XMLHttpRequest.prototype.onload = XMLHttpRequest.prototype.onerror = eb$voidfunction;
XMLHttpRequest.prototype.overrideMimeType = function(t) {
if(typeof t == "string") this.eb$mt = t;
}
XMLHttpRequest.prototype.eb$listen = eb$listen;
XMLHttpRequest.prototype.eb$unlisten = eb$unlisten;
XMLHttpRequest.prototype.addEventListener = addEventListener;
XMLHttpRequest.prototype.removeEventListener = removeEventListener;
XMLHttpRequest.prototype.dispatchEvent = mw$.dispatchEvent;
XMLHttpRequest.prototype.eb$mt = null;
XMLHttpRequest.prototype.async = false;
XMLHttpRequest.prototype.readyState = 0;
XMLHttpRequest.prototype.responseText = "";
XMLHttpRequest.prototype.response = "";
XMLHttpRequest.prototype.responseXML = null;
XMLHttpRequest.prototype.status = 0;
XMLHttpRequest.prototype.statusText = "";

// response to a fetch() request
swm("Response", function(){this.xhr = null, this.bodyUsed = false})
Object.defineProperty(Response.prototype, "body", {get:function(){this.bodyUsed=true;return this.xhr.responseText;}})
Object.defineProperty(Response.prototype, "headers", {get:function(){return this.xhr.responseHeaders;}})
Object.defineProperty(Response.prototype, "ok", {get:function(){return this.xhr.status >= 200 && this.xhr.status <= 299;}})
Object.defineProperty(Response.prototype, "redirect", {get:function(){return this.xhr.responseURL != this.xhr.url;}})
Object.defineProperty(Response.prototype, "status", {get:function(){return this.xhr.status;}})
Object.defineProperty(Response.prototype, "statusText", {get:function(){return this.xhr.statusText;}})
// this one isn't right; just a stub for now
Object.defineProperty(Response.prototype, "type", {get:function(){alert3("Response.type always basic");return "basic";}})
// should this beUrl or response URL?
Object.defineProperty(Response.prototype, "url", {get:function(){return this.xhr.url;}})
// json is the only method so far; I guess we write them as we need them.
Response.prototype.json = function(){return Promise.resolve(JSON.parse(this.body))}

swm1("fetch", function(url, o) {
var dopost = false;
if(o && o.method && o.method.toLowerCase() == "post") dopost = true;
var body = "";
if(o && typeof o.body == "string") body = o.body;
var xhr = new XMLHttpRequest;
// don't run this async, it needs to complete and then we set Response.ok and other response variables.
xhr.open(dopost?"POST":"GET",url, false);
if(o && typeof o.headers == "object") xhr.headers = o.headers;
if(o && o.credentials) alert3("fetch credentials " + o.credentials + " not supported.");
xhr.send(body, 0);
var r = new Response; r.xhr = xhr;
return Promise.resolve(r);
})

// pages seem to want document.style to exist
sdm("style", new CSSStyleDeclaration)
document.style.element = document;
document.style.bgcolor = "white";

sdm("ELEMENT_NODE", 1)
sdm("TEXT_NODE", 3)
sdm("COMMENT_NODE", 8)
sdm("DOCUMENT_NODE", 9)
sdm("DOCUMENT_TYPE_NODE", 10)
sdm("DOCUMENT_FRAGMENT_NODE", 11)

// originally ms extension pre-DOM, we don't fully support it
//but offer the legacy document.all.tags method.
sdm("all", {})
document.all.tags = function(s) {
return mw$.eb$gebtn(document.body, s.toLowerCase());
}

swm("eb$demin", mw$.deminimize)
swm("eb$watch", mw$.addTrace)
/*
swm("$uv", [])
swm("$uv$sn", 0)
*/
swm2("$jt$c", 'z')
swm2("$jt$sn", 0)

sdm("childNodes", [])
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

swm("handle$cc", function(f, t) {
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
})

// Local storage, this is per window.
// Then there's sessionStorage, and honestly I don't understand the difference.
// This is NamedNodeMap, to take advantage of preexisting methods.
swm("localStorage", {})
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

swm("sessionStorage", {})
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
}, enumerable:true});
// We need location$2 so we can define origin and replace etc
swm2("location$2", new URL)
Object.defineProperty(location$2, "origin", {get:function(){
return this.protocol ? this.protocol + "//" + this.host : null}});
Object.defineProperty(window, "origin", {get: function(){return location.origin}});
sdm("location$2", new URL)
Object.defineProperty(document, "location", {
get: function() { return this.location$2; },
set: function(h) {
if(!this.location$2) {
this.location$2 = new z$URL(h);
} else {
this.location$2.href = h;
}
}, enumerable:true});
    location.replace = document.location.replace = function(s) { this.href = s};
Object.defineProperty(window.location,'replace',{enumerable:false});
Object.defineProperty(document.location,'replace',{enumerable:false});
Object.defineProperty(window.location,'eb$ctx',{value:eb$ctx});
Object.defineProperty(document.location,'eb$ctx',{value:eb$ctx});

// Window constructor, passes the url back to edbrowse
// so it can open a new web page.
swm("Window", function() {
var newloc = "";
var winname = "";
if(arguments.length > 0) newloc = arguments[0];
if(arguments.length > 1) winname = arguments[1];
// I only do something if opening a new web page.
// If it's just a blank window, I don't know what to do with that.
if(newloc.length)
eb$newLocation('p' + eb$ctx + newloc+ '\n' + winname);
this.opener = window;
})

// window.open is the same as new window, just pass the args through
swm1("open", function() {
return Window.apply(this, arguments);
})

// nasa.gov and perhaps other sites check for self.constructor == Window.
// That is, Window should be the constructor of window.
// The constructor is Object by default.
swm("constructor", Window)

// Apply rules to a given style object, which is this.
Object.defineProperty(CSSStyleDeclaration.prototype, "cssText", { get: mw$.cssTextGet,
set: function(h) { var w = my$win(); w.soj$ = this; eb$cssText.call(this,h); delete w.soj$; } });

swm("eb$qs$start", function() { mw$.cssGather(true); mw$.frames$rebuild(window);})

swm("DOMParser", mw$.DOMParser)

swm("XMLSerializer", function(){})
XMLSerializer.prototype.serializeToString = function(root) {
alert3("trying to use XMLSerializer");
return "<div>XMLSerializer not yet implemented</div>"; }

swm2("css$ver", 0)
swm2("css_all", "")
swm2("last$css_all", "")
swm2("cssSource", [])
sdm("xmlVersion", 0)

swm("MutationObserver", function(f) {
var w = my$win();
w.mutList.push(this);
this.callback = (typeof f == "function" ? f : eb$voidfunction);
this.active = false;
this.target = null;
})
spdc("MutationObserver", null)
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

swm("MutationRecord", function(){})
spdc("MutationRecord", null)

swm("mutList", [])

swm1("crypto", {})
crypto.getRandomValues = function(a) {
if(typeof a != "object") return NULL;
var l = a.length;
for(var i=0; i<l; ++i) a[i] = Math.floor(Math.random()*0x100000000);
return a;
}

swm2("ra$step", 0)
swm1("requestAnimationFrame", function() {
// This absolutely doesn't do anything. What is edbrowse suppose to do with animation?
return ++ra$step;
})

swm1("cancelAnimationFrame", eb$voidfunction)

// link in the blob code
swm("Blob", mw$.Blob)
swm("File", mw$.File)
swm("FileReader", mw$.FileReader)
URL.createObjectURL = mw$.URL.createObjectURL
URL.revokeObjectURL = mw$.URL.revokeObjectURL
swm("FormData", mw$.FormData)
swm("TextEncoder", mw$.TextEncoder)
swm("TextDecoder", mw$.TextDecoder)
swm("MessagePort", mw$.MessagePort)
swm("MessageChannel", mw$.MessageChannel)
swm("mp$registry", []) // MessagePort registry
swm("URLSearchParams", mw$.URLSearchParams)

swm("trustedTypes", function(){})
trustedTypes.createPolicy = function(pn,po){
var x = {policyName: pn};
for (var i in po) { x[i] = po[i]}
return x;
}

swm("DOMException", function(m, n) { // constructor
this.message = typeof m == "string" ? m : "";
this.code = 0;
if(typeof n == "string") {
this.name = n;
// we need to set code here, based on standard names, not yet implemented.
alert3("DOMException name " + n);
}
})

swm("AbortSignal", function(){})
AbortSignal.prototype = new EventTarget;
AbortSignal.prototype.aborted = false;
AbortSignal.prototype.reason = 0;
AbortSignal.prototype.throwIfAborted = eb$voidfunction;
AbortSignal.abort = function(){ var c = new AbortSignal(); c.aborted = true; return c; }
AbortSignal.timeout = function(ms){ var c = new AbortSignal();
// this is suppose to abort after a timeout period but I don't know how to do that
if(typeof ms == "number") alert3("abort after " + ms + "ms not implemented");
return c; }

swm("AbortController", function(){})
Object.defineProperty(AbortController.prototype, "signal",
{get:function(){return new AbortSignal}});
AbortController.prototype.abort = function(){
alert3("abort dom request not implemented"); }

swm("IntersectionObserverEntry", function(){})
swm("IntersectionObserver", function(callback, o){
this.callback = callback, this.root = null;
var h = 1.0;
if(typeof o == "object") {
if(o.root) this.root = o.root;
if(o.threshold) h = o.threshold;
}
var alertstring = "intersecting " + (this.root ? this.root : "viewport");
if(typeof h == "number") alertstring += " with threshold " + h;
else if(Array.isArray(h)) {
alertstring += " with threshold [";
for(var i = 0; i < h.length; ++i) {
var n = h[i];
if(i) alertstring += ',';
if(typeof n == "number") alertstring += n;
}
alertstring += ']';
}
alert3(alertstring);
})
/*********************************************************************
This is just trying to get something off the ground.
Assume our target is always visible.
I don't even know what visible means in edbrowse.
You have printed, or asked for, a line in the target area?
And what percentage of that target area is visible,
just because you printed a line therein?
This stuff is so visual it's almost impossible to simulate with any fidelity.
So for a start, everything is visible, and that might cause the
website to load anything you might ever look at or scroll down to,
making edbrowse even slower than it already is. But it's a start.
*********************************************************************/
IntersectionObserver.prototype.observe = function(t) {
var alertstring = "intersect with " + t;
if(t.eb$seqno) alertstring += "." + t.eb$seqno;
alert3(alertstring);
var e = new IntersectionObserverEntry;
e.target = t;
e.isIntersecting = true; // target is visible
e.intersectingRatio = 1.0; // the whole target is visible
// bounding rectangle is just the whole damn screen,
// hope nobody ever looks at it or expects it to be real.
e.boundingClientRect = this.root ? this.root.getBoundingClientRect() : document.getBoundingClientRect();
// I don't even know what these are!
e.rootBounds = e.intersectionRect = e.boundingClientRect;
// I guess we're ready to roll
this.callback([e]);
// in edbrowse the target remains visible forever, callback will never be called again.
// We don't have to remember target or the conditions of intersection etc.
}

// more visual stuff. But nothing resizes in edbrowse, ever,
// so this should be easy to stub out.
swm("ResizeObserver", function(){})
ResizeObserver.prototype.disconnect = eb$voidfunction;
ResizeObserver.prototype.observe = eb$voidfunction;
ResizeObserver.prototype.unobserve = eb$voidfunction;

// Trivial implementation of queueMicrotask that completely destroys the
// reason for its existence.
swm1("queueMicrotask", function(f) {
if(typeof f == "function") f()})

// don't need these any more
delete swm;
delete sdm;
delete swm1;
delete sdm1;
delete swm2;
delete sdm2;
delete spdc;
