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
#include <kern/hash.h>
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
	r = calculate_load_addr(&modules_lkm[mod_num], ret);
/*	r = allocate_module(&modules_lkm[mod_num], ret);
	if(r < 0){
		cprintf("Failed to allocate Memory for Module. Aborting...\n");
		return -1;
	}
	r = load_final_module(&modules_lkm[mod_num],ret);
	int z=0;
	//		cprintf("Data Addr: 0x%016x ,Text Addr: 0x%016x, Rodata Addr: 0x%016x, Bss Addr: 0x%016x Number of Pages Allocated: %d \n", modules_lkm[mod_num].data_addr, modules_lkm[mod_num].text_addr, modules_lkm[mod_num].rodata_addr, modules_lkm[mod_num].bss_addr, modules_lkm[mod_num].num_pages);
			modules_lkm[mod_num].init_mod = (void (*) (void))modules_lkm[mod_num].text_addr;
	//	cprintf("Value of function pointer = 0x%016x \n", modules_lkm[mod_num].init_mod);	
			modules_lkm[mod_num].init_mod();
//		char *tmpd = modules_lkm[mod_num].text_addr;
//		for(z=0; z<200; z++)
//		cprintf("%c",tmpd[z]);
	 
	//	cprintf("Printing Module details:\n");
*/
	
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
/*
   int allocate_module(struct LKM_Module *lkm_mod, int e_shnum)
   {
   int i=0, j=0, ret=0, flag=0;
   size_t size,size_p;
   int perm = PTE_P | PTE_W ;
   struct Secthdr *secthdr = lkm_mod->sect_hdr;
   void *start, *va = lkm_mod->load_addr;
   lkm_mod->num_pages = 0;


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
   if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".data" ))==0){
   flag = 1;
//				cprintf("Entered into data section. start: 0x%016x, size=0x%08x\n",start,size);
lkm_mod->data_addr = start;			
}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".text" ))==0){
flag = 1;
//				cprintf("Entered into text section. start: 0x%016x, size=0x%08x\n",start,size);
lkm_mod->text_addr = start;
}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".rodata" ))==0){
flag = 1;
//				cprintf("Entered into rodata section. start: 0x%016x size=0x%08x\n",start, size);
lkm_mod->rodata_addr = start;
}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".bss"))==0){
flag = 1;
//				cprintf("Entered into bss section. start: 0x%016x size=0x%08x\n",start, size);
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
memcpy(start, (void *)((char *)lkm_mod->tmp_buf + secthdr->sh_offset), size);
}	
}
i++;
secthdr++;
}
i = 0;
ret = 0;
secthdr = lkm_mod->sect_hdr;
while(i < e_shnum){
if((secthdr->sh_type == ELF_SHT_RELA) && (ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".rela.eh_frame" ) != 0)){
cprintf("Fixing Relocation for Section Number : %d \n",i);
ret = apply_relocation(lkm_mod, i);
if(ret < 0){
cprintf("Failed to Apply Relocation. Aborting..\n");
return -1;
}
}else if(secthdr->sh_type == ELF_SHT_REL){
	cprintf("REL Section. Not Handling it here \n");
}
i++;
secthdr++;
} 
return 0;
}
	*/
int allocate_module(struct LKM_Module *lkm_mod, int e_shnum)
{
	int i=0, j=0, ret=0, flag=0;
	size_t size,size_p;
	int perm = PTE_P | PTE_W ;
	struct Secthdr *secthdr = lkm_mod->sect_hdr;
	void *start, *va = lkm_mod->load_addr;
	lkm_mod->num_pages = 0;


	while(i < e_shnum){
		if((secthdr->sh_type == ELF_SHT_RELA) && (ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".rela.eh_frame" ) != 0)){
			cprintf("Fixing Relocation for Section Number : %d \n",i);
			ret = apply_relocation(lkm_mod, i);
			if(ret < 0){
				cprintf("Failed to Apply Relocation. Aborting..\n");
				return -1;
			}
		}else if(secthdr->sh_type == ELF_SHT_REL){
			cprintf("REL Section. Not Handling it here \n");
		}
		i++;
		secthdr++;
	}
	return 0;

}
int calculate_load_addr(struct LKM_Module *lkm_mod, int e_shnum)
{
	int i=0, j=0, ret=0, flag=0;
	size_t size,size_p;
	struct Secthdr *secthdr = lkm_mod->sect_hdr;
	void *start, *va = lkm_mod->load_addr;
	lkm_mod->num_pages = 0;


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
			if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".data" ))==0){
				lkm_mod->data_addr = start;
				va = va + (PGSIZE * (size_p / PGSIZE));
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".text" ))==0){
				lkm_mod->text_addr = start;
				va = va + (PGSIZE * (size_p / PGSIZE));
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".rodata" ))==0){
				lkm_mod->rodata_addr = start;
				va = va + (PGSIZE * (size_p / PGSIZE));
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".bss"))==0){
				lkm_mod->bss_addr = start;
				va = va + (PGSIZE * (size_p / PGSIZE));
			}
		}
		i++;
		secthdr++;
	}
	return 0;
}
int apply_relocation(struct LKM_Module *lkm_mod, int sect_num)
{
	int i=0;
	uint64_t sym_val;
	struct Secthdr *to_rel_sect, *secthdr = &lkm_mod->sect_hdr[sect_num];
	struct Elf64_Rela *rela = (void *)((char *)lkm_mod->tmp_buf + secthdr->sh_offset);
	size_t size = secthdr->sh_size / sizeof(*rela);
	//	void *rel = (void *)((char *)lkm_mod->tmp_buf + lkm_mod->sect_hdr[secthdr->sh_info].sh_offset);
	struct Symhdr *symhdr = (void *)((char *)lkm_mod->tmp_buf + lkm_mod->sect_hdr[secthdr->sh_link].sh_offset); //secthdr->sh_link will give section header index of symbol table. sh_offset will give me offset of symbol table. Add it to base of file to get the actual address of symbol table.
	to_rel_sect = &lkm_mod->sect_hdr[secthdr->sh_info];
	uint64_t to_rel_size = to_rel_sect->sh_size;
	//	cprintf("There are %d entries in rela section\n", size);
	struct Elf64_Rela *ptr_rela = rela;
	while(i < size){
		if(ELF64_R_TYPE(ptr_rela->r_info) == R_X86_64_64){
			sym_val = find_symbol(lkm_mod, to_rel_sect, symhdr, ptr_rela);
			if(sym_val == 0){
				cprintf("Unable to find symbol value..\n");
				return -1;
			}
		//	cprintf("\n ******* \n");
		//	cprintf("(S+A) = %x, and roff = %x\n", sym_val, ptr_rela->r_offset);
		//	cprintf("\n ******* \n");
		//	cprintf("Value at secthdr : 0x%016x \n", *((uint64_t *)((char *)secthdr + ptr_rela->r_offset)));
			*((uint64_t *)((char *)secthdr + ptr_rela->r_offset)) = sym_val;
		//	cprintf("Value at secthdr : 0x%016x \n", *((uint64_t *)((char *)secthdr + ptr_rela->r_offset)));
		//			cprintf("From lkm_mod: Secthdr : 0x%016x \n", ()(char *)modules_lkm[mod_num].sect_hdr[sect_num] + ptr_rela->r_offset);
		}
		i++;
		ptr_rela++;
	}
	return 0;
}

