#include "nestypes.h"
#include "kmsnddev.h"
#include "format/audiosys.h"
#include "format/handler.h"
#include "format/nsf6502.h"
#include "logtable.h"
#include "m_nsf.h"
#include "s_apu.h"
#include <time.h>
#include <math.h>

#define NES_BASECYCLES (21477270)

/* 31 - log2(NES_BASECYCLES/(12*MIN_FREQ)) > CPS_BITS  */
/* MIN_FREQ:11025 23.6 > CPS_BITS */
/* 32-12(max spd) > CPS_BITS */

#define CPS_BITS 11

#define SQUARE_RENDERS 6
#define TRIANGLE_RENDERS 6
#define NOISE_RENDERS 4
#define DPCM_RENDERS 4

#define SQ_VOL_BIT 10
#define TR_VOL ((1<<9)*4)
#define NOISE_VOL ((1<<8)*4)
#define DPCM_VOL ((1<<6)*4)
#define DPCM_VOL_DOWN 455


#define AMPTML_BITS 14
#define AMPTML_MAX (1 << (AMPTML_BITS))

#if 1
#define VOL_SHIFT 0 /* 後期型 */
#else
#define VOL_SHIFT 1 /* 初期型 */
#endif

typedef struct {
	Uint32 counter;				/* length counter */
	Uint8 clock_disable;	/* length counter clock disable */
} LENGTHCOUNTER;

typedef struct {
	Uint8 disable;			/* envelope decay disable */
	Uint8 counter;			/* envelope decay counter */
	Uint8 rate;				/* envelope decay rate */
	Uint8 timer;			/* envelope decay timer */
	Uint8 looping_enable;	/* envelope decay looping enable */
	Uint8 volume;			/* volume */
} ENVELOPEDECAY;
typedef struct {
	Uint8 ch;				/* sweep channel */
	Uint8 active;			/* sweep active */
	Uint8 rate;				/* sweep rate */
	Uint8 timer;			/* sweep timer */
	Uint8 direction;		/* sweep direction */
	Uint8 shifter;			/* sweep shifter */
} SWEEP;

typedef struct {
	LENGTHCOUNTER lc;
	ENVELOPEDECAY ed;
	SWEEP sw;
	Uint32 mastervolume;
	Uint32 cps;		/* cycles per sample */
	Uint32 *cpf;	/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint32 fc;		/* frame counter; */
	Uint32 wl;		/* wave length */
	Uint32 pt;		/* programmable timer */
	Uint32 st;		/* wave step */
	Uint32 duty;		/* duty rate */
	Uint32 output;
	Uint8 fp;		/* frame position */
	Uint8 key;
	Uint8 mute;
	Uint32 ct;
} NESAPU_SQUARE;

typedef struct {
	Uint32 *cpf;			/* cycles per frame (240Hz fix) */
	Uint32 fc;			/* frame counter; */
	Uint8 load;			/* length counter load register */
	Uint8 start;		/* length counter start */
	Uint8 counter;		/* length counter */
	Uint8 startb;		/* length counter start */
	Uint8 counterb;		/* length counter */
	Uint8 tocount;		/* length counter go to count mode */
	Uint8 mode;			/* length counter mode load(0) count(1) */
	Uint8 clock_disable;	/* length counter clock disable */
} LINEARCOUNTER;

typedef struct {
	LENGTHCOUNTER lc;
	LINEARCOUNTER li;
	Uint32 mastervolume;
	Uint32 cps;		/* cycles per sample */
	Uint32 *cpf;	/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint32 cpb180;	/* cycles per base/180 */
	Uint32 fc;		/* frame counter; */
	Uint32 b180c;	/* base/180 counter; */
	Uint32 wl;		/* wave length */
	Uint32 wlb;		/* wave length base*/
	Uint32 pt;		/* programmable timer */
	Uint32 st;		/* wave step */
	Uint32 output;
	Uint8 fp;		/* frame position; */
	Uint8 key;
	Uint8 mute;
	Int32 ct;
	Uint8 ct2;
	Uint32 dpcmout;
} NESAPU_TRIANGLE;
typedef struct {
	LENGTHCOUNTER lc;
	LINEARCOUNTER li;
	ENVELOPEDECAY ed;
	Uint32 mastervolume;
	Uint32 cps;		/* cycles per sample */
	Uint32 *cpf;	/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint32 fc;		/* frame counter; */
	Uint32 wl;		/* wave length */
	Uint32 pt;		/* programmable timer */
	Uint32 rng;
	Uint32 rngcount;
	Uint32 output;
	Uint8 rngshort;
	Uint8 fp;		/* frame position; */
	Uint8 key;
	Uint8 mute;
	Uint8 ct;
	Uint32 dpcmout;
} NESAPU_NOISE;

typedef struct {
	Uint32 cps;		/* cycles per sample */
	Uint32 wl;		/* wave length */
	Uint32 pt;		/* programmable timer */
	Uint32 length;	/* bit length */
	Uint32 mastervolume;
	Uint32 adr;		/* current address */
	Int32 dacout;
	Int32 dacout0;

	Uint8 start_length;
	Uint8 start_adr;
	Uint8 loop_enable;
	Uint8 irq_enable;
	Uint8 irq_report;
	Uint8 input;	/* 8bit input buffer */
	Uint8 first;
	Uint8 dacbase;
	Uint8 key;
	Uint8 mute;

	Uint32 output;
	Uint8 ct;
	Uint32 dactbl[128];

} NESAPU_DPCM;

typedef struct {
	NESAPU_SQUARE square[2];
	NESAPU_TRIANGLE triangle;
	NESAPU_NOISE noise;
	NESAPU_DPCM dpcm;
	Uint32 cpf[4];	/* cycles per frame (240/192Hz) ($4017.bit7) */
	Uint8 regs[0x20];
	//Int32 amptbl[1 << AMPTML_BITS];
} APUSOUND;

