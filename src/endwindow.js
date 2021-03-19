// Use this as a stub if you don't need deminimization, and are not including third.js

if(!mw$.compiled) {
// Mark the master window as irrevocably compiled.
Object.defineProperty(mw$, "compiled", {value:true, writable:false, configurable:false, enumerable:true});
}
