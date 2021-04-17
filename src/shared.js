// stringname=sharedJS
/*********************************************************************
Javascript that loads and runs in the master window.
Functions and classses defined here can be shared by all the edbrowse windows,
if we're very careful!
We have to make sure nothing can be hijacked, starting with the Object.
p = document.createElement("p"); p.toString();
A website might innocently do that.
Nail down prototype and some methods that might innocently be called.
*********************************************************************/

Object.defineProperty(this, "Object",{writable:false,configurable:false});
Object.defineProperty(Object, "prototype",{writable:false,configurable:false});
Object.defineProperty(Object.prototype, "toString",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "toLocaleString",{enumerable:false,writable:false,configurable:false});
// demin.js sets constructor to Object, which it was before, but that means,
// I can't nail this down until demin.js has run its course.
// Object.defineProperty(Object.prototype, "constructor",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "valueOf",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "hasOwnProperty",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "isPrototypeOf",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Object.prototype, "propertyIsEnumerable",{enumerable:false,writable:false,configurable:false});

Object.defineProperty(this, "Function",{writable:false,configurable:false});
Object.defineProperty(Function, "prototype",{writable:false,configurable:false});
Object.defineProperty(Function.prototype, "call",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "apply",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "bind",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "toString",{enumerable:false,writable:false,configurable:false});
Object.defineProperty(Function.prototype, "constructor",{enumerable:false,writable:false,configurable:false});

alert = eb$puts;
// print an error inline, at debug level 3 or higher.
function alert3(s) { eb$logputs(3, s); }
function alert4(s) { eb$logputs(4, s); }

// Dump the tree below a node, this is for debugging.
// Print the first line of text for a text node, and no braces
// because nothing should be below a text node.
// You can make this more elaborate and informative if you wish.
function dumptree(top) {
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
if(top.dom$class == "Frame") {
if(top.eb$expf) top.contentWindow.dumptree(top.contentDocument);
} else if(top.childNodes) {
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
dumptree(c);
}
}
alert("}");
}

function uptrace(t) {
while(t) {
var msg = t.nodeName;
if(t.class) msg += "." + t.class;
if(t.id) msg += "#" + t.id;
alert(msg);
t = t.parentNode;
}
}

// implementation of getElementsByTagName, getElementsByName, and getElementsByClassName.
// These are recursive as they descend through the tree of nodes.

function getElementsByTagName(s) {
if(!s) { // missing or null argument
alert3("getElementsByTagName(type " + typeof s + ")");
return [];
}
s = s.toLowerCase();
return eb$gebtn(this, s);
}

function eb$gebtn(top, s) {
var a = [];
if(s === '*' || (top.nodeName && top.nodeName.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
// don't descend into another frame.
// The frame has no children through childNodes, so we don't really need this line.
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebtn(c, s));
}
}
return a;
}

function getElementsByName(s) {
if(!s) { // missing or null argument
alert3("getElementsByName(type " + typeof s + ")");
return [];
}
s = s.toLowerCase();
return eb$gebn(this, s);
}

function eb$gebn(top, s) {
var a = [];
if(s === '*' || (top.name && top.name.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebn(c, s));
}
}
return a;
}

function getElementById(s) {
if(!s) { // missing or null argument
alert3("getElementById(type " + typeof s + ")");
return null;
}
s = s.toLowerCase();
var a = eb$gebi(this, s);
return a.length ? a[0] : null;
}

// this could stop when it finds the first match, it just doesn't
function eb$gebi(top, s) {
var a = [];
if(s === '*' || (top.id && top.id.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebi(c, s));
}
}
return a;
}

function getElementsByClassName(s) {
s = s.toLowerCase();
return eb$gebcn(this, s);
}

function eb$gebcn(top, s) {
var a = [];
if(s === '*' || (top.class && top.class.toLowerCase() === s))
a.push(top);
if(top.childNodes) {
if(top.dom$class != "Frame")
for(var i=0; i<top.childNodes.length; ++i) {
var c = top.childNodes[i];
a = a.concat(eb$gebcn(c, s));
}
}
return a;
}

function nodeContains(n) {  return eb$cont(this, n); }

function eb$cont(top, n) {
if(top === n) return true;
if(!top.childNodes) return false;
if(top.dom$class == "Frame") return false;
for(var i=0; i<top.childNodes.length; ++i)
if(eb$cont(top.childNodes[i], n)) return true;
return false;
}

// lock down
var flist = ["alert","alert3","alert4","dumptree","uptrace",
"eb$newLocation","eb$logElement",
"getElementsByTagName", "getElementsByClassName", "getElementsByName", "getElementById","nodeContains",
"eb$gebtn","eb$gebn","eb$gebcn","eb$gebid","eb$cont",
];
for(var i=0; i<flist.length; ++i)
Object.defineProperty(this, flist[i], {writable:false,configurable:false});
