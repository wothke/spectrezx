/**
* Impl of ZxTune interface used by SpectreZX player
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

//local includes
#define ZXTUNE_API ZXTUNE_API_EXPORT
#include "spectre.h"
//common includes
#include <contract.h>
#include "cycle_buffer.h"
#include "error_tools.h"
#include "progress_callback.h"
//library includes
#include "binary/container.h"
#include "binary/container_factories.h"
#include "core/core_parameters.h"
#include "core/data_location.h"
#include "core/module_attrs.h"
#include "core/module_detect.h"
#include "core/module_open.h"
#include "core/module_holder.h"
#include "core/module_player.h"
#include "core/module_types.h"
#include "parameters/container.h"
#include "platform/version/api.h"
#include "sound/sound_parameters.h"
#include "sound/service.h"


//std includes
#include <map>
#include <iostream>
#include <string>
#include <stdexcept>

//boost includes
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/static_assert.hpp>
#include <boost/range/end.hpp>
#include <boost/type_traits/is_signed.hpp>

#include "player.h"

int guiSamplesRate= 44100;	// override ZxTune defaults so it is in sync with the needs of the GUI

// -------------------- SongInfo -------------------

static const char *EMPTY= "";

class SongInfo::SongInfoImpl {
public:
	SongInfoImpl() {}
	virtual ~SongInfoImpl() {}

	void set_codec(std::string c)		{ codec_= c.empty() ? std::string(EMPTY) : c; }
	const char *get_codec()		 		{ return codec_.c_str(); }

	void set_author(std::string a) 		{ author_= a.empty() ? std::string(EMPTY) : a;  }
	const char *get_author()		 	{ return author_.c_str(); }

	void set_title(std::string t) 		{ title_= t.empty() ? std::string(EMPTY) : t; }
	const char *get_title()		 		{ return title_.c_str(); }

	void set_sub_path(std::string n)	{ sub_path_= n.empty() ? std::string(EMPTY) : n; }
	const char *get_subpath()		 	{ return sub_path_.c_str(); } // archive path to specific sub-song
	
	void set_comment(std::string c) 	{ comment_= c.empty() ? std::string(EMPTY) : c; }
	const char *get_comment()		 	{ return comment_.c_str(); }
	
	void set_program(std::string p) 	{ program_= p.empty() ? std::string(EMPTY) : p; }
	const char *get_program()			{ return program_.c_str(); }
	
	void set_track_number(int n) 		{ track_number_= n;}
	int get_track_number()		 		{ return track_number_; }
	
	void set_total_tracks(int n) 		{ total_tracks_= n;}
	int get_total_tracks() 				{ return total_tracks_;}
		
private:
	std::string codec_;
	std::string author_;
	std::string title_;
	std::string sub_path_;
	std::string comment_;
	std::string program_;
	
	int track_number_;
	int total_tracks_;
	
	int total_positions_;
};

SongInfo::SongInfo() : pimpl_(new SongInfoImpl()) {}
SongInfo::~SongInfo() 						{ delete pimpl_; pimpl_= 0; }
const char *SongInfo::get_codec()		 	{ return pimpl_->get_codec(); }
const char *SongInfo::get_author()		 	{ return pimpl_->get_author(); }
const char *SongInfo::get_title()		 	{ return pimpl_->get_title(); }
const char *SongInfo::get_subpath()			{ return pimpl_->get_subpath(); }
const char *SongInfo::get_comment()		 	{ return pimpl_->get_comment(); }
const char *SongInfo::get_program()		 	{ return pimpl_->get_program(); }
int SongInfo::get_track_number()		 	{ return pimpl_->get_track_number(); }
int SongInfo::get_total_tracks()		 	{ return pimpl_->get_total_tracks(); }


// -------------------- ZxTuneWrapper -------------------

class ZxTuneWrapper::ZxTuneWrapperImpl {
public:
	ZxTuneWrapperImpl(std::string p, const void* data, size_t size) {
		m_file_path_= p; 
		input_file_= createData(data, size);
	}
	
	virtual ~ZxTuneWrapperImpl() {
		close();
	}
	
	void close() {
		if(input_file_)
		{
			input_player_.reset();
			input_modules_.clear();
			input_module_.reset();
			input_file_.reset();
		}
	}

	void parse_modules() {
		struct ModuleDetector : public Module::DetectCallback
		{
			ModuleDetector(Modules* _mods) : modules(_mods) {}
			virtual void ProcessModule(ZXTune::DataLocation::Ptr location, ZXTune::Plugin::Ptr, Module::Holder::Ptr holder) const
			{
				ModuleDesc m;
				m.module = holder;
				m.subpath = location->GetPath()->AsString();
				modules->push_back(m);
			}
			virtual Log::ProgressCallback* GetProgress() const { return NULL; }
			virtual Parameters::Accessor::Ptr GetPluginsParameters() const { return Parameters::Container::Create(); }
			Modules* modules;
		};

		ModuleDetector md(&input_modules_);
		Module::Detect(ZXTune::CreateLocation(input_file_), md);
		if(input_modules_.empty())
		{
			input_file_.reset();
			throw  std::invalid_argument("io unsupported format");
		}
	}
		
	void decode_initialize(unsigned int p_subsong, SongInfo & p_info) {
		std::string subpath = get_subpath(p_subsong, p_info);

		ZXTune::DataLocation::Ptr loc= ZXTune::OpenLocation(Parameters::Container::Create(), input_file_, subpath);
		input_module_ = Module::Open(loc);
		if(!input_module_)
			throw  std::invalid_argument("io unsupported format"); 

		Module::Information::Ptr mi = input_module_->GetModuleInformation();
		if(!mi)
			throw  std::invalid_argument("io unsupported format"); 

		input_player_ = PlayerWrapper::Create(input_module_, guiSamplesRate);
				
		if(!input_player_)
			throw  std::invalid_argument("io unsupported format"); 	

	   input_player_->GetRenderer()->SetPosition(0);			
	}
	
	void get_song_info(unsigned int p_subsong, SongInfo & p_info)	{
		SongInfo::SongInfoImpl &info= *(p_info.pimpl_);
	
		Module::Information::Ptr mi;
		Parameters::Accessor::Ptr props;
		std::string subpath;
		if(!input_modules_.empty())
		{
			if(p_subsong > input_modules_.size())
				throw  std::invalid_argument("io unsupported format");
			mi = input_modules_[p_subsong - 1].module->GetModuleInformation();
			props = input_modules_[p_subsong - 1].module->GetModuleProperties();
			if(!mi)
				throw  std::invalid_argument("io unsupported format");
			subpath = input_modules_[p_subsong - 1].subpath;
			
			info.set_sub_path(subpath);
		}
		else
		{
			subpath = get_subpath(p_subsong, p_info);
			Module::Holder::Ptr m = Module::Open(ZXTune::OpenLocation(Parameters::Container::Create(), input_file_, subpath));
			if(!m)
				throw  std::invalid_argument("io unsupported format");
			mi = m->GetModuleInformation();
			props = m->GetModuleProperties();
			if(!mi)
				throw  std::invalid_argument("io unsupported format");
		}
		
		String type;
		info.set_codec(props->FindValue(Module::ATTR_TYPE, type) ? type : std::string("undefined"));
		
		String author;
		info.set_author(props->FindValue(Module::ATTR_AUTHOR, author) ? author : std::string("undefined"));

		String title;
		if(props->FindValue(Module::ATTR_TITLE, title) && !title.empty())
			info.set_title(title);
		else
			info.set_title(subpath);
		
		String comment;
		info.set_comment(props->FindValue(Module::ATTR_COMMENT, comment) ? comment : std::string("undefined"));

		String program;
		info.set_program(props->FindValue(Module::ATTR_PROGRAM, program) ? program : std::string("undefined"));
		
		if(input_modules_.size() > 1)
		{
			info.set_track_number(p_subsong);
			info.set_total_tracks(input_modules_.size());
		} else {
			info.set_track_number(0);
			info.set_total_tracks(1);
		}
	}

	int render_sound(void* buffer, size_t samples) {	
		try {
			return input_player_->RenderSound(reinterpret_cast<Sound::Sample*>(buffer), samples);
		}
		catch (const Error&) {
			return -1;
		}
		catch (const std::exception&) {
			return -1;
		}
	}
	
	int get_current_position() {
		return input_player_->GetRenderer()->GetTrackState()->Frame();
	}
	
	int get_max_position() {
		return input_module_->GetModuleInformation()->FramesCount();
	}
	
	void seek_position(int pos) {
	   input_player_->GetRenderer()->SetPosition(pos);
	}
protected: 
	std::string get_subpath(unsigned int p_subsong, SongInfo & p_info) {
		const char* subpath = p_info.get_subpath();
		if(subpath && strlen(subpath))
			return std::string(subpath);
		return std::string();
	}
	
	Binary::Container::Ptr createData(const void* data, size_t size) {
		try	{
			Binary::Container::Ptr result = Binary::CreateContainer(data, size);	// use my existing "aligned" buffer
			return result;
		} catch (const std::exception& e) {
			std::cerr << "ERROR in createData() "<< e.what()<<std::endl;		
			return Binary::Container::Ptr();
		}
	}
	
private:
	std::string				m_file_path_;
	Binary::Container::Ptr	input_file_;

	struct ModuleDesc
	{
		Module::Holder::Ptr module;
		std::string subpath;
	};
	typedef std::vector<ModuleDesc> Modules;
	Modules					input_modules_;
	Module::Holder::Ptr		input_module_;
	PlayerWrapper::Ptr		input_player_;
};

ZxTuneWrapper::ZxTuneWrapper(std::string p, const void* data, size_t size) : pimpl_(new ZxTuneWrapper::ZxTuneWrapperImpl(p, data, size)) {
}

ZxTuneWrapper::~ZxTuneWrapper() { 
	delete pimpl_; pimpl_= 0;
}

void ZxTuneWrapper::parse_modules() {
	pimpl_->parse_modules();
}

void ZxTuneWrapper::decode_initialize(unsigned int p_subsong, SongInfo & p_info) {
	pimpl_->decode_initialize(p_subsong, p_info);
}

void ZxTuneWrapper::get_song_info(unsigned int p_subsong, SongInfo & p_info) {
	pimpl_->get_song_info(p_subsong, p_info);
}

int ZxTuneWrapper::render_sound(void* buffer, size_t samples) {
	return pimpl_->render_sound(buffer, samples);
}

int ZxTuneWrapper::get_current_position() {
	return pimpl_->get_current_position();
}
int ZxTuneWrapper::get_max_position() {
	return pimpl_->get_max_position();
}
void ZxTuneWrapper::seek_position(int pos) {
	pimpl_->seek_position(pos);
}


