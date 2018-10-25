/***********************************************************************
*
*  FILE        : test.c
*  DATE        : 2018-10-25
*  DESCRIPTION : Main Program
*
*  NOTE:THIS IS A TYPICAL EXAMPLE.
*
***********************************************************************/
#include "r_smc_entry.h"

void main(void);
void UserInit(void);

void main(void)
{
	UserInit();

	while(1)
	{
		nop();
		nop();
		nop();
	}
}

void UserInit(void)
{
	R_Config_CMT0_Start();
}
