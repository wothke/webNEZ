#include "m_nsf.h"

extern NSFNSF *nsfnsf;

extern void FDSSoundInstall1(void);
extern void FDSSoundInstall2(void);
extern void FDSSoundInstall3(void);
//extern int FDSSoundInstallExt(void);

void FDSSoundInstall(void)
{
	switch (nsfnsf ? nsfnsf->fds_type : 2)
	{
	case 1:
		FDSSoundInstall1();
		break;
	case 3:
		FDSSoundInstall2();
		break;
#if 0
	case 0:
//		if (FDSSoundInstallExt()) break;
		/* fall down */
#endif
	default:
	case 2:
		FDSSoundInstall3();
		break;
	}
}

void FDSSelect(unsigned type)
{
	if (nsfnsf)
		nsfnsf->fds_type = type;
}