static APUSOUND apu;

Int32 NSF_noise_random_reset = 0;
Int32 NESAPUVolume = 64;
Int32 NESRealDAC = 1;
Int32 NSF_2A03Type = 1;
/* ------------------------- */
/*  NES INTERNAL SOUND(APU)  */
/* ------------------------- */

/* GBSOUND.TXT */
const static Uint8 square_duty_table[][8] = 
{
	{0,0,0,1,0,0,0,0} , {0,0,0,1,1,0,0,0} , {0,0,0,1,1,1,1,0} , {1,1,1,0,0,1,1,1} ,
	{1,0,0,0,0,0,0,0} , {1,1,1,1,0,0,0,0} , {1,1,0,0,0,0,0,0} , {1,1,1,1,1,1,0,0}  //ƒNƒ\ŒÝŠ·‹@‚ÌDuty”ä‚Ì‚Ð‚Á‚­‚è•Ô‚Á‚Ä‚é‚â‚Â 
};
//	{ {0,1,0,0,0,0,0,0} , {0,1,1,0,0,0,0,0} , {0,1,1,1,1,0,0,0} , {0,1,1,1,1,1,1,0} };

const static Uint8 square_duty_avg[4] = {2,4,8,12};

static const Uint8 vbl_length_table[96] = {
	0x05, 0x7f, 0x0a, 0x01, 0x14, 0x02, 0x28, 0x03,
	0x50, 0x04, 0x1e, 0x05, 0x07, 0x06, 0x0d, 0x07,
	0x06, 0x08, 0x0c, 0x09, 0x18, 0x0a, 0x30, 0x0b,
	0x60, 0x0c, 0x24, 0x0d, 0x08, 0x0e, 0x10, 0x0f,
	0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x72, 0x6f,
	0x67, 0x72, 0x61, 0x6d, 0x20, 0x75, 0x73, 0x65, 
	0x20, 0x4e, 0x45, 0x5a, 0x50, 0x6c, 0x75, 0x67,
	0x2b, 0x2b, 0x20, 0x69, 0x6e, 0x20, 0x46, 0x61, 
	0x6d, 0x69, 0x63, 0x6f, 0x6e, 0x2e, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x28, 0x6d, 0x65, 
	0x73, 0x73, 0x61, 0x67, 0x65, 0x20, 0x62, 0x79,
	0x20, 0x4f, 0x66, 0x66, 0x47, 0x61, 0x6f, 0x29

};

static const Uint32 wavelength_converter_table[16] = {
	0x002, 0x004, 0x008, 0x010, 0x020, 0x030, 0x040, 0x050,
	0x065, 0x07f, 0x0be, 0x0fe, 0x17d, 0x1fc, 0x3f9, 0x7f2
};

static const Uint32 spd_limit_table[8] =
{
	0x3FF, 0x555, 0x666, 0x71C, 
	0x787, 0x7C1, 0x7E0, 0x7F0,
};

static Uint32 dpcm_freq_table[16] =
{
	428, 380, 340, 320,
	286, 254, 226, 214,
	190, 160, 142, 128,
	106,  85,  72,  54,
};

__inline static void LengthCounterStep(LENGTHCOUNTER *lc)
{
	if (lc->counter && !lc->clock_disable) lc->counter--;
}

/*
E4008‚ÌMSB‚ª0‚Ìê‡‚É400B‚É‘‚­‚ÆAˆÈŒã‚Í4008‚É‰½‚Ì’l‚ð‘‚¢‚Ä‚à–³Ž‹‚³‚ê‚éB
E4008‚ÌMSB‚ª1‚Ìê‡‚É400B‚É‘‚­‚ÆAˆÈŒã4008‚É80‚ÅÁ‰¹E81-FF‚Å”­º‚ª‰Â”\B
@‚½‚¾‚µA00-7F‚Ì’l‚ð‘‚¢‚½ê‡A‚»‚ÌuŠÔ‚ÉŠeŽíƒJƒEƒ“ƒ^‚ª—LŒøE‚»‚ÌƒJƒEƒ“ƒg’l‚ÅƒJƒEƒ“ƒgŠJŽn‚Æ‚È‚èA
@‚»‚êˆÈŒã‚Ì4008‘‚«ž‚Ý‚ÍAƒL[ƒIƒtƒJƒEƒ“ƒ^‚Í—LŒø–³ŒøˆÈŠO–³Ž‹‚Æ‚È‚éB
E4008‚Ì’l•ÏX‚É‚æ‚éÁ‰¹E”­º‚Ìƒ^ƒCƒ~ƒ“ƒO‚ÍAŽOŠp”g‰¹’·ƒJƒEƒ“ƒgŽži‚Â‚Ü‚èA4017-MSB‚ª0‚ÌŽž‚Í240HzŽüŠújB
E400B‘‚«ž‚Ý‚É‚æ‚éƒL[ƒIƒ“‚ÍAƒJƒEƒ“ƒgŽüŠú‚ª—ˆ‚½‚ ‚Æ‚É”­º‚³‚ê‚éB
EŽü”g”‚Í‘‚¢‚ÄuŽž‚É”½‰f‚³‚ê‚éB‚»‚Ì‚½‚ßA8-10bit–Ú‚ð‚Ü‚½‚®Žü”g”•ÏX‚ÍA
@•ÏX‚Ì‡ŠÔ‚ÌŽü”g”‚Ì’l‚É‚æ‚Á‚Ä‚Íƒvƒ`ƒmƒCƒY‚ª‚Æ‚«‚Ç‚«o‚éB
*/
__inline static void LinearCounterStep(LINEARCOUNTER *li/*, Uint32 cps*/)
	{
//	li->fc += cps;
//	while (li->fc > li->cpf[0])
//	{
//		li->fc -= li->cpf[0];
		if (li->start)
		{
			//li->start = 0;
			li->counter = li->load;
			li->start = li->clock_disable = li->tocount;
		}
		else{
			if(li->counter){
				li->counter--;
		}
	}
//		if (!li->tocount && li->counter)
//		{
//			li->start = 0;
//		}


		
//	}
}

