#include <kern/module.h>
#include <inc/elf.h>
#include <inc/types.h>
#include <kern/pci.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/mmu.h>
#include <kern/pmap.h>
#include <kern/env.h>
#define LKM_ADDR_START  0x800A400000	//100 Mbs above KERNBASE

struct LKM_Module *modules_lkm=NULL;
struct Elf *elf;
int mod_num=-1;
/*void *data_addr=NULL;
void *text_addr=NULL;
void *rodata_addr=NULL;
void *bss_addr=NULL;
*/
void initialize_lkm(void)
{
	int i;
	if(modules_lkm == NULL){
		panic("Failed to Initialize Modules \n");
	}
	for(i = 0; i < MAX_LKM; i++){
		modules_lkm[i].load_status = 0;
		modules_lkm[i].load_addr = (uint64_t *)(LKM_ADDR_START + (i*(uint64_t)(PGSIZE + (1024*1024))));	//Keeping 1 page extra then 1 MB.
	}

}

int load_module(char *buf, char *file)
{
//	cprintf("load_module called with file name: %s \n",file);
	int ret=0,i=0,j=0,r=0;
	elf = (struct Elf *)buf;
	if(elf->e_magic != ELF_MAGIC){
		cprintf("Error Occured while Loading the module. ELF Header is:%08x, Required is: %08x\n", elf->e_magic, ELF_MAGIC);
		return -1;
	}
//	cprintf("\n\n\t**********Inserting Module: %s **********\t\n\n",file);
	ret = check_module(file);
	if(ret == 0){
		cprintf("\n\t***** Module:%s is already loaded *****\t\n",file);
	}
	for(i = 0; i < MAX_LKM; i++){
		if(modules_lkm[i].load_status == 0){
			break;
		}
	}
	if(i == MAX_LKM){
		cprintf("\n ***No Space to Load the Module. Maximum Possible Number of Modules are already Loaded ***\n");
		return -1;
	}
	cprintf("\n\t**********Inserting Module: %s **********\t\n",file);
	mod_num = i;
	if(mod_num < 0){
		cprintf("\n ***No Space to Load the Module. Maximum Possible Number of Modules are already Loaded ***\n");
		return -1;
	}
	strncpy(modules_lkm[mod_num].name, file, sizeof(modules_lkm[mod_num].name)-1);
	modules_lkm[mod_num].tmp_buf = (void *)buf;
	modules_lkm[mod_num].elf_hdr = (struct Elf *)buf;
	cprintf("Name of Module: %s\n",modules_lkm[mod_num].name);
	ret = calculate_sect_hdr(&modules_lkm[mod_num]);
	if(ret > 0){
		calculate_sym_hdr(&modules_lkm[mod_num], ret);
	}else{
		cprintf("Failed to Load Sections and Symbols..\n");
		return -1;
	}
	r = allocate_module(&modules_lkm[mod_num], ret);
	if(r < 0){
		cprintf("Failed to allocate Memory for Module. Aborting...\n");
		return -1;
	}
	int z=0;
	cprintf("Data Addr: 0x%016x ,Text Addr: 0x%016x, Rodata Addr: 0x%016x, Bss Addr: 0x%016x Number of Pages Allocated: %d \n", modules_lkm[mod_num].data_addr, modules_lkm[mod_num].text_addr, modules_lkm[mod_num].rodata_addr, modules_lkm[mod_num].bss_addr, modules_lkm[mod_num].num_pages);
	return 0;
}

int check_module(char *mod)
{
	int ret,i=0;
	ret=1;
	for(i = 0; i < MAX_LKM; i++){
		if((modules_lkm[i].load_status == 1) && ((ret = strcmp(modules_lkm[i].name, mod)) == 0)){
			return 0;
		}
	}
	return -1;
}

int calculate_sect_hdr(struct LKM_Module *lkm_mod)
{
	int i=0;
	struct Secthdr *sectname;
	Elf64_Off e_shoff;
	Elf64_Half e_shnum;
	Elf64_Half e_shstrndx;

	e_shoff = lkm_mod->elf_hdr->e_shoff;
	e_shnum = lkm_mod->elf_hdr->e_shnum;
	e_shstrndx = lkm_mod->elf_hdr->e_shstrndx;

	lkm_mod->sect_hdr = (void *)((char *)lkm_mod->tmp_buf + e_shoff);
	
	sectname = &lkm_mod->sect_hdr[e_shstrndx];
	lkm_mod->shname_st_table = (void *)((char *)lkm_mod->tmp_buf + sectname->sh_offset);
	
	return (int)e_shnum;
}

