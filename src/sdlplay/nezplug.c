// main dispatcher (for some reason "nezplay" mangled this code directly into "m_nsf.c" (when "migrating
// to EMSCRIPTEN" I put the code back here - like in NEZplug++ - and I also put the NEZplug++ impl for
// m_nsf.c and m_nsd.c "back" in place) .. it seems to be much cleaner to separate the different
// format-handlers from the initial dispatching

#include "neserr.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "nezglue_x.h"
#include "device/kmsnddev.h"
#include "m_hes.h"
#include "m_gbr.h"
#include "m_zxay.h"
#include "m_nsf.h"
#include "m_kss.h"
#include "m_nsd.h"
#include "m_sgc.h"


#define GetDwordLEM(p) (Uint32)((((Uint8 *)p)[0] | (((Uint8 *)p)[1] << 8) | (((Uint8 *)p)[2] << 16) | (((Uint8 *)p)[3] << 24)))

static Uint GetWordLE(Uint8 *p)
{
	return p[0] | (p[1] << 8);
}

static Uint32 GetDwordLE(Uint8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

Uint NEZLoad(Uint8 *pData, Uint uSize)
{
	Uint ret = NESERR_NOERROR;
    nsf_state.mode = MODE_UNKNOWN;

	while (1)
	{
		NESTerminate();
		NESHandlerInitialize();
		NESAudioHandlerInitialize();
        SONGINFO_SetNSFflag(SONGINFO_OTHER);
		
		if (uSize < 8)
		{
			ret = NESERR_FORMAT;
			break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("KSCC"))
		{
			/* KSS */
			ret =  KSSLoad(pData, uSize);
			if (ret) break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("KSSX"))
		{
			/* KSS */
			ret =  KSSLoad(pData, uSize);
			if (ret) break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("HESM"))
		{
			/* HES */
			ret =  HESLoad(pData, uSize);
			if (ret) break;
		}
		else if (uSize > 0x220 && GetDwordLE(pData + 0x200) == GetDwordLEM("HESM"))
		{
			/* HES(+512byte header) */
			ret =  HESLoad(pData + 0x200, uSize - 0x200);
			if (ret) break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("NESM") && pData[4] == 0x1A)
		{
			/* NSF */
			ret = NSFLoad(pData, uSize);
			if (ret) break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("ZXAY") && GetDwordLE(pData + 4) == GetDwordLEM("EMUL"))
		{
			/* ZXAY */
			ret =  ZXAYLoad(pData, uSize);
			if (ret) break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("GBRF"))
		{
			/* GBR */
			ret =  GBRLoad(pData, uSize);
			if (ret) break;
		}
		else if ((GetDwordLE(pData + 0) & 0x00ffffff) == GetDwordLEM("GBS\x0"))
		{
			/* GBS */
			ret =  GBRLoad(pData, uSize);
			if (ret) break;
		}
		else if (pData[0] == 0xc3 && uSize > GetWordLE(pData + 1) + 4 && GetWordLE(pData + 1) > 0x70 && (GetDwordLE(pData + GetWordLE(pData + 1) - 0x70) & 0x00ffffff) == GetDwordLEM("GBS\x0"))
		{
			/* GB(GBS player) */
			ret =  GBRLoad(pData + GetWordLE(pData + 1) - 0x70, uSize - (GetWordLE(pData + 1) - 0x70));
			if (ret) break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("NESL") && pData[4] == 0x1A)
		{
			/* NSD */
			ret = NSDLoad(pData, uSize);
			if (ret) break;
		}
		else if ((GetDwordLE(pData + 0) & 0x00ffffff) == GetDwordLEM("SGC"))
		{
			/* SGC */
			ret = SGCLoad(pData, uSize);
			if (ret) break;
		}
		else
		{
			ret = NESERR_FORMAT;
			break;
		}
		return NESERR_NOERROR;
	}
	NESTerminate();
	return ret;
}
