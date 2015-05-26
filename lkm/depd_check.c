int lkm_init(void)
{
//	check_mod_depd();	
	return 0;
}

void lkm_exit(void)
{
	cprintf("Leaving depd_check \n");
}

void check_mod_depd_file(void)
{
	cprintf("1st level resolved \n");
//	check_mod_depd();
}