static void EnvelopeDecayStep(ENVELOPEDECAY *ed)
{
//	if (!ed->disable)
	if((ed->timer & 0x1f) == 0)
	{
		ed->timer = ed->rate;
		if (ed->counter || ed->looping_enable){
			ed->counter = (ed->counter - 1) & 0xF;
	}
	}else
		ed->timer--;
}

static void SweepStep(SWEEP *sw, Uint32 *wl)
{
	if (sw->active && sw->shifter && --sw->timer > 7)
	{
		sw->timer = sw->rate;
		if (sw->direction)
		{
			if(!sw->ch){
				if (*wl > 0)*wl += ~(*wl >> sw->shifter);
			}else{
			*wl -= (*wl >> sw->shifter);
			}
		}
		else
		{
			*wl += (*wl >> sw->shifter);
//			if (*wl < 0x7ff && !sw->ch) *wl+=1;
		}
	}
}

static void NESAPUSoundSquareCount(NESAPU_SQUARE *ch){

	ch->fc += ch->cps;
	while (ch->fc >= ch->cpf[0])
	{
		ch->fc -= ch->cpf[0];
		if (!(ch->fp & 4)){
			if (!(ch->fp & 1)) LengthCounterStep(&ch->lc);	/* 120Hz */
			if (!(ch->fp & 1)) SweepStep(&ch->sw, &ch->wl);	/* 120Hz */
			EnvelopeDecayStep(&ch->ed);	/* 240Hz */
		}
		ch->fp++;if(ch->fp >= 4+ch->cpf[2])ch->fp=0;
	}

}


static Int32 NESAPUSoundSquareRender(NESAPU_SQUARE *ch)
{
	Int32 outputbuf=0,count=0;
	NESAPUSoundSquareCount(ch);
	if (!ch->key || !ch->lc.counter)
	{
		return 0;
	}
	else
	{
		if (!ch->sw.direction && ch->wl > spd_limit_table[ch->sw.shifter])
		{
#if 1
			return 0;
#endif
		}
		else if (ch->wl <= 7 || 0x7ff < ch->wl)
		{
#if 1
			return 0;
#endif
		}
		else
		{
			ch->pt += ch->cps << SQUARE_RENDERS;

			ch->output = (square_duty_table[ch->duty][ch->st]) 
				? (ch->ed.disable ? ch->ed.volume : ch->ed.counter)<<1 : 0;
			ch->output <<= SQ_VOL_BIT; 
/*
			if (ch->wl <= 7){
				ch->st+=ch->pt / (((ch->wl + 1) << CPS_BITS)>>(SQUARE_RENDERS+1));
				ch->pt%=((ch->wl + 1) << CPS_BITS);
				ch->st&= 0x7;
				//wl‚ª000H‚¾‚Æ‰¹‚Ío—Í‚³‚ê‚È‚¢H
				ch->output = ch->wl != 0 ? ((ch->ed.disable ? ch->ed.volume : ch->ed.counter) * square_duty_avg[ch->duty]) : 0;
				ch->output = LinearToLog(ch->output) + ch->mastervolume;
				ch->output = -LogToLinear(ch->output, LOG_LIN_BITS - LIN_BITS - 9 + VOL_SHIFT) * SQ_VOL;
			}
			else
*/				while (ch->pt >= ((ch->wl + 1) << CPS_BITS))
				{
					outputbuf += ch->output;
					count++;

					ch->pt -= ((ch->wl + 1) << CPS_BITS);

					ch->ct++;
					if(ch->ct >= (1<<(SQUARE_RENDERS+1))){
						ch->ct = 0;
						ch->st = (ch->st + 1) & 0x7;

						ch->output = (square_duty_table[ch->duty][ch->st]) 
							? (ch->ed.disable ? ch->ed.volume : ch->ed.counter)<<1 : 0;
						ch->output <<= SQ_VOL_BIT; 
					}
			}
		}
	}
	if (ch->mute) return 0;

	outputbuf += ch->output;
	count++;
	return outputbuf /count;
}

