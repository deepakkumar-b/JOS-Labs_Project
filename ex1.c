//simple inline assembly example
//For JOS Lab 1 Ex 1
#include <stdio.h>

int main(int argc, char **argv)
{
	int x = 1;
	printf("Hello x = %d\n", x);

	__asm__("movl %1,%%eax;"
                "incl %%eax;"
                "movl %%eax,%1;"
                :"=a"(x)
                :"a"(x));

	printf("Hello x = %d after increment \n", x);
	if(x == 2)
	{
		printf("OK\n");
	}
	else
	{
		printf("Error\n");
	}
}
