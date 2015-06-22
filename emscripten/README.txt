SpectreZX - WebAudio plugin of ZXTune
=========

	Copyright (C) 2015 Juergen Wothke

	LICENSE
		See individual files for specific licensing information


SpectreZX is based on ZXTune (zxtune-r3150). Some unused folders have been completely removed from "3rdparty" (curl.dll, FLAC, lame, ogg, runtime, sidplayfp, unrar, vorbis, xmp), and also folders "apps" & "test". The "boost" (partial; version 1.57.0) folder was added to remove the external dependency.  Disabled some unused code via ifdef EMSCRIPTEN (e.g. SID/XMP). Unfortunately the original source exploits undefined compiler behavior - which does not work with clang++/Emscripten: The code uses unaligned boost::array instances within packed arrays.. which is a programming error because the "this" based array operations will lose the "packed array" context making it impossible to reliably access the potentially unaligned array content... I had therefore to make various fixes in the respective player impls (see "formats" folder). (PS: the code must be compiled using -DNO_DEBUG_LOGS because the built-in debug-messages will actually crash some of the players...)

Everything needed for the WebAudio version is contained in this "emscripten" folder. The 
original ZXTune code is almost unchanged (see above), i.e. should be trivial to 
merge future ZXTune fixes or updates.


You'll need Emscripten (I used the win installer on WinXP: emsdk-1.13.0-full-32bit.exe which 
could - at the time - be found here: http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html) 

I did not need to perform ANY additions or manual changes on the installation. The below 
instructions assume that the spectreZX project folder has been moved into the main emscripten 
installation folder (maybe not necessary) and that a command prompt has been opened within the 
project's "emscripten" sub-folder, and that the Emscripten environment vars have been previously 
set (run emsdk_env.bat).


Howto build (on Windows):

The Web version is built using the makeEmscripten.bat that can be found in this folder. The 
script will compile directly into the "emscripten/htdocs" example web folder, were it will create 
the backend_zxtune.js library. The content of the "htdocs" can be tested by first copying into some 
document folder of a web server (this running example shows how the code is used). 


Background information:

Because the original implementation was not touched, the code doesn't cope with the async file 
loading approach normally taken by web apps. (You might want to have a look at my WebUADE for an 
example of a lib that has been patched to deal with load-on-demand..) Fortunately ZXTune currently 
does not seem to use any on demand file loads - if this ever was to change, then the current web 
port would need to be upgraded.