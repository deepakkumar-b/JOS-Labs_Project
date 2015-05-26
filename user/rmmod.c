#include<inc/lib.h>

void rmmod(char *file)
{
	sys_rmmod(file);
}

void umain(int argc, char **argv)
{
	binaryname = "rmmod";
	if(argc > 1){
		rmmod(argv[1]);
	}else{
		cprintf("Module name missing\n ");
	}
}