static void NESAPUSoundTriangleCount(NESAPU_TRIANGLE *ch)
{
	/*
	ch->b180c += ch->cps;
	if(ch->b180c >= ch->cpb180){
		ch->wl = ch->wlb;
		if (ch->li.startb == 0xc0){
			ch->li.counter = ch->li.counterb;
			ch->li.start = ch->li.startb = 0x80;
		}
		ch->b180c %= ch->cpb180;
	}
	*/
	ch->fc += ch->cps;
	//if (!(ch->fp & 4)){
	//}
	while (ch->fc >= ch->cpf[0])
	{
		ch->fc -= ch->cpf[0];
		if (!(ch->fp & 4)){
			if (!(ch->fp & 1)) LengthCounterStep(&ch->lc);	/* 120Hz */
			LinearCounterStep(&ch->li);
		}
		ch->fp++;if(ch->fp >= 4+ch->cpf[2])ch->fp=0;
	}
}
static Int32 NESAPUSoundTriangleRender(NESAPU_TRIANGLE *ch)
{
	Int32 outputbuf=0,count=0;
	//Int32 output;
	NESAPUSoundTriangleCount(ch);

	ch->output = (ch->st & 0x0f);
	if (ch->st & 0x10) ch->output = ch->output ^ 0xf ;
	ch->output *= TR_VOL;

	if (ch->key && (ch->li.clock_disable ? ch->li.load : ch->li.counter ) &&  ch->lc.counter) {
		/* ŒÃ‚¢ƒ^ƒCƒv 
		ch->pt += ch->cps << TRIANGLE_RENDERS;
		if (ch->wl <= 4){
			ch->st += ch->pt / (((ch->wl + 1) << CPS_BITS)>>TRIANGLE_RENDERS);
			ch->pt %= ((ch->wl + 1) << CPS_BITS);
			ch->output = 8;
			ch->output *= TR_VOL;
		}else{
			while (ch->pt >= ((ch->wl + 1) << CPS_BITS))
			{
				outputbuf += ch->output;
				count++;

				ch->pt -= ((ch->wl + 1) << CPS_BITS);
				ch->ct++;
				if(ch->ct >= (1<<TRIANGLE_RENDERS)){
					ch->ct = 0;
					ch->st++;

					ch->output = (ch->st & 0x0f);
					if (ch->st & 0x10) ch->output = ch->output ^ 0xf ;
					ch->output *= TR_VOL;
				}
			}
		}
		*/
		//V‚µ‚¢ƒ^ƒCƒvBƒŒƒ“ƒ_[–ˆ‚Éˆê’è‚ÌŒ¸ŽZ‚ðs‚¢AƒAƒ“ƒ_[ƒtƒ[Žž‚ÉŽü”g”ƒŒƒWƒXƒ^’l‚Å
		//ƒJƒEƒ“ƒgƒŠƒZƒbƒg‚·‚é•ûŽ®B
		//9bit–Ú‚ð‚Ü‚½‚®‚Æ‚«‚Ìƒvƒ`ƒmƒCƒY‚Ìæ‚è•û“I‚ÉA‚½‚Ô‚ñŽÀ‹@‚Ì“®ì‚Í‚±‚ê‚©‚ÆB
		if(ch->wlb < 4){
			//Žü”g”ƒŒƒWƒXƒ^‚ª‹É’[‚É¬‚³‚¢‚Æ‚«B‚±‚±‚Í“K“–‚Å—Ç‚¢‚©‚ŸB
			/*//‚±‚ê‚¿‚Æd‚¢
			ch->pt += ch->cps << TRIANGLE_RENDERS;

			ch->ct -= ch->pt / (1<<CPS_BITS);
			ch->pt %= (1<<CPS_BITS);

			while (ch->ct < 0)//ƒJƒEƒ“ƒ^‚ªƒAƒ“ƒ_[ƒtƒ[‚µ‚½ê‡
			{
				ch->ct += ch->wlb;
				ch->ct2++;

				if(ch->ct2 >= (1 << TRIANGLE_RENDERS)){
					ch->wlb = ch->wl +1;
					ch->ct += ch->wlb;
					ch->ct2 = 0;
					ch->st++;
				}
			}*/
			ch->wlb = ch->wl +1;

			ch->output = 8;
			ch->output *= TR_VOL;
		}else{
			ch->pt += ch->cps << TRIANGLE_RENDERS;

			ch->ct -= ch->pt / (1<<CPS_BITS);
			ch->pt %= (1<<CPS_BITS);

			while (ch->ct < 0)//ƒJƒEƒ“ƒ^‚ªƒAƒ“ƒ_[ƒtƒ[‚µ‚½ê‡
			{
				ch->ct += ch->wlb;
				ch->ct2++;

				outputbuf += ch->output;
				count++;

				if(ch->ct2 >= (1 << TRIANGLE_RENDERS)){
					ch->wlb = ch->wl +1;
					ch->ct2 = 0;
					ch->st++;

					ch->output = (ch->st & 0x0f);
					if (ch->st & 0x10) ch->output = ch->output ^ 0xf ;
					ch->output *= TR_VOL;
				}
			}
		}
	}
	if (ch->mute) return 0;
	outputbuf += ch->output;
	count++;

	outputbuf = outputbuf / count;
	return outputbuf * ch->dpcmout / DPCM_VOL_DOWN;

//	return LogToLinear(output, LOG_LIN_BITS - LIN_BITS - 18 + VOL_SHIFT) * ((0x80 - ch->dpcmvol)/128.0) * 1.25;
}

static void NESAPUSoundNoiseCount(NESAPU_NOISE *ch){

	ch->fc += ch->cps;
	while (ch->fc >= ch->cpf[0])
	{
		ch->fc -= ch->cpf[0];
		if (!(ch->fp & 4)){
			if (!(ch->fp & 1)) LengthCounterStep(&ch->lc);	/* 120Hz */
			EnvelopeDecayStep(&ch->ed);						/* 240Hz */
		}
		ch->fp++;if(ch->fp >= 4+ch->cpf[2])ch->fp=0;
	}

}
static Int32 NESAPUSoundNoiseRender(NESAPU_NOISE *ch)
{
	Int32 outputbuf=0,count=0;

	NESAPUSoundNoiseCount(ch);
	if (!ch->key || !ch->lc.counter){
		return 0;
	}
	if (!ch->wl) return 0;

	ch->output = (ch->rng & 1) * (ch->ed.disable ? ch->ed.volume : ch->ed.counter);
	ch->output *= NOISE_VOL;
	//ƒNƒ\ŒÝŠ·‹@‚ÍA‚â‚½‚ç‚ÆƒmƒCƒY‚ª‚Å‚©‚¢B
	if(NSF_2A03Type==2)ch->output *= 2;

	ch->pt += ch->cps << NOISE_RENDERS;
	while (ch->pt >= ch->wl << (CPS_BITS + 1))
	{
		outputbuf += ch->output;
		count++;

		ch->pt -= ch->wl << (CPS_BITS + 1);

		/* ‰¹Ž¿Œüã‚Ì‚½‚ß */
		ch->rngcount++;
		if( ch->rngcount >= (1<<NOISE_RENDERS)){
			ch->rngcount = 0;
			ch->rng >>= 1;
			ch->rng |= ((ch->rng ^ (ch->rng >> (ch->rngshort ? 6 : 1))) & 1) << 15;

			ch->output = (ch->rng & 1) * (ch->ed.disable ? ch->ed.volume : ch->ed.counter);
			ch->output *= NOISE_VOL;
			//ƒNƒ\ŒÝŠ·‹@‚ÍA‚â‚½‚ç‚ÆƒmƒCƒY‚ª‚Å‚©‚¢B
			if(NSF_2A03Type==2)ch->output *= 2;
		}
	}
	outputbuf += ch->output;
	count++;
	if (ch->mute) return 0;

	outputbuf = outputbuf / count;
	return outputbuf * ch->dpcmout / DPCM_VOL_DOWN;
}

