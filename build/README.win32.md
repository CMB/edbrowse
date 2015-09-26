# Win32 Build

This is a WIN32 build of edbrowse, using cmake for configuration, and generation of the MSVC solution files...

##### Dependencies

 - Tidy - https://github.com/htacg/tidy-html5
 - CURL - http://curl.haxx.se/
 - PCRE - various...
 - Readline - source url?
 - Mozjs-24 - TODO: source url and build - Still working on this.
 - others? ...
 
It is asssumed the first four are already installed. Binary installs can be used, and each ROOT install location noted. The cmake configuration step will try to find them...

The configure will FAIL if they are not found. A cmake option can be used to help in the finding. Add like `-DCMAKE_PREFIX_PATH:PATH=C:\PF\Tidy;F:\Projects\software;...` for each install path. Some care usually needed using path names with spaces.

##### Building

A command line build, starting with cloning `edbrowse` is -

```
> git clone git@github.com:geoffmcl/edbrowse.git edbrowse
> cd edbrowse\build
> # set TMPOPS=-DCMAKE_INSTALL_PREFIX:PATH=..\..\3rdParty -DCMAKE_BUILD_TYPE=Release
> cmake .. %TMPOPS%
> cmake --build . --configure Release
```

The build folder contains a `build-me.bat`, which may need to be adjusted to suit your environment... should find your installed MSVC and write solution files sutable for your version.

##### History

20150924: Cloned original repo - https://github.com/CMB/edbrowse - to my repos - https://github.com/geoffmcl/edbrowse - and started a build.

Using a simple directory structure -

 - F:\Projects - the root
 -  ...  \tidy-html5
 -  ...  \edbrowse
 -  ...  \readline-5.0.0
 -  ...  \curl-7.42.1
 -  ...  \other dependencies
 -  ...  \software - each dependency installed here

This simple stucture helps a lot with the cmake find package configuration...

Created, using src\makefile, guessed at, the root edbrowse\CMakeLists.txt file, and added some cmake find modules in edbrowse\CMakeModules, for Tidy, PCRE, Readline, ...

Created an edbrowse\build folder, and added a `build-me.bat`, errors to bldlog-1.txt...

Had to break some things to get this first build done, but hopefully these can be fixed in a cross platform way...

One of them is the use of `fork()` to do background loads. There seems **no** suitable Win32 port of this available. An easy alternative is to use pthread (POSIX thread), which does have good Win32 support. Code changes are usually releative minor...

Enjoy.

Geoff. 20150921

; eof
