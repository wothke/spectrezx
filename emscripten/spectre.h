/**
* Exposes those parts of the ZxTune interface that are currently used by SpectreZX player.
* code based on "foobar2000 plugin"
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
**/

#pragma once

#include <stddef.h>
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZXTUNE_API_EXPORT
#define ZXTUNE_API_IMPORT

#ifndef ZXTUNE_API
#define ZXTUNE_API ZXTUNE_API_IMPORT
#endif

// due to the lack of a C++ concept for proper separation of interface and implementation
// a PIMPL hack is used here to at least hide most of the implementation details..

class SongInfo;

class ZxTuneWrapper {
	friend class SongInfo;
public:
	ZxTuneWrapper(std::string p, const void* data, size_t size);
	virtual ~ZxTuneWrapper();

	void parse_modules();
	void decode_initialize(unsigned int p_subsong, SongInfo & p_info);
	void get_song_info(unsigned int p_subsong, SongInfo & p_info);
	int render_sound(void* buffer, size_t samples);
	
	int get_current_position();
	int get_max_position();
	void seek_position(int pos);
protected:
	class ZxTuneWrapperImpl;
    ZxTuneWrapperImpl* pimpl_;
};

class SongInfo {
	friend class ZxTuneWrapper::ZxTuneWrapperImpl;
public:
	SongInfo();
	virtual ~SongInfo();

	const void reset();
	
	const char *get_codec();
	const char *get_author();
	const char *get_title();
	const char *get_subpath();
	const char *get_comment();
	const char *get_program();
	int get_track_number();	
	int get_total_tracks();
protected:
	class SongInfoImpl;
    SongInfoImpl* pimpl_;
};

#ifdef __cplusplus
} //extern
#endif
