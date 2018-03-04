#include "neserr.h"
#include "handler.h"
#include "audiosys.h"
#include "nsf6502.h"
#include "songinfo.h"

#include "device/nes/s_apu.h"

/* ------------ */
/*  km6502 I/F  */
/* ------------ */

#define USE_DIRECT_ZEROPAGE 0
#define USE_CALLBACK	1
#define USE_INLINEMMC	12
#define USE_USERPOINTER	0
#define External __inline static

#include "m_nsf.h"
#include "km6502/km2a03m.h"

extern NSFNSF *nsfnsf;

static void NES6502BreakPoint(Uint A)
{
	nsfnsf->work6502_BP = A;
}

void NES6502Irq()
{
	nsfnsf->work6502.iRequest |= K6502_INT;
}

/*
//DPCM��IRQ�\��p�B�T���v�����O���[�g�̉e�����󂯂��ɁA
//�����Ȃ�DPCM-IRQ�����邽�߂ɍ쐬�B 0�Ŋ��荞�ݖ���
void NES6502SetIrqCount(NEZ_PLAY *pNezPlay, Int A)
{
	((NSFNSF*)pNezPlay->nsf)->dpcmirq_ct = A;
}
*/

#if 0
static void NES6502Nmi()
{
	nsfnsf->work6502.iRequest |= K6502_NMI;
}
#endif
Uint NES6502Read(Uint A)
{
	return nsfnsf->work6502.ReadByte[A >> USE_INLINEMMC](A);
}

Uint NES6502ReadDma(Uint A)
{
	nsfnsf->work6502.clock++;	/* DMA cycle */
	if(nsfnsf->dpcmirq_ct >= 0){
		nsfnsf->dpcmirq_ct--;
	}
	return nsfnsf->work6502.ReadByte[A >> USE_INLINEMMC](A);
}

void NES6502Write(Uint A, Uint V)
{
	nsfnsf->work6502.WriteByte[A >> USE_INLINEMMC](A, V);
}

Uint NES6502GetCycles()
{
	return nsfnsf->work6502.clock + nsfnsf->work6502_start_cycles;
}

static Uint NES6502Execute(Uint start_cycles, Uint total_cycles)
{
	NSFNSF *nsf = nsfnsf;
	//Int32 clb = nsf->work6502.clock;
	nsf->work6502_start_cycles = start_cycles;

	while (nsf->work6502.clock < total_cycles)
	{
		//clb = nsf->work6502.clock;
		K6502_Exec(&nsf->work6502);
		/*
		//DPCM�p IRQ�J�E���^
		if(nsf->dpcmirq_ct>-65536){
			clb = nsf->work6502.clock - clb;
			nsf->dpcmirq_ct -= clb;
			clb = nsf->work6502.clock;
			if (nsf->dpcmirq_ct <= 0){
				nsf->dpcmirq_ct = -65536;
				nsf->work6502.iRequest |= K6502_INT;
				((APUSOUND*)nsf->apu)->dpcm.irq_report = 0x80;//���������C���M�����[�����A�������邵���Ȃ� 
				((APUSOUND*)nsf->apu)->dpcm.key = 0; 
				((APUSOUND*)nsf->apu)->dpcm.length = 0; 
			}
		}
		if (nsf->work6502.PC == nsf->work6502_BP)
		{
			if(nsf->dpcmirq_ct<=-65536 && !(nsf->work6502.iRequest & K6502_INT)){
				//nsf->work6502.clock = 0;
				//return 1;
			}
		}
		*/
		if (nsf->work6502.PC == nsf->work6502_BP)
		{
			nsf->work6502.clock = 0;
			return 1;
		}
	}
	nsf->work6502.clock -= total_cycles;
	/*
	if (nsf->work6502.PC == nsf->work6502_BP)
	{
		return 1;
	}
	*/
	return 0;
}

/* ----------------------- */
/*  Memory Access Handler  */
/* ----------------------- */

static void NES6502ReadHandlerSet(Uint bank, READHANDLER rh)
{
	nsfnsf->work6502.ReadByte[bank] = rh;
}

static void NES6502WriteHandlerSet(Uint bank, WRITEHANDLER wh)
{
	nsfnsf->work6502.WriteByte[bank] = wh;
}