static void NESAPUSoundDpcmRead(NESAPU_DPCM *ch)
{
	ch->input = (Uint8)NES6502ReadDma(ch->adr);
	if(++ch->adr > 0xffff)ch->adr = 0x8000;
	
}

static void NESAPUSoundDpcmStart(NESAPU_DPCM *ch)
{
	ch->adr = 0xC000 + ((Uint)ch->start_adr << 6);
	ch->length = (((Uint)ch->start_length << 4) + 1) << 3;
	ch->irq_report = 0;
/*
	if (ch->irq_enable && !ch->loop_enable){
		//Š„‚èž‚Ý‚ª‚©‚©‚éðŒ‚Ìê‡
		NES6502SetIrqCount( ch->length * ch->wl);
	}
*/
	NESAPUSoundDpcmRead(ch);
}

static Int32 __fastcall NESAPUSoundDpcmRender(void)
{
#define ch (&apu.dpcm)
#define DPCM_OUT ch->dactbl[((ch->dacout<<1) + ch->dacout0)] * DPCM_VOL;

	Int32 outputbuf=0,count=0;

	ch->output = DPCM_OUT;
	if (ch->first)
	{
		ch->first = 0;
		ch->dacbase = (Uint8)ch->dacout;
	}
	if (ch->key && ch->length)
	{

		ch->pt += ch->cps << DPCM_RENDERS;
		while (ch->pt >= ((ch->wl + 0) << CPS_BITS))
		{
			outputbuf += ch->output;
			count++;

			ch->pt -= ((ch->wl + 0) << CPS_BITS);
			ch->ct++;
			if(ch->ct >= (1<<DPCM_RENDERS)){
				ch->ct = 0;
				if (ch->length == 0) continue;
				if (ch->input & 1)
					ch->dacout += (ch->dacout < +0x3f);
				else
					ch->dacout -= (ch->dacout > 0);
				ch->input >>= 1;

				if (--ch->length == 0)
				{
					if (ch->loop_enable)
					{
						NESAPUSoundDpcmStart(ch);	/*loop */
					}
					else
					{
						if (ch->irq_enable)
						{
							NES6502Irq();	// irq gen
							ch->irq_report = 0x80;
						}
						
						ch->length = 0;
						ch->key = 0;
					}
				}
				else if ((ch->length & 7) == 0)
				{
					NESAPUSoundDpcmRead(ch);
				}
				ch->output = DPCM_OUT;
			}
		}
	}
	if (ch->mute) return 0;
#if 1
	outputbuf += ch->output;
	count++;
	outputbuf /= count;
	if (NESRealDAC) {
		apu.triangle.dpcmout = 
		apu.noise.dpcmout = 
			DPCM_VOL_DOWN - (outputbuf / DPCM_VOL);
	}else{
		apu.triangle.dpcmout = 
		apu.noise.dpcmout = 
			DPCM_VOL_DOWN;
	}
	return	outputbuf;
#else
	return (LogToLinear(LinearToLog((ch->dacout << 1) + ch->dacout0) + ch->mastervolume, LOG_LIN_BITS - LIN_BITS - 16 + VOL_SHIFT + 1)
		  - LogToLinear(LinearToLog( ch->dacbase                   ) + ch->mastervolume, LOG_LIN_BITS - LIN_BITS - 16 + VOL_SHIFT + 1)
		  ) * NESAPUVolume / 64;
#endif
#undef ch
}

static Int32 __fastcall APUSoundRender(void)
{
	Int32 accum = 0 , sqout = 0 , tndout = 0;
	sqout += NESAPUSoundSquareRender(&apu.square[0]) * chmask[DEV_2A03_SQ1];
	sqout += NESAPUSoundSquareRender(&apu.square[1]) * chmask[DEV_2A03_SQ2];
//DAC‚ÌŽd—l‚ª‚æ‚­•ª‚©‚é‚Ü‚Å‚Í–³Œø‚É‚µ‚Ä‚¨‚­
//	if (NESRealDAC) {
//		sqout = apu.amptbl[sqout >> (16 + 1 + 1 - AMPTML_BITS)];
//	} else {
		sqout >>= 1;
//	}
	accum += sqout * apu.square[0].mastervolume / 20/*20kƒ¶*/;
	tndout += NESAPUSoundDpcmRender() * chmask[DEV_2A03_DPCM];
	tndout += NESAPUSoundTriangleRender(&apu.triangle) * chmask[DEV_2A03_TR];
	tndout += NESAPUSoundNoiseRender(&apu.noise) * chmask[DEV_2A03_NOISE];
//	if (NESRealDAC) {
//		tndout = apu.amptbl[tndout >> (16 + 1 + 1 - AMPTML_BITS)];
//	} else {
		tndout >>= 1;
//	}
	accum += tndout * apu.triangle.mastervolume / 12/*12kƒ¶*/;
	//accum = apu.amptbl[tndout >> (26 - AMPTML_BITS)];
	accum -= 0x60000;
	return accum * NESAPUVolume / 8;
}

