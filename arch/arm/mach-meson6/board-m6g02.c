/*
This file is to select a different board file for the same platform
*/



#ifdef CONFIG_MACH_MESON6_G02

	#ifdef CONFIG_MACH_MESON6_ATV520
	#include "board-atv520.c"
	#endif
	#ifdef CONFIG_MACH_MESON6_ATV1200
	#include "board-atv1200.c"
	#endif
#endif