#define EXTREADWRITE(p) \
static Uint __fastcall ExtRd##p ( Uint A) \
{ \
	NES_READ_HANDLER *ph = nsfnsf->nprh[0x##p ]; \
	do \
	{ \
		if (ph->min <= A && A <= ph->max) \
		{ \
			return ph->Proc(A); \
		} \
	} while ((ph = ph->next) != 0); \
	return 0; \
} \
static void __fastcall ExtWr##p (Uint A, Uint V) \
{ \
	NES_WRITE_HANDLER *ph = nsfnsf->npwh[0x##p ]; \
	do \
	{ \
		if (ph->min <= A && A <= ph->max) \
		{ \
			ph->Proc(A, V); \
			return; \
		} \
	} while ((ph = ph->next) != 0); \
}
EXTREADWRITE(0)
EXTREADWRITE(1)
EXTREADWRITE(2)
EXTREADWRITE(3)
EXTREADWRITE(4)
EXTREADWRITE(5)
EXTREADWRITE(6)
EXTREADWRITE(7)
EXTREADWRITE(8)
EXTREADWRITE(9)
EXTREADWRITE(A)
EXTREADWRITE(B)
EXTREADWRITE(C)
EXTREADWRITE(D)
EXTREADWRITE(E)
EXTREADWRITE(F)
static const READHANDLER ExtRdTbl[0x10] = {
	ExtRd0,ExtRd1,ExtRd2,ExtRd3,
	ExtRd4,ExtRd5,ExtRd6,ExtRd7,
	ExtRd8,ExtRd9,ExtRdA,ExtRdB,
	ExtRdC,ExtRdD,ExtRdE,ExtRdF,
};
static const WRITEHANDLER ExtWrTbl[0x10] = {
	ExtWr0,ExtWr1,ExtWr2,ExtWr3,
	ExtWr4,ExtWr5,ExtWr6,ExtWr7,
	ExtWr8,ExtWr9,ExtWrA,ExtWrB,
	ExtWrC,ExtWrD,ExtWrE,ExtWrF,
};
static Uint __fastcall NullRead(Uint A)
{
	return 0;
}
static void __fastcall NullWrite(Uint A, Uint V)
{
}


//�������烁�����[�r���A�[�ݒ�
Uint32 (*memview_memread)(Uint32 a);
//NEZ_PLAY* memview_context;
int MEM_MAX,MEM_IO,MEM_RAM,MEM_ROM;
Uint32 memview_memread_nes(Uint32 a){
	if(nsfnsf->nprh[(a>>12) & 0xF]!=NULL)
		return ExtRdTbl[(a>>12) & 0xF](a);
	else return 0xFF;
}
//�����܂Ń������[�r���A�[�ݒ�



static void NES6502Reset()
{
	NSFNSF *nsf = nsfnsf;
	nsf->work6502.clock = 0;
	nsf->work6502.iRequest = K6502_INIT;
	nsf->work6502.PC = nsf->work6502_BP = 0xFFFF;
	NES6502Execute(0, nsf->work6502.clock + 1);

	//�������烁�����[�r���A�[�ݒ�
//	memview_context = pNezPlay;
	MEM_MAX=0xffff;
	MEM_IO =0x4000;
	MEM_RAM=0x0000;
	MEM_ROM=0x8000;
	memview_memread = memview_memread_nes;
	//�����܂Ń������[�r���A�[�ݒ�

}


static void InstallPageReadHandler(NES_READ_HANDLER *ph)
{
	NSFNSF *nsf = nsfnsf;
	Uint page = (ph->min >> 12) & 0xF;
	if (nsf->nprh[page])
		NES6502ReadHandlerSet(page, ExtRdTbl[page]);
	else
		NES6502ReadHandlerSet(page, ph->Proc);
	/* Add to head of list*/
	ph->next = nsf->nprh[page];
	nsf->nprh[page] = ph;
}
static void InstallPageWriteHandler(NES_WRITE_HANDLER *ph)
{
	NSFNSF *nsf = nsfnsf;
	Uint page = (ph->min >> 12) & 0xF;
	if (nsf->npwh[page])
		NES6502WriteHandlerSet(page, ExtWrTbl[page]);
	else
		NES6502WriteHandlerSet(page, ph->Proc);
	/* Add to head of list*/
	ph->next = nsf->npwh[page];
	nsf->npwh[page] = ph;
}
void NESReadHandlerInstall(NES_READ_HANDLER *ph)
{
	for (; ph->Proc; ph++) InstallPageReadHandler(ph);
}

void NESWriteHandlerInstall(NES_WRITE_HANDLER *ph)
{
	for (; ph->Proc; ph++) InstallPageWriteHandler(ph);
}

void NESMemoryHandlerInitialize()
{
	NSFNSF *nsf = nsfnsf;
	Uint i;
	for (i = 0; i < 0x10;  i++)
	{
		NES6502ReadHandlerSet(i, NullRead);
		NES6502WriteHandlerSet(i, NullWrite);
		nsf->nprh[i] = 0;
		nsf->npwh[i] = 0;
	}
}

/* ------------- */
/*  nsf6502 I/F  */
/* ------------- */

#define SHIFT_CPS 24
#define NES_BASECYCLES (21477270)

static void NSFRomInit(Uint A)
{
	NSFNSF *nsf = nsfnsf;
	nsf->nsf6502.rom[0] = 0x20;	/* jsr */
	nsf->nsf6502.rom[1] = (A & 0xff);		/* init */
	nsf->nsf6502.rom[2] = ((A >> 8) & 0xff);
	nsf->nsf6502.rom[3] = 0x4C;	/* JMP nnnn */
	nsf->nsf6502.rom[4] = 0x03;
	nsf->nsf6502.rom[5] = 0x41;
}

static Uint GetWordLE(Uint8 *p)
{
	return p[0] | (p[1] << 8);
}

static void NSF6502PlaySetup()
{
	NSFNSF *nsf = nsfnsf;
	if (nsf->nsf6502.breaked)
	{
		nsf->nsf6502.breaked = 0;
		NES6502BreakPoint(0x4103);
		NSFRomInit(SONGINFO_GetPlayAddress());	/* PLAY */
		nsf->work6502.PC = 0x4100;
		nsf->work6502.A = 0x00;
		nsf->work6502.S = 0xFF;
		nsf->work6502.P = 0x26							/* IRZ */;
	}
}

static Int32 __fastcall Execute6502(void)
{
	NSFNSF *nsf = nsfnsf;
	Uint32 cycles;
	nsf->nsf6502.cleft += nsf->nsf6502.cps;
	cycles = nsf->nsf6502.cleft >> SHIFT_CPS;
	if (/*nsf->dpcmirq_ct>-65536 || */!nsf->nsf6502.breaked)
	{
		nsf->nsf6502.breaked = NES6502Execute(nsf->nsf6502.total_cycles, cycles);
	}else
	if (nsf->work6502.iRequest & IRQ_INT || nsf->work6502.PC != 0x4103)
	{
		NES6502Execute(nsf->nsf6502.total_cycles, cycles);
	}
	nsf->nsf6502.cleft &= (1 << SHIFT_CPS) - 1;
	nsf->nsf6502.cycles += cycles * 12;
	if (nsf->nsf6502.cycles > nsf->nsf6502.cpf[nsf->nsf6502.iframe])
	{
		nsf->vsyncirq_fg = 0x40;
		nsf->nsf6502.cycles -= nsf->nsf6502.cpf[nsf->nsf6502.iframe];
		if (nsf->nsf6502.breaked)
		{
			nsf->nsf6502.iframe ^= 1;
			NSF6502PlaySetup();
		}
	}
	/*
	if (nsf->nsf6502.cycles >= nsf->nsf6502.cpf[nsf->nsf6502.iframe] * 2)
	{
		nsf->nsf6502.cycles = nsf->nsf6502.cpf[nsf->nsf6502.iframe] * 2;
	}
	*/
	nsf->nsf6502.total_cycles += cycles;
	return 0;
}

const static NES_AUDIO_HANDLER nsf6502_audio_handler[] = {	// looks like the audio is processed using separate WriteMapper
	{ 0, Execute6502, },
	{ 0, 0,},
};

#ifdef _WIN32
extern int __stdcall MulDiv(int nNumber,int nNumerator,int nDenominator);
static Uint32 __forceinline muldiv(Uint32 m, Uint32 n, Uint32 d)
{
	return MulDiv(m,n,d);
}
#else
static Uint32 muldiv(Uint32 m, Uint32 n, Uint32 d)
{
	return ((double)m) * n / d;
}
#endif

static Uint32 DivFix(Uint32 p1, Uint32 p2, Uint32 fix)
{
	Uint32 ret;
	ret = p1 / p2;
	p1  = p1 % p2;/* p1 = p1 - p2 * ret; */
	while (fix--)
	{
		p1 += p1;
		ret += ret;
		if (p1 >= p2)
		{
			p1 -= p2;
			ret++;
		}
	}
	return ret;
}

static void __fastcall NSF6502Reset(void)
{
	NSFNSF *nsf = nsfnsf;
	Uint8 *nsfhead = NSFGetHeader();
	Uint freq = NESAudioFrequencyGet();
	Uint speed = 0;

	nsf->nsf6502.palntsc = nsfhead[0x7a] & 1;
	if (!nsf->nsf6502.palntsc && GetWordLE(nsfhead + 0x6E))
		speed = GetWordLE(nsfhead + 0x6E);	/* NTSC tune */
	else if (nsf->nsf6502.palntsc && GetWordLE(nsfhead + 0x78))
		speed = GetWordLE(nsfhead + 0x78);	/* PAL  tune */

	if (speed == 0)
		speed = nsf->nsf6502.palntsc ? 0x4e20 : 0x411A;

	nsf->nsf6502.cleft = 0;
	nsf->nsf6502.cps = DivFix(NES_BASECYCLES, 12 * freq, SHIFT_CPS);

	nsf->nsf6502.cycles = 0;
	if (nsf->nsf6502.palntsc)
	{
		nsf->nsf6502.cpf[0] = muldiv(speed, 4 * 341 * 313    , 0x4e20);
		nsf->nsf6502.cpf[1] = muldiv(speed, 4 * 341 * 313 - 4, 0x4e20);
	}
	else
	{
		nsf->nsf6502.cpf[0] = muldiv(speed, 4 * 341 * 262    , 0x411a);
		nsf->nsf6502.cpf[1] = muldiv(speed, 4 * 341 * 262 - 4, 0x411a);
	}
	nsf->nsf6502.iframe = 0;

	NES6502Reset();
	NES6502BreakPoint( 0x4103);
	NSFRomInit(SONGINFO_GetInitAddress());
	nsf->work6502.PC = 0x4100;
	nsf->work6502.A = SONGINFO_GetSongNo() - 1;
	nsf->work6502.X = nsf->nsf6502.palntsc;
	nsf->work6502.Y = 0;
	nsf->work6502.S = 0xFF;
	nsf->work6502.P = 0x26;							/* IRZ */
	nsf->nsf6502.total_cycles = 0;
	//nsf->dpcmirq_ct = -65536;
//	nsf->work6502.user = pNezPlay;
	nsf->vsyncirq_fg = 0x40;

#define LIMIT_INIT (2 * 60)	/* 2sec */
#if LIMIT_INIT
	{
		Uint sec;
		for (sec = 0; sec < LIMIT_INIT; sec++)
		{
			nsf->nsf6502.breaked = NES6502Execute(nsf->nsf6502.total_cycles, nsf->nsf6502.cpf[0]);
			if (nsf->nsf6502.breaked) break;
		}
	}
#else
	while (1)
	{
		nsf->nsf6502.breaked = NES6502Execute(nsf->nsf6502.total_cycles, ~0);
		if (nsf->nsf6502.breaked) break;
	}
#endif

	NSF6502PlaySetup();
}

const static NES_RESET_HANDLER nsf6502_reset_handler[] = {
	{ NES_RESET_SYS_LAST, NSF6502Reset, },
	{ 0,                  0, },
};


/* Nosefart-ROM area */
static Uint32 __fastcall ReadNosefartRom( Uint32 A)
{
	return nsfnsf->nsf6502.rom[A & 0x000F];
}


static NES_READ_HANDLER nsf6502_read_handler[] = {
	{ 0x4100,0x410F,ReadNosefartRom, },
	{ 0     ,0     ,0, },
};

Uint NSF6502Install()
{
	NESReadHandlerInstall(nsf6502_read_handler);
	NESAudioHandlerInstall(nsf6502_audio_handler);
	NESResetHandlerInstall(nsf6502_reset_handler);
	return NESERR_NOERROR;
}

