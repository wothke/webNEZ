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

#include <handler.h>
#include <audiosys.h>
#include <songinfo.h>
#include <m_nsf.h>


#ifdef EMSCRIPTEN
#define EMSCRIPTEN_KEEPALIVE __attribute__((used))
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

extern "C" Uint NEZLoad(Uint8 *pData, Uint uSize);

Uint8 chmask[0x80]={
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};


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

struct StaticBlock {
    StaticBlock(){
		info_texts[0]= title_str;
		info_texts[1]= track_str;
		info_texts[2]= artist_str;
		info_texts[3]= copyright_str;
    }
};

static StaticBlock g_emscripen_info;

void* buffer_copy= NULL;

void trash_buffer_copy() {
	if (buffer_copy) {
		free(buffer_copy);
		buffer_copy= NULL;		
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
	trash_buffer_copy();
		
    NESTerminate();
}


NSF_STATE nsf_state;

int emu_setup()
{
   // NESAudioSetStrict(1);	// what might this be for?
    NESReset();        
	return 1;
}

/*
static int nsf_volume = 0;

void SetVolumeNSF(int vol)
{
    nsf_volume = vol;
    NESVolume( nsf_volume );    
}
*/
void ResetNSF(void)
{
	NESReset();        
//	NESVolume( nsf_volume );
}

extern "C"  int emu_load_file(char *filename, void * inBuffer, uint32_t inBufSize)  __attribute__((noinline));
extern "C"  int EMSCRIPTEN_KEEPALIVE emu_load_file(char *filename, void * inBuffer, uint32_t inBufSize) {
	// it seems that nezPlug corrupts the used input data and Emscripten passes its original
	// cached data -- so we better use a copy..
	inBuffer= get_buffer_copy(inBuffer, inBufSize);

	if (emu_setup()) {
		SONGINFO_Reset();	// e.g. HES does not set anything..
		
		if (!NEZLoad((uint8_t *)inBuffer, inBufSize)) {
			
			NESAudioFrequencySet(SAMPLE_FREQ);
			NESAudioChannelSet(CHANNELS);
		
			ResetNSF();

			std::string title= SONGINFO_GetTitle();
			if (title.length() == 0) {
				title= filename;
				title.erase( title.find_last_of( '.' ) );	// remove ext
			}			
			snprintf(title_str, TEXT_MAX, "%s", title.c_str());
			
			snprintf(artist_str, TEXT_MAX, "%s", trim(SONGINFO_GetArtist()).c_str());
			snprintf(copyright_str, TEXT_MAX, "%s", trim(SONGINFO_GetCopyright()).c_str());
			
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
	// .hes files to contain multiple tracks but used IDs are not obvious
	
	if (track >= 0) {
		SONGINFO_SetSongNo( track );
	}
	ResetNSF();

	track=  SONGINFO_GetSongNo();	// also read default when not set explicitly

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
	NESAudioRender( sample_buffer, SAMPLE_BUF_SIZE );
	samples_available= SAMPLE_BUF_SIZE;
	
	return 0;
}

extern "C" int emu_get_current_position() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_get_current_position() {
    return NESGetFrame();
}

extern "C" void emu_seek_position(int pos) __attribute__((noinline));
extern "C" void EMSCRIPTEN_KEEPALIVE emu_seek_position(int frames) {
//	NESGetFramePerSeconds() NESGetFrameCycle();  
	
    int current = NESGetFrame();
    
    // if negative, seek is cancelled
    if (frames < 0)
    {
        NESSeekFrame(current);
        return;
    }
    
    // reset if the frame is passed
    if (frames < current)
        ResetNSF();
    
    NESSeekFrame(frames);
}

extern "C" int emu_get_max_position() __attribute__((noinline));
extern "C" int EMSCRIPTEN_KEEPALIVE emu_get_max_position() {
	return NESGetMaxPosition();
}

