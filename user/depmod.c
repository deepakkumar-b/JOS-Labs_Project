#include <inc/lib.h>

void depmod(char *file)
{
	sys_depmod(file);
}

void umain(int argc, char **argv)
{
        binaryname = "depmod";
        if(argc > 1){
                depmod(argv[1]);
        }else{
                cprintf("Module name missing \n");
        }
}

