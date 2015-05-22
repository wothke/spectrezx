/*
* SpectreZX
* =========
*
* 	Copyright (C) 2015 Juergen Wothke
*
* Based on original C code of ZXTune (see http://zxtune.bitbucket.org/).
*
* LICENSE
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or (at
* your option) any later version. This library is distributed in the hope
* that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

This is a JavaScript/WebAudio version of ZXTune: It plays mostly ZX Spectrum chiptune music using tracker formats like: Chip Tracker v1.xx, Digital Music Maker, Digital Studio AY/Covox, Extreme Tracker v1.xx, ProDigiTracker v0.xx, SQ Digital Tracker, Sample Tracker. (It also supports various packed and archived file formats.)

This "project" is based on ZXTune version "zxtune-r3150": The project includes most of the original code including the 3rd party dependencies. (Some of the unused 'boost' stuff was stripped off and RAR support was also removed.)

All the "Web" specific additions (i.e. the whole point of this project) are contained in the "emscripten" subfolder. The main interface between the JavaScript/WebAudio world and the original C++ code is the adapter.cpp file.


Tech note:

Unfortunatly the original ZXTune implementaton uses a non-portable approach of reading memory areas (i.e. input music files) directly into "packed" structs as a means to interpret them. While this approach may work on architectures that allow unaligned data access, it does NOT work here. Example: A compiler may expect a "short*" (16-bit) to point to an "even" memory addrsss and respective access logic is then based on that assumption. If that access logic later is used with some "odd" input address, chances are that it will fail miserably. The biggest effort of this project therefore was to patch the original ZXTune impl with more robust access logic for unaligned data (You may find the respectice patched areas by diff-ing with the original "zxtune-r3150" version or by searching for the keyword "EMSCRIPTEN".). 

Eventhough the code now seems to work reasonably well, it may still require additional fixes that I have missed. For the benefit of those that may wish to resume the fixing here some background infos: 

1) Packed structs that contain built-in types or arrays of built-in types are fine:
	struct Dummy {
		short someArray[100];
	} __attribute__ ((packed))

	Even if a respective struct were populated from some unaligned memory address the compiler would correctly access the VALUES, e.g.
	
	short val= s->someArray[x]; // fine: the compiler will recognize the need for unaligned access (if any)
	
2)	but the compiler will no longer know about unaligned data when simple pointers are involved:
	
	short *ptr= &s->someArray[x];	// fail!
	
	supposing that pointer is passed as an argument to some function that wishes to update the original memory location

	void f(short *ptr) {
		*ptr= 100;			// fail! (if memory address is not properly aligned)
	}

3) to mitigate the above issue make sure to only propagate pointers to the packed structs - so that the compiler knows what needs to be done, e.g. you may want to introduce extra wrappers:

	struct unalignedShort {
		short val;
	} __attribute__ ((packed))

	void f(unalignedShort *ptr) {
		ptr->val= 100;			// fine: compiler will know what to do..
	}

4) Caution: From a usage point of view boost::array<..> may look like a built-in array but from an implementation point of view it behaves very differently: Its access logic depends on the proper alignment of the array and it just won't work if it isn't aligned! The same is true for any NON-built-in type. 



Howto build:

You'll need Emscripten (I used the win installer on WinXP: emsdk-1.13.0-full-32bit.exe which could be found here: 
http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html) I did not need to perform 
ANY additions or manual changes on the installation. The below instructions assume that the spectrezx project 
folder has been moved into the main emscripten folder (maybe not necessary) and that a command prompt has been 
opened within the spectrezx/emscripten sub-folder, and that the Emscripten environment vars have been set (run emsdk_env.bat).

Running the makeEmscripten.bat will generate a JavaScript 'ZXTune' library (web_zxtune.js) including the above mentioned 
interface APIs. This lib is directly written into the web-page example in the "htdocs" sub-folder. (This generated lib is 
used from some manually written JavaScript/WebAudio code, see htdocs/sample_player.js). Study the example in "htdocs" 
for how the player is used.