int load_final_module(struct LKM_Module *lkm_mod, int e_shnum)
{
	int i=0, j=0, ret=0, flag=0;
	size_t size,size_p;
	int perm = PTE_P | PTE_W ;
	struct Secthdr *secthdr = lkm_mod->sect_hdr;
	void *start, *va = lkm_mod->load_addr;
	lkm_mod->num_pages = 0;


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
			if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".data" ))==0){
				flag = 1;
				//                              cprintf("Entered into data section. start: 0x%016x, size=0x%08x\n",start,size);
//				lkm_mod->data_addr = start;                     
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".text" ))==0){
				flag = 1;
				//                              cprintf("Entered into text section. start: 0x%016x, size=0x%08x\n",start,size);
//				lkm_mod->text_addr = start;
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".rodata" ))==0){
				flag = 1;
				//                              cprintf("Entered into rodata section. start: 0x%016x size=0x%08x\n",start, size);
//				lkm_mod->rodata_addr = start;
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".bss"))==0){
				flag = 1;
				//                              cprintf("Entered into bss section. start: 0x%016x size=0x%08x\n",start, size);
//				lkm_mod->bss_addr = start;
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
					lkm_mod->num_pages++; //++ to +=1
					//                                      cprintf("Value of Va = 0x%016x \n",va);
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

uint64_t find_symbol(struct LKM_Module *lkm_mod, struct Secthdr *secthdr, struct Symhdr *symhdr, struct Elf64_Rela *rela)
{
	//	cprintf("symbol table index is %d \n", ELF64_R_SYM(rela->r_info));	
	struct Symhdr *rela_sym = &symhdr[ELF64_R_SYM(rela->r_info)];
	char *str_tab = lkm_mod->st_table;
	uint64_t sym_val;
	switch(rela_sym->st_shndx){
		case ELF_SHN_UNDEF:
			//				cprintf("Undefined Symbol:%s \n", (void *)((char *)lkm_mod->st_table + rela_sym->st_name));
			sym_val = fetch_symbol_addr(rela_sym->st_name +str_tab);
			if(sym_val == 0){
				cprintf("Unable to find symbol in Kernel Symbol Table \n");
				return 0;
			}
			sym_val = sym_val + rela->r_addend;
			cprintf("Symbol: %s , Value: 0x%0x \n", (rela_sym->st_name +str_tab), sym_val);
			break;
		case ELF_SHN_ABS:
			//				cprintf("ABS Symbol : %x \n",rela_sym->st_value);
			sym_val = rela_sym->st_value;
			break;
		case ELF_SHN_COMMON:
			//				cprintf("Common Symbol. \n");
			sym_val = 0;
			break;
		default:
			if(rela_sym->st_shndx > lkm_mod->elf_hdr->e_shnum){
				cprintf("Error while relocating..\n");
				return 0;
			}
			struct Secthdr *tmp_sect = &lkm_mod->sect_hdr[rela_sym->st_shndx];
			uint64_t addr;
			if(strcmp((lkm_mod->shname_st_table + tmp_sect->sh_name),".data") == 0){
				addr = (uint64_t)lkm_mod->data_addr;
			}else if(strcmp((lkm_mod->shname_st_table + tmp_sect->sh_name),".text") == 0){
				addr = (uint64_t)lkm_mod->text_addr;
			}else if(strcmp((lkm_mod->shname_st_table + tmp_sect->sh_name),".rodata") == 0){
				addr = (uint64_t)lkm_mod->rodata_addr;
			}else if(strcmp((lkm_mod->shname_st_table + tmp_sect->sh_name),".bss") == 0){
				addr = (uint64_t)lkm_mod->bss_addr;
			}
			sym_val = addr + rela_sym->st_value + rela->r_addend;
		//	cprintf(" load_addr = %x symbol name is %s symbol value is %x and st_value = %d\n", addr,  (str_tab + rela_sym->st_name), sym_val, rela_sym->st_value);
			break;
	}
	return sym_val;
}
