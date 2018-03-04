#include "songinfo.h"

static struct
{
	Uint songno;
	Uint maxsongno;
	Uint startsongno;
	Uint extdevice;
	Uint initaddress;
	Uint playaddress;
	Uint channel;
	Uint initlimit;
    Uint isnsf;
    
    char title[128];
	
	// hm.. nezplay is not even using these...
    char artist[128];	
    char copyright[128];
} local = {
	0,0,0,0,0,0,1,0,
    0, // isnsf
    "",
    "",
    ""
};

Uint SONGINFO_GetSongNo(void)
{
	return local.songno;
}
void SONGINFO_SetSongNo(Uint v)
{
	if (local.maxsongno && v > local.maxsongno) v = local.maxsongno;
	if (v == 0) v++;
	local.songno = v;
}
Uint SONGINFO_GetStartSongNo(void)
{
	return local.startsongno;
}
void SONGINFO_SetStartSongNo(Uint v)
{
	local.startsongno = v;
}
Uint SONGINFO_GetMaxSongNo(void)
{
	return local.maxsongno;
}
void SONGINFO_SetMaxSongNo(Uint v)
{
	local.maxsongno = v;
}
Uint SONGINFO_GetExtendDevice(void)
{
	return local.extdevice;
}
void SONGINFO_SetExtendDevice(Uint v)
{
	local.extdevice = v;
}
Uint SONGINFO_GetInitAddress(void)
{
	return local.initaddress;
}
void SONGINFO_SetInitAddress(Uint v)
{
	local.initaddress = v;
}
Uint SONGINFO_GetPlayAddress(void)
{
	return local.playaddress;
}
void SONGINFO_SetPlayAddress(Uint v)
{
	local.playaddress = v;
}
Uint SONGINFO_GetChannel(void)
{
	return local.channel;
}
void SONGINFO_SetChannel(Uint v)
{
	local.channel = v;
}

Uint SONGINFO_GetInitializeLimiter(void)
{
	return local.initlimit;
}
void SONGINFO_SetInitializeLimiter(Uint v)
{
	local.initlimit = v;
}

void SONGINFO_SetTitle(const char *title)
{
    strcpy(local.title, title);
}

char *SONGINFO_GetTitle(void)
{
    return local.title;
}

void SONGINFO_SetArtist(const char *artist)
{
    strcpy(local.artist, artist);
}

char *SONGINFO_GetArtist(void)
{
    return local.artist;
}
void SONGINFO_SetCopyright(const char *copyright)
{
    strcpy(local.copyright, copyright);
}

char *SONGINFO_GetCopyright(void)
{
    return local.copyright;
}

void SONGINFO_Reset(void) {
	local.songno= 0;
	local.maxsongno= 0;
	local.startsongno= 0;
	local.extdevice= 0;
	local.initaddress= 0;
	local.playaddress= 0;
	local.channel= 0;
	local.initlimit= 0;
    local.isnsf= 0;
	SONGINFO_SetTitle("");
	SONGINFO_SetArtist("");
	SONGINFO_SetCopyright("");
}

Uint SONGINFO_GetNSFflag(void)
{
	return local.isnsf;
}
void SONGINFO_SetNSFflag(Uint v)
{
	local.isnsf = v;
}
