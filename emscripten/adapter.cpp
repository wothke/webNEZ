/*
* This file adapts nezplay/NETplug++ to the interface expected by my generic JavaScript player..
*
* Copyright (C) 2018 Juergen Wothke
*
*
* Credits:   NEZplug++ by Mamiya (and nezplay by BouKiCHi)
* 
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


/*
	where to find test files: 
	
	*.sgc (SGC) https://www.zophar.net/music/sega-game-gear-sgc/ (some files use some vgm format that doesn't work here)
	.kss (KSSX, KSCC, ) https://www.zophar.net/music/kss
	.hes (HESM) https://ftp.modland.com/pub/modules/HES/
	.nsf (NESM) https://ftp.modland.com/pub/modules/Nintendo%20Sound%20Format/ 
	.ay, cpc.emul (ZXAY) https://zxart.ee/eng/music/ , https://ftp.modland.com/pub/modules/AY%20Emul/
	.gbr(GBRF, GBS) https://ftp.modland.com/pub/modules/Gameboy%20Sound%20System%20GBR/
	(NESL)
	
*/

#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>     

#include <iostream>
#include <fstream>

//#include <handler.h>
#include <nezplug.h>
#include <audiosys.h>
#include <songinfo.h>


#ifdef EMSCRIPTEN
#define EMSCRIPTEN_KEEPALIVE __attribute__((used))
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

#define CHANNELS 2				
#define BYTES_PER_SAMPLE 2
#define SAMPLE_BUF_SIZE	1024
#define SAMPLE_FREQ	44100


int16_t sample_buffer[SAMPLE_BUF_SIZE * CHANNELS];
int samples_available= 0;

char* info_texts[4];

#define TEXT_MAX	255
char title_str[TEXT_MAX];
char track_str[TEXT_MAX];
char artist_str[TEXT_MAX];
char copyright_str[TEXT_MAX];

NEZ_PLAY *nezPlay= NULL;

struct StaticBlock {
    StaticBlock(){
		info_texts[0]= title_str;
		info_texts[1]= track_str;
		info_texts[2]= artist_str;
		info_texts[3]= copyright_str;		
    }
};

static StaticBlock g_emscripen_info;

void* buffer_copy= 0;

void trash_buffer_copy() {
	if (buffer_copy) {
		free(buffer_copy);
		buffer_copy= 0;		
	}
}

void * get_buffer_copy(const void * inBuffer, size_t inBufSize) {
	trash_buffer_copy();
	
	buffer_copy= malloc(inBufSize);
	memcpy ( buffer_copy, inBuffer, inBufSize );
	return buffer_copy;
}

extern "C" void emu_teardown (void)  __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_teardown (void) {
	if (nezPlay != NULL) {
		NEZDelete(nezPlay);
		nezPlay= NULL;		
	}
}

extern int NSF_noise_random_reset;
extern int NSF_2A03Type;
extern int Namco106_Realmode;
extern int Namco106_Volume;
extern int GBAMode;
extern int MSXPSGType;
extern int MSXPSGVolume;
extern int FDS_RealMode;
extern int LowPassFilterLevel;
extern int NESAPUVolume;
extern int NESRealDAC;
extern int Always_stereo;

int emu_setup()
{
	nezPlay= NEZNew();	// defaults: frequency = 48000; channel = 1; naf_type = NES_AUDIO_FILTER_NONE;

	NEZSetFrequency(nezPlay, SAMPLE_FREQ);
	NEZSetChannel(nezPlay, CHANNELS);

	NSF_noise_random_reset = 0;
	NSF_2A03Type= 1;
	Namco106_Realmode= 1;
	Namco106_Volume= 16;
	GBAMode= 0;
	MSXPSGType= 1;
	MSXPSGVolume= 64;
	FDS_RealMode= 3;
	LowPassFilterLevel= 16; 
	NESAPUVolume= 64; 
	NESRealDAC = 1;
	Always_stereo= 1;

	return 1;
}

extern "C"  int emu_load_file(char *filename, void * inBuffer, uint32_t inBufSize)  __attribute__((noinline));
extern "C"  int EMSCRIPTEN_KEEPALIVE emu_load_file(char *filename, void * inBuffer, uint32_t inBufSize) {
	// it seems that nezPlug corrupts the used input data and Emscripten passes its original
	// cached data -- so we better use a copy..
	inBuffer= get_buffer_copy(inBuffer, inBufSize);

	if (emu_setup()) {
		SONGINFO_Reset(nezPlay->song);	// e.g. HES does not set anything..
		
		if (!NEZLoad(nezPlay, (uint8_t *)inBuffer, inBufSize)) {
			
			std::string title= SONGINFO_GetTitle(nezPlay->song);
			if (title.length() == 0) {
				title= filename;
				title.erase( title.find_last_of( '.' ) );	// remove ext
			}			
			snprintf(title_str, TEXT_MAX, "%s", title.c_str());
			
			snprintf(artist_str, TEXT_MAX, "%s", trim(SONGINFO_GetArtist(nezPlay->song)).c_str());
			snprintf(copyright_str, TEXT_MAX, "%s", trim(SONGINFO_GetCopyright(nezPlay->song)).c_str());
			
			return 0;		
		}
	} else {
		// error
	}
	return 1;
}

extern "C" int emu_get_sample_rate() __attribute__((noinline));
extern "C" EMSCRIPTEN_KEEPALIVE int emu_get_sample_rate()
{
	return SAMPLE_FREQ;
}
 
extern "C" int emu_set_subsong(int subsong, unsigned char boost) __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_set_subsong(int track, unsigned char boost) {
	// .hes files do contain multiple tracks but used IDs are not obvious
	
	if (track >= 0) {
		NEZSetSongNo(nezPlay, track );
	}
	NEZReset(nezPlay);		// without this there is no sound..
	
//	NEZSetFrequency(nezPlay, SAMPLE_FREQ);
//	NEZSetChannel(nezPlay, CHANNELS);

	track=  NEZGetSongNo(nezPlay);	// also read default when not set explicitly

	snprintf(track_str, TEXT_MAX, "%d", track);

	return 0;
}

extern "C" const char** emu_get_track_info() __attribute__((noinline));
extern "C" const char** EMSCRIPTEN_KEEPALIVE emu_get_track_info() {
	return (const char**)info_texts;
}

extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) __attribute__((noinline));
extern "C" char* EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer(void) {
	return (char*)sample_buffer;
}

extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) __attribute__((noinline));
extern "C" long EMSCRIPTEN_KEEPALIVE emu_get_audio_buffer_length(void) {
	return samples_available;
}

extern "C" int emu_compute_audio_samples() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_compute_audio_samples() {
	NEZRender(nezPlay, sample_buffer, SAMPLE_BUF_SIZE);
	samples_available= SAMPLE_BUF_SIZE;
	
	return 0;
}

extern "C" int emu_get_current_position() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_get_current_position() {
    return -1; // not implemented
}

extern "C" void emu_seek_position(int pos) __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_seek_position(int frames) {
}

extern "C" int emu_get_max_position() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_get_max_position() {
	return -1; // not implemented
}