static NES_AUDIO_HANDLER s_apu_audio_handler[] = {
	{ 1, APUSoundRender, }, 
	{ 0, 0, }, 
};

static void __fastcall APUSoundVolume(Uint volume)
{
	
	if(volume > 255) volume = 255;
	volume = 256 - volume;
	/* SND1 */
	apu.square[0].mastervolume = volume;
	apu.square[1].mastervolume = volume;

	/* SND2 */
	apu.triangle.mastervolume = volume;
	apu.noise.mastervolume = volume;
	apu.dpcm.mastervolume = volume;

}

static NES_VOLUME_HANDLER s_apu_volume_handler[] = {
	{ APUSoundVolume, },
	{ 0, }, 
};

static void __fastcall APUSoundWrite(Uint address, Uint value)
{
	if (0x4000 <= address && address <= 0x4017)
	{
		apu.regs[address - 0x4000] = (Uint8)value;
		switch (address)
		{
			case 0x4000:	case 0x4004:
				{
					int ch = address >= 0x4004;
					if (value & 0x10)
						apu.square[ch].ed.volume = (Uint8)(value & 0x0f);
					apu.square[ch].ed.rate   = (Uint8)(value & 0x0f);
					apu.square[ch].ed.disable = (Uint8)(value & 0x10);
					apu.square[ch].lc.clock_disable = (Uint8)(value & 0x20);
					apu.square[ch].ed.looping_enable = (Uint8)(value & 0x20);
					//ƒNƒ\ŒÝŠ·‹@‚Ì‚Ö‚ñ‚Ä‚±Duty”ä‚É‚·‚é‚Ì‚ðA‚±‚±‚Å‚â‚éB
					if(NSF_2A03Type==2){
						apu.square[ch].duty = ((value >> 6) & 3)|4;
					}else{
						apu.square[ch].duty = (value >> 6) & 3;
					}
				}
				break;
			case 0x4001:	case 0x4005:
				{
					int ch = address >= 0x4004;
					apu.square[ch].sw.shifter = (Uint8)(value & 7);
					apu.square[ch].sw.direction = (Uint8)(value & 8);
					apu.square[ch].sw.rate = (Uint8)((value >> 4) & 7);
					apu.square[ch].sw.active = (Uint8)(value & 0x80);
					apu.square[ch].sw.timer = apu.square[ch].sw.rate;
				}
				break;
			case 0x4002:	case 0x4006:
				{
					int ch = address >= 0x4004;
					apu.square[ch].wl &= 0xffffff00;
					apu.square[ch].wl += value;
				}
				break;
			case 0x4003:	case 0x4007:
				{
					int ch = address >= 0x4004;
					//apu.square[ch].pt = 0;
#if 1
					apu.square[ch].st = 0x0;
					apu.square[ch].ct = 0x30;
#endif
					apu.square[ch].wl &= 0x0ff;
					apu.square[ch].wl += (value & 7) << 8;
					apu.square[ch].lc.counter = vbl_length_table[value >> 3] * 2;
					apu.square[ch].ed.counter = 0xf;
					apu.square[ch].ed.timer = apu.square[ch].ed.rate + 1;
				}
				break;
			case 0x4008:
				apu.triangle.li.tocount = (Uint8)(value & 0x80);
				apu.triangle.li.load = (Uint8)(value & 0x7f);
				apu.triangle.lc.clock_disable = (Uint8)(value & 0x80);
					/*
				if(apu.triangle.li.clock_disable){
					apu.triangle.li.counter = apu.triangle.li.load;
					apu.triangle.li.clock_disable = (Uint8)(value & 0x80);
				}*/
				//if(apu.triangle.li.clock_disable)
				//	apu.triangle.li.mode=1;

				//apu.triangle.li.start = (Uint8)(value & 0x80);
				break;
			case 0x400a:
				apu.triangle.wl &= 0x700;
				apu.triangle.wl += value;
//				if(apu.triangle.wlb == 0){
//					apu.triangle.wl = apu.triangle.wlb;
//				}
				//apu.triangle.wl = apu.triangle.wlb;
				//apu.triangle.b180c = 0;
				break;
			case 0x400b:
				apu.triangle.wl &= 0x0ff;
				apu.triangle.wl += (value & 7) << 8;
/*				if (!apu.triangle.lc.counter
				 || !apu.triangle.li.counter
				 || !apu.triangle.li.mode)
*///				apu.triangle.wl = apu.triangle.wlb;
				apu.triangle.lc.counter = vbl_length_table[value >> 3] * 2;
				//apu.triangle.li.mode = apu.triangle.li.clock_disable;
				//apu.triangle.li.counter = apu.triangle.li.load;
                    
				//apu.triangle.li.clock_disable = apu.triangle.li.tocount;
				apu.triangle.lc.clock_disable = apu.triangle.li.tocount;

				apu.triangle.li.start = 0x80;
				//apu.triangle.wl = apu.triangle.wlb;
				//apu.triangle.b180c = 0;
				break;
			case 0x400c:
				if (value & 0x10)
					apu.noise.ed.volume = (Uint8)(value & 0x0f);
				else
				{
					apu.noise.ed.rate = (Uint8)(value & 0x0f);
				}
				apu.noise.ed.disable = (Uint8)(value & 0x10);
				apu.noise.lc.clock_disable = (Uint8)(value & 0x20);
				apu.noise.ed.looping_enable = (Uint8)(value & 0x20);
				break;
			case 0x400e:
				//‰‘ã2A03‚Å‚ÍAƒmƒCƒY15’iŠK•’ZŽüŠú–³‚µ‚È‚Ì‚ÅA‚»‚ê‚ðƒŒƒWƒXƒ^‚¢‚¶‚è‚ÅÄŒ»B
				if(NSF_2A03Type==0){
					apu.noise.wl = wavelength_converter_table[(value & 0x0f)==0xf ? (0xe) : (value & 0x0f)];
					apu.noise.rngshort = 0;
				}else{
					apu.noise.wl = wavelength_converter_table[value & 0x0f];
					apu.noise.rngshort = (Uint8)(value & 0x80);
				}
				break;
			case 0x400f:
				// apu.noise.rng = 0x8000;
				apu.noise.ed.counter = 0xf;
				apu.noise.lc.counter = vbl_length_table[value >> 3] * 2;
				apu.noise.ed.timer = apu.noise.ed.rate + 1;
				break;

			case 0x4010:
				apu.dpcm.wl = dpcm_freq_table[value & 0x0F];
				apu.dpcm.loop_enable = (Uint8)(value & 0x40);
				apu.dpcm.irq_enable = (Uint8)(value & 0x80);
				if (!apu.dpcm.irq_enable) apu.dpcm.irq_report = 0;
				break;
			case 0x4011:
#if 0
				if (apu.dpcm.first && (value & 0x7f))
				{
					apu.dpcm.first = 0;
					apu.dpcm.dacbase = value & 0x7f;
				}
#endif
				apu.dpcm.dacout = (value >> 1) & 0x3f;
				apu.dpcm.dacout0 = value & 1;
				break;
			case 0x4012:
				apu.dpcm.start_adr = (Uint8)value;
				break;
			case 0x4013:
				apu.dpcm.start_length = (Uint8)value;
				break;

			case 0x4015:
				if (value & 1)
					apu.square[0].key = 1;
				else
				{
					apu.square[0].key = 0;
					apu.square[0].lc.counter = 0;
				}
				if (value & 2)
					apu.square[1].key = 1;
				else
				{
					apu.square[1].key = 0;
					apu.square[1].lc.counter = 0;
				}
				if (value & 4)
					apu.triangle.key = 1;
				else
				{
					apu.triangle.key = 0;
					apu.triangle.lc.counter = 0;
					apu.triangle.li.counter = 0;
					apu.triangle.li.start = 0;
				}
				if (value & 8)
					apu.noise.key = 1;
				else
				{
					apu.noise.key = 0;
					apu.noise.lc.counter = 0;
				}
				if (value & 16)
				{
					if (!apu.dpcm.key)
					{
						apu.dpcm.key = 1;
						NESAPUSoundDpcmStart(&apu.dpcm);
					}
				}
				else
				{
					apu.dpcm.key = 0;
				}
				break;
			case 0x4017:
					apu.square[0].fp = 0;
					apu.square[1].fp = 0;
					apu.triangle.fp = 0;
					apu.noise.fp = 0;


					apu.square[0].fc = apu.cpf[0] - apu.square[0].cps;
					apu.square[1].fc = apu.cpf[0] - apu.square[1].cps;
					apu.triangle.fc = apu.cpf[0] - apu.triangle.cps;
					apu.noise.fc = apu.cpf[0] - apu.noise.cps;
					
					apu.triangle.b180c = apu.triangle.cpb180;

					NESAPUSoundSquareCount(&apu.square[0]);
					NESAPUSoundSquareCount(&apu.square[1]);
					NESAPUSoundTriangleCount(&apu.triangle);
					NESAPUSoundNoiseCount(&apu.noise);
				if (value & 0x80){
					//80ƒtƒ‰ƒO
					apu.cpf[2] = 1;

				}else{
					apu.cpf[2] = 0;

				}
				break;
		}
	}
}

