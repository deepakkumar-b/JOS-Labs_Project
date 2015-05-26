#include <kern/hash.h>
#include <inc/lib.h>
#define BUF_TEMP	0xEEDFD000	//10 Mb below normal stack
void kern_symbol_hash(void)
{
	struct File *file;
	void *va = (void *)BUF_TEMP;
	void *buf = va;
	int num_pg,i;
	int ret = file_open("kernel", &file);
	if(ret < 0){
		cprintf("Failure while loading kernel Symbols \n");
		return -1;
	}
	size = ROUNDUP(file->f_size, PGSIZE);
	num_pg = (int)(size / PGSIZE);
	for(i = 0; i < num_pg; i++){
		struct PageInfo *pg;
		pg = page_alloc(ALLOC_ZERO);
		if(pg != NULL){
			ret = page_insert(curenv->env_pml4e, pg, va, (PTE_P | PTE_W));
			if(ret < 0){
				cprintf("Failure to insert page \n");
				return -1;
			}
		}else{
			cprintf("Failure to Allocate the page \n");
			return -1;
		}
	}
	ret = file_read(file, buf, file->f_size, 0); 
	if(ret != file->f_size){
		cprintf("File Read Incomplete...\n");
		return -1;
	}
	return 0;
}
