#include <inc/lib.h>
void lsmod()
{
	sys_lsmod();
}
void umain(int argc, char **argv)
{
	binaryname = "lsmod";
	if(argc < 2){
		lsmod();
	}else{
		cprintf("Arguments not required\n");
	}
}