static NES_WRITE_HANDLER s_apu_write_handler[] =
{
	{ 0x4000, 0x4017, APUSoundWrite, },
	{ 0,      0,      0, },
};

extern NSFNSF *nsfnsf;

static Uint __fastcall APUSoundRead(Uint address)
{
	if (0x4015 == address)
	{
		int key = 0;
		if (apu.square[0].key && apu.square[0].lc.counter) key |= 1;
		if (apu.square[1].key && apu.square[1].lc.counter) key |= 2;
		if (apu.triangle.key && apu.triangle.lc.counter) key |= 4;
		if (apu.noise.key && apu.noise.lc.counter) key |= 8;
		if (apu.dpcm.length) key |= 16;
		key |= apu.dpcm.irq_report;
		key |= nsfnsf->vsyncirq_fg;
		apu.dpcm.irq_report = 0;
		nsfnsf->vsyncirq_fg = 0;
		return key;
	//	return key | 0x40 | apu.dpcm.irq_report;
	}
	if (0x4000 <= address && address <= 0x4017)
		return 0x40; //$4000`$4014‚ð“Ç‚ñ‚Å‚àA‘S•”$40‚ª•Ô‚Á‚Ä‚­‚éiƒtƒ@ƒ~ƒx’²‚×j 
	return 0xFF;
}

static NES_READ_HANDLER s_apu_read_handler[] =
{
	{ 0x4000, 0x4017, APUSoundRead, },
	{ 0,      0,      0, },
};

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

static Uint __fastcall GetNTSCPAL(void)
{
	Uint8 *nsfhead = NSFGetHeader();
	if (nsfhead[0x7a] & 1){
		return 13;
	}
	else
	{
		return 12;
	}

}
static void NESAPUSoundSquareReset(NESAPU_SQUARE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_SQUARE));
	ch->cps = DivFix(NES_BASECYCLES, GetNTSCPAL() * NESAudioFrequencyGet(), CPS_BITS);
}
static void NESAPUSoundTriangleReset(NESAPU_TRIANGLE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_TRIANGLE));
	ch->cps = DivFix(NES_BASECYCLES, GetNTSCPAL() * NESAudioFrequencyGet(), CPS_BITS);
	ch->st=0x8;
}
static void NESAPUSoundNoiseReset(NESAPU_NOISE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_NOISE));
	ch->cps = DivFix(NES_BASECYCLES, GetNTSCPAL() * NESAudioFrequencyGet(), CPS_BITS);

	//ƒmƒCƒY‰¹‚Ìƒ‰ƒ“ƒ_ƒ€‰Šú‰»
	if(NSF_noise_random_reset){
		srand(time(NULL) + clock());
		ch->rng = rand() + (rand()<<16);
	}else{
		ch->rng = 0x8000;
	}
	ch->rngcount = 0;
}


