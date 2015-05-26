void lkm_exit(void) {
//    lkm_init();
    cprintf("Good bye from LKM \n");
}
int lkm_init(void)
{
    cprintf("Hello world from LKM\n");
    return 0;
}

