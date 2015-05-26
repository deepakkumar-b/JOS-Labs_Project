int lkm_init(void)
{
	cprintf("Depd2 _ check \n");
	check_mod_file();
	check_mod_depd_file();
	return 0;
}

void lkm_exit(void)
{
	cprintf("Exiting \n");
}