static void NESAPUSoundDpcmReset(NESAPU_DPCM *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_DPCM));
	ch->cps = DivFix(NES_BASECYCLES, 12 * NESAudioFrequencyGet(), CPS_BITS);
}

static void __fastcall APUSoundReset(void)
{
	Uint i;
	NESAPUSoundSquareReset(&apu.square[0]);
	NESAPUSoundSquareReset(&apu.square[1]);
	NESAPUSoundTriangleReset(&apu.triangle);
	NESAPUSoundNoiseReset(&apu.noise);
	NESAPUSoundDpcmReset(&apu.dpcm);
	apu.cpf[1] = DivFix(NES_BASECYCLES, 12 * 240, CPS_BITS);
//		apu.cpf[2] = DivFix(NES_BASECYCLES, 12 * 240 * 5 / 6, CPS_BITS);

	apu.cpf[0] = apu.cpf[1];
	apu.cpf[2] = 0;
	apu.square[0].sw.ch = 0;
	apu.square[1].sw.ch = 1;
	apu.square[0].cpf = apu.cpf;
	apu.square[1].cpf = apu.cpf;
	apu.triangle.cpf = apu.cpf;
	apu.noise.cpf = apu.cpf;
	apu.triangle.li.cpf = apu.cpf;

	apu.triangle.cpb180 = DivFix(NES_BASECYCLES, 12 * NES_BASECYCLES / 180/8, CPS_BITS);//Žb’è

	for (i = 0; i <= 0x17; i++)
	{
		APUSoundWrite(0x4000 + i, (i == 0x10) ? 0x10 : 0x00);
	}
	APUSoundWrite(0x4015, 0x0f);
/*	
#define TBL_MAX (1 << (AMPTML_BITS-2))
#define OUTPUT_VOL 4
#define SQRT_MIN 1.0
#define MINUS_CALC(x) ((1 << AMPTML_BITS) / (((1 << AMPTML_BITS) - x) * 0.0000001 + 1.2))

	for (i = 0; i < (1 << AMPTML_BITS); i++)
	{
//		apu.amptbl[i] = (Int32)(i * OUTPUT_VOL - (CV_MAX / (1.0 - j * 0.99 / CV_MAX) * OUTPUT_VOL));
//		apu.amptbl[i] = (Int32)((sqrt(SQRT_MIN + i / ((1 << AMPTML_BITS) * 0.05)) - sqrt(SQRT_MIN)) * 0x8000);
//		apu.amptbl[i] = (Int32)((200.0 / (100.0 / ((i+1.0) / (1 << AMPTML_BITS)) + 100)) / 2 * 0x20000);
//		apu.amptbl[i] = (Int32)(i - (sqrt(0.25 + i / ((1 << AMPTML_BITS) * 0.3)) - 0.5) * 0x2);
		apu.amptbl[i] = (Int32)((i - (MINUS_CALC(i) - MINUS_CALC(0)) ) * 8);
	}
*/

	for (i = 0; i < 128; i++)
	{
		//DPCM‚ÌDAC•”•ª‚ÍA0F->10A2F->30A3F->40A4F->50A6F->70 ‚Ìo—Í·‚ÍA‘¼‚ÌŽž‚Æ”ä‚×‚Ä1.5”{‚­‚ç‚¢•‚ª‚ ‚éB
		//‚³‚ç‚ÉA1F->20A5F->60 ‚Ìo—Í·‚ÍA‘¼‚ÌŽž‚Æ”ä‚×‚Ä2”{‚­‚ç‚¢•‚ª‚ ‚éB
		apu.dpcm.dactbl[i] = i * 2 + (i >> 4) + ((i + 0x20) >> 5);
	}

#if 1
	apu.dpcm.first = 1;
#endif
}

static NES_RESET_HANDLER s_apu_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, APUSoundReset, }, 
	{ 0,                   0, }, 
};

static void __fastcall APUSoundTerm(void)
{
}

static NES_TERMINATE_HANDLER s_apu_terminate_handler[] = {
	{ APUSoundTerm, }, 
	{ 0, }, 
};

//‚±‚±‚©‚çƒŒƒWƒXƒ^ƒrƒ…ƒA[Ý’è
Uint8 *regdata_2a03;
Uint32 (*ioview_ioread_DEV_2A03)(Uint32 a);
static Uint32 ioview_ioread_bf(Uint32 a){
	if(a<=0x17)return regdata_2a03[a];else return 0x100;
}
//‚±‚±‚Ü‚ÅƒŒƒWƒXƒ^ƒrƒ…ƒA[Ý’è



void APUSoundInstall(void)
{
	XMEMSET(&apu, 0, sizeof(APUSOUND));

	LogTableInitialize();
	NESAudioHandlerInstall(s_apu_audio_handler);
	NESVolumeHandlerInstall(s_apu_volume_handler);
	NESReadHandlerInstall(s_apu_read_handler);
	NESWriteHandlerInstall(s_apu_write_handler);
	NESResetHandlerInstall(s_apu_reset_handler);
	NESResetHandlerInstall(s_apu_reset_handler);

	//‚±‚±‚©‚çƒŒƒWƒXƒ^ƒrƒ…ƒA[Ý’è
	regdata_2a03 = apu.regs;
	ioview_ioread_DEV_2A03 = ioview_ioread_bf;
	//‚±‚±‚Ü‚ÅƒŒƒWƒXƒ^ƒrƒ…ƒA[Ý’è
}