int calculate_sym_hdr(struct LKM_Module *lkm_mod, int e_shnum)
{
	int i=0;
	struct Secthdr *secthdr = lkm_mod->sect_hdr;
	while(i < e_shnum){
		if(secthdr->sh_type == ELF_SHT_SYMTAB){
			lkm_mod->sym_hdr = (void *)((char *)lkm_mod->tmp_buf + secthdr->sh_offset);
			lkm_mod->sym_tab_index = i;
		}
		if((secthdr->sh_type == ELF_SHT_STRTAB) && (i != lkm_mod->elf_hdr->e_shstrndx)){
			lkm_mod->st_table = (void *)((char *)lkm_mod->tmp_buf + secthdr->sh_offset);
			lkm_mod->st_tab_index = i;
		}
		i++;
		secthdr++;
	}
//	cprintf("symtab index @ %d and strtab index @ %d\n", lkm_mod->sym_tab_index, lkm_mod->st_tab_index);
	return 0;
}

int allocate_module(struct LKM_Module *lkm_mod, int e_shnum)
{
	int i=0, j=0, ret=0, flag=0;
	size_t size,size_p;
	int perm = PTE_P | PTE_W ;
        struct Secthdr *secthdr = lkm_mod->sect_hdr;
	void *start, *va = lkm_mod->load_addr;
	lkm_mod->num_pages = 0;
	Elf64_Half e_shstrndx = lkm_mod->elf_hdr->e_shstrndx;
	struct Secthdr *sectname = &lkm_mod->sect_hdr[e_shstrndx];
	char *shname = (void *)((char *)lkm_mod->tmp_buf + sectname->sh_offset);

	while(i < e_shnum){
		if((secthdr->sh_flags & ELF_SHT_ALLOC)){
			size = secthdr->sh_size;
			size_p = ROUNDUP(size, PGSIZE);
			start = va;
			if(size == 0){
				i++;
				secthdr++;
				continue;
			}
			if((ret = strcmp(secthdr->sh_name + shname, ".data" ))==0){
				flag = 1;
				cprintf("Entered into data section. start: 0x%016x, size=0x%08x\n",start,size);
				lkm_mod->data_addr = start;			
			}else if((ret = strcmp(secthdr->sh_name + shname, ".text" ))==0){
				flag = 1;
				cprintf("Entered into text section. start: 0x%016x, size=0x%08x\n",start,size);
				lkm_mod->text_addr = start;
                        }else if((ret = strcmp(secthdr->sh_name + shname, ".rodata" ))==0){
				flag = 1;
				cprintf("Entered into rodata section. start: 0x%016x size=0x%08x\n",start, size);
				lkm_mod->rodata_addr = start;
                        }else if((ret = strcmp(secthdr->sh_name + shname, ".bss"))==0){
				flag = 1;
				cprintf("Entered into bss section. start: 0x%016x size=0x%08x\n",start, size);
				lkm_mod->bss_addr = start;
                        }
			if(flag == 1){
				for(j=0; j < (size_p / PGSIZE); j++){
                                	struct PageInfo *pg=page_alloc(ALLOC_ZERO);
                                	if(!pg){
                                        	cprintf("Failed to allocate Page. Loading Aborted \n");
                                        	return -1;
                                	}
                                	ret = page_insert(curenv->env_pml4e, pg, va, perm);
                                	if(ret < 0){
                                        	cprintf("Failed to Insert the Page. Loading Aborted \n");
                                        	return -1;
                                	}
                               		va = va + PGSIZE;
					lkm_mod->num_pages++;
//					cprintf("Value of Va = 0x%016x \n",va);
                        	}
				flag = 0;
				cprintf("tempbuf+shoff : %s\n",(char*)(lkm_mod->tmp_buf) + secthdr->sh_offset);
                        	memcpy(start, (void *)((char *)lkm_mod->tmp_buf + secthdr->sh_offset), size);
			}	
		}
		i++;
		secthdr++;
	}
	return 0;
}
