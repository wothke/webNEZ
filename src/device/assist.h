#ifndef __ASSIST_H__
#define __ASSIST_H__

// XXX FIXME this file should be unused once merge of NEZplug++ is 
// complete (currently still used in "devices")

#include "nezglue_x.h"

static char defmask[12]={1,1,1,1, 1,1,1,1, 1,1,1,1};
static char *chmask = defmask;

#define NES_BASECYCLES (21477270)
#define NES_CPUCLOCK (NES_BASECYCLES / 12)

static S_STATE defstate[12]=
{
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0},
    {1,0,0}
};

static S_STATE *chstate = defstate;

#endif