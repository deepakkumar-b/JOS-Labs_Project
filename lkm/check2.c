int lkm_init(void)
{
        cprintf("Check 2 loaded :) \n");
}

void lkm_exit(void)
{
        cprintf("Exiting Check 2 \n");
}

int check_fun2(void)
{
        cprintf("check_fun2 called \n");
	check_fun1();
}

