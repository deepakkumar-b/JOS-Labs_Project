int lkm_init(void)
{
	cprintf("Check 1 loaded :) \n");
}

void lkm_exit(void)
{
	cprintf("Exiting Check 1 \n");
}

int check_fun1(void)
{
	cprintf("check_fun1 called \n");
}
