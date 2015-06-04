/*
* This file adapts "ZXTune" to the interface expected by my generic JavaScript player..
*
* Copyright (C) 2015 Juergen Wothke
*
* LICENSE
* 
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or (at
* your option) any later version. This library is distributed in the hope
* that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <emscripten.h>
#include <stdio.h>
#include <contract.h>
#include <pointers.h>
#include <types.h>
#include <stdlib.h>     /* malloc, free, rand */

#include <exception>
#include <iostream>
#include <fstream>

#include "spectre.h"

#ifdef EMSCRIPTEN
#define EMSCRIPTEN_KEEPALIVE __attribute__((used))
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

// for some reason my old Emscripten version DOES NOT provide std:exception
// with its stdlib... if you are building with a better version then disable this 
// BLOODY_HACK
#ifdef BLOODY_HACK 
namespace std
{
exception::~exception() _NOEXCEPT
{
}

const char* exception::what() const _NOEXCEPT
{
  return "std::exception";
}
}
#endif

#define BUF_SIZE	1024
#define TEXT_MAX	255
#define NUM_MAX	15

// see Sound::Sample::CHANNELS
#define CHANNELS 2				
#define BYTES_PER_SAMPLE 2
#define SAMPLE_BUF_SIZE	8192
char sample_buffer[SAMPLE_BUF_SIZE * BYTES_PER_SAMPLE * CHANNELS];
int samples_available= 0;

extern int guiSamplesRate;

const char* info_texts[7];

char title_str[TEXT_MAX];
char author_str[TEXT_MAX];
char desc_str[TEXT_MAX];
char program_str[TEXT_MAX];
char subpath_str[TEXT_MAX];
char tracks_str[NUM_MAX];
char current_track_str[NUM_MAX];

struct StaticBlock {
    StaticBlock(){
		info_texts[0]= title_str;
		info_texts[1]= author_str;
		info_texts[2]= desc_str;
		info_texts[3]= program_str;
		info_texts[4]= subpath_str;
		info_texts[5]= tracks_str;
		info_texts[6]= current_track_str;
    }
};

static StaticBlock g_emscripen_info;

namespace
{
	static ZxTuneWrapper *g_zxtune= 0;
	static SongInfo g_songinfo;

	void openFile(const std::string& name, Dump& result)
	{
		std::ifstream stream(name.c_str(), std::ios::binary);
		if (!stream) {
			throw std::runtime_error("Failed to open " + name);
		}
		stream.seekg(0, std::ios_base::end);
		const std::size_t size = stream.tellg();
		stream.seekg(0);
		Dump tmp(size);
		stream.read(safe_ptr_cast<char*>(&tmp[0]), tmp.size());
		result.swap(tmp);
	}
	
	void teardownZxTune() {
		if (g_zxtune != 0) {
			delete g_zxtune;
			g_zxtune= 0;
		}	
	}
	
	void createZxTune(char *songmodule, Dump &dump) {
		if (g_zxtune != 0)
			delete g_zxtune;
			
		std::string name(songmodule);
		g_zxtune= new ZxTuneWrapper(name,  &dump.front(), dump.size());		
		g_zxtune->parse_modules();
	}
	
	void selectZxTune(int subsong) {
		subsong+=1;	// JavaScript uses 0..n but zxTune 1..n+1
		g_zxtune->get_song_info(subsong, g_songinfo);
		g_zxtune->decode_initialize(subsong, g_songinfo);	
	}
	
	int render_sound(void* buffer, size_t samples) {
	  return  g_zxtune->render_sound(buffer, samples);
	}
}

extern "C" void emu_teardown (void)  __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_teardown (void) {
	teardownZxTune();
}

// confirm effective sample rate
extern "C" int emu_get_sample_rate() __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_get_sample_rate()
{
	return guiSamplesRate;
}

extern "C" int emu_init(int sample_rate, char *basedir, char *songmodule) __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_init(int sample_rate, char *basedir, char *songmodule)
{
	guiSamplesRate= sample_rate;

	emu_teardown();
	
	try	{  
		std::string filename(songmodule);
		Dump dump;
		openFile(filename, dump);
		
		createZxTune(songmodule, dump);
	} catch (const std::exception&) {
		std::cerr << "fatal error: "<< std::endl;		
		return -1;
	}	
	return 0;
}

extern "C" void emu_set_subsong(int subsong) __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_set_subsong(int subsong) {
	selectZxTune(subsong);
}

extern "C" const char** emu_get_track_info() __attribute__((noinline));
extern "C" const char** EMSCRIPTEN_KEEPALIVE emu_get_track_info() {
	snprintf(title_str, TEXT_MAX, "%s", g_songinfo.get_title());
	snprintf(author_str, TEXT_MAX, "%s", g_songinfo.get_author());
	snprintf(desc_str, TEXT_MAX, "%s", g_songinfo.get_comment());
	snprintf(program_str, TEXT_MAX, "%s", g_songinfo.get_program());
	snprintf(subpath_str, TEXT_MAX, "%s", g_songinfo.get_subpath());
	snprintf(tracks_str, NUM_MAX, "%d",  g_songinfo.get_total_tracks());
	snprintf(current_track_str, NUM_MAX, "%d",  g_songinfo.get_track_number());
	return info_texts;
}

extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) __attribute__((noinline));
extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) {
	return sample_buffer;
}

extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) __attribute__((noinline));
extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) {
	return samples_available;
}

extern "C" int emu_compute_audio_samples() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_compute_audio_samples() {
  int size= render_sound(sample_buffer, SAMPLE_BUF_SIZE);

	if (size) {
		samples_available= size;
		return 0;	// we might want to handle errors (-1) separately..
	} else {
		return 1;
	}
}

