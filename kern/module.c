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
int dependent_count = 0;
char *dependencies_tmp[MAX_LKM];
int dependency_graph[MAX_LKM][MAX_LKM];

void initialize_lkm(void)
{
	int i,j=0;
	if(modules_lkm == NULL){
		panic("Failed to Initialize Modules \n");
	}
	for(i = 0; i < MAX_LKM; i++){
		modules_lkm[i].depd_count = 0;
		modules_lkm[i].load_status = 0;
		modules_lkm[i].load_addr = (uint64_t *)(LKM_ADDR_START + (i*(uint64_t)(PGSIZE + (1024*1024))));	//Keeping 1 page extra then 1 MB.
		for(j=0; j<MAX_LKM; j++){
			modules_lkm[i].depd_mod[j]=0;
		}
	}

}

int load_module(char *buf, char *file)
{
	int ret=0,i=0,j=0,r=0;
	elf = (struct Elf *)buf;
	if(elf->e_magic != ELF_MAGIC){
		cprintf("Error Occured while Loading the module. ELF Header is:%08x, Required is: %08x\n", elf->e_magic, ELF_MAGIC);
		return -1;
	}
	ret = check_module(file);
	if(ret != -1){
		cprintf("\n\t***** Module:%s is already loaded *****\t\n",file);
		return -1;
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
		flush_module(&modules_lkm[mod_num], mod_num);
		return -1;
	}
	r = calculate_load_addr(&modules_lkm[mod_num], ret);
	if(r < 0){
		cprintf("Failed to generate Load Address for the Module. Aborting..\n");
		flush_module(&modules_lkm[mod_num], mod_num);
		return -1;
	}
	r = prepare_mod_for_load(&modules_lkm[mod_num],modules_lkm[mod_num].sym_tab_index,modules_lkm[mod_num].st_tab_index);
	if(r < 0){
                cprintf("Unable to locate lkm_init or lkm_exit in the Module. Aborting..\n");
                flush_module(&modules_lkm[mod_num], mod_num);
                return -1;
        }
	r = allocate_module(&modules_lkm[mod_num],ret);
	if(r < 0){
		cprintf("Failed to allocate Memory for Module. Aborting...\n");
		flush_module(&modules_lkm[mod_num], mod_num);
		return -1;
	}

	modules_lkm[mod_num].load_status = 1;
	r = load_final_module(&modules_lkm[mod_num],ret);
	if(r < 0){
                flush_module(&modules_lkm[mod_num], mod_num);
                return -1;
        }
	calculate_dependencies(&modules_lkm[mod_num]);
	calculate_reverse_dependency(&modules_lkm[mod_num],mod_num);
	modules_lkm[mod_num].init_mod();
	mod_num = 0;
	dependent_count = 0;
        memset(dependencies_tmp, '\0', sizeof(dependencies_tmp));
	cprintf("\n\t********** Module: %s Successfully Loaded **********\t\n\n",file);
	return 0;
}

int check_module(char *mod)
{
	int ret,i=0;
	ret=1;
	for(i = 0; i < MAX_LKM; i++){
		if((modules_lkm[i].load_status == 1) && ((ret = strcmp(modules_lkm[i].name, mod)) == 0)){
			return i;
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

int apply_relocation(struct LKM_Module *lkm_mod, int sect_num)
{
        int i=0;
        uint64_t sym_val;
        struct Secthdr *to_rel_sect, *secthdr = &lkm_mod->sect_hdr[sect_num];
        struct Elf64_Rela *rela = (void *)((char *)lkm_mod->tmp_buf + secthdr->sh_offset);
        size_t size = secthdr->sh_size / sizeof(*rela);
	void *rel = (void *)((char *)lkm_mod->tmp_buf + lkm_mod->sect_hdr[secthdr->sh_info].sh_offset);
        struct Symhdr *symhdr = (void *)((char *)lkm_mod->tmp_buf + lkm_mod->sect_hdr[secthdr->sh_link].sh_offset); 
	//secthdr->sh_link will give section header index of symbol table. sh_offset will give me offset of symbol table. Add it to base of file to get the actual address of symbol table.
        to_rel_sect = &lkm_mod->sect_hdr[secthdr->sh_info];
        uint64_t to_rel_size = to_rel_sect->sh_size;
        struct Elf64_Rela *ptr_rela = rela;
        while(i < size){
                if(ELF64_R_TYPE(ptr_rela->r_info) == R_X86_64_64){
                        sym_val = find_symbol(lkm_mod, to_rel_sect, symhdr, ptr_rela);
                        if(sym_val == 0){
                                cprintf("Unable to find symbol value..\n");
                                return -1;
                        }
                        *((uint64_t *)((char *)rel + ptr_rela->r_offset)) = sym_val;
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
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".text" ))==0){
				flag = 1;
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".rodata" ))==0){
				flag = 1;
			}else if((ret = strcmp(secthdr->sh_name + lkm_mod->shname_st_table, ".bss"))==0){
				flag = 1;
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
				}
				flag = 0;
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
	struct Symhdr *rela_sym = &symhdr[ELF64_R_SYM(rela->r_info)];
	char *str_tab = lkm_mod->st_table;
	uint64_t sym_val;
	int ret=0,ret2=0;
	switch(rela_sym->st_shndx){
		case ELF_SHN_UNDEF:
//			cprintf("Undefined Symbol:%s \n", (void *)((char *)lkm_mod->st_table + rela_sym->st_name));
			sym_val = fetch_symbol_addr(rela_sym->st_name +str_tab);
			if(sym_val == 0){
				cprintf("Unable to find %s(). Probably its Dependent on some Module which is not loaded.....\n",(void *)((char *)lkm_mod->st_table + rela_sym->st_name));
				return 0;
			}
			dependencies_tmp[dependent_count]=fetch_symbol_tag(rela_sym->st_name +str_tab);
			if(((ret = strncmp("kernel", dependencies_tmp[dependent_count], strlen("kernel"))) != 0) && ((ret2=strlen(dependencies_tmp[dependent_count]))!=0)){
//				cprintf("Symbol: %s Dependent on %s Dependent count = %d \n", rela_sym->st_name +str_tab, dependencies_tmp[dependent_count],dependent_count);
				lkm_mod->depd_count++;
				dependent_count++;
			}else{
				memset(dependencies_tmp[dependent_count],'\0', sizeof(dependencies_tmp[dependent_count]));
			}
			sym_val = sym_val + rela->r_addend;
			break;
		case ELF_SHN_ABS:
			sym_val = rela_sym->st_value;
			break;
		case ELF_SHN_COMMON:
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
			if(ELF64_ST_TYPE(rela_sym->st_info) == STT_FUNC){
//				cprintf("symbol name is %s symbol value is %x \n",(str_tab + rela_sym->st_name), sym_val);
				if((ret = strncmp(str_tab + rela_sym->st_name, "lkm_init", strlen("lkm_init")) != 0) && (ret = strncmp(str_tab + rela_sym->st_name, "lkm_exit", strlen("lkm_exit")))!= 0){
					if((ret = strlen(str_tab + rela_sym->st_name)) != 0){	//new code
						insert_symbol(str_tab + rela_sym->st_name, sym_val, modules_lkm[mod_num].name);
					}
				}
			}
			break;
	}
	return sym_val;
}

int prepare_mod_for_load(struct LKM_Module *lkm_mod, int sym_index, int str_index)
{
	int i,ret=0, flag=0;
	struct Secthdr *secthdr = lkm_mod->sect_hdr;
	struct Symhdr *tsymhdr,*symhdr = (void *)(lkm_mod->tmp_buf + secthdr[sym_index].sh_offset);
        tsymhdr = symhdr;
        char *sttab = (void *)(lkm_mod->tmp_buf + secthdr[str_index].sh_offset);
        size_t sym_size = secthdr[sym_index].sh_size / (sizeof(struct Symhdr));
        i=0;
        while(i < sym_size){
                if(ELF64_ST_TYPE(tsymhdr->st_info) == STT_FUNC){
			if((ret = strcmp("lkm_init", sttab + tsymhdr->st_name)) == 0){
				modules_lkm[mod_num].init_mod = (void (*) (void))lkm_mod->text_addr + tsymhdr->st_value;
				flag++;
			}else if((ret = strcmp("lkm_exit", sttab + tsymhdr->st_name)) == 0){
                        	modules_lkm[mod_num].unload_mod = (void (*) (void))lkm_mod->text_addr + tsymhdr->st_value;
				flag++;
			}else if((ret = strlen(sttab + tsymhdr->st_name)) != 0){
				insert_symbol(sttab + tsymhdr->st_name, (uintptr_t)(lkm_mod->text_addr + tsymhdr->st_value), modules_lkm[mod_num].name);
//				cprintf("Inserting Symbol Name : %s Symbol addr: %x \n", sttab + tsymhdr->st_name, (uintptr_t)(lkm_mod->text_addr + tsymhdr->st_value));
			}	
                }
                i++;
                tsymhdr++;
        }
	if(flag != 2)
		return -1;
	return 0;
}

void flush_module(struct LKM_Module *lkm_mod, int num_mod)
{
	int i=0,j=0;
	invalidate_hash_mod(lkm_mod->name);
	memset(lkm_mod, '\0', sizeof(struct LKM_Module));
	lkm_mod->depd_count = 0;
        lkm_mod->load_status = 0;
        lkm_mod->load_addr = (uint64_t *)(LKM_ADDR_START + (num_mod*(uint64_t)(PGSIZE + (1024*1024))));	
	dependent_count = 0;
	memset(dependencies_tmp, '\0', sizeof(dependencies_tmp));
	for(j=0; j<MAX_LKM; j++){
                        lkm_mod->depd_mod[j]=0;
	}
}

void calculate_dependencies(struct LKM_Module *lkm_mod)
{
	int i=0, n=-1;
	int size = lkm_mod->depd_count;
	for(i=0; i<size; i++){
		n = check_module(dependencies_tmp[i]);
		if(n != -1){
			lkm_mod->depd_mod[n] = 1;
		}
	}
}

void print_dependencies(struct LKM_Module *lkm_mod)
{
	int i=0;
	for(i=0; i<MAX_LKM; i++){
		if(lkm_mod->depd_mod[i] == 1){
			
			print_dependencies(&modules_lkm[i]);
			cprintf("Dependent on: %s \n", modules_lkm[i].name);
		}
	}
	
}

void print_mod_dep(char *file)
{
	int ret=0;
        ret = check_module(file);
        if(ret == -1){
                cprintf("\n\t***** Module:%s is Not loaded *****\t\n",file);
                return;
        }
	if(modules_lkm[ret].depd_count > 0){
		cprintf("\n***Module: %s is dependent on Following  modules*** \n",file, modules_lkm[ret].depd_count);
		print_dependencies(&modules_lkm[ret]);
	}else{
		cprintf("\n\t ****** Module : %s has NO DEPENDENCIES ****** \n",file);
	}
	cprintf("\n");
}

void print_loaded_modules(void)
{
	int i=0, flag=0;
	for(i=0; i<MAX_LKM; i++){
                if(modules_lkm[i].load_status == 1){
                	flag = 1;
			break;
		}
        }
	if(flag == 0){
		cprintf("\n\t***** No Modules are at present Loaded in the kernel *****\n\n");
		return;
	}
	cprintf("\n***Following Modules are Loaded at Present***\n");
	for(i=0; i<MAX_LKM; i++){
		if(modules_lkm[i].load_status == 1){
			cprintf("Module Name: %s \t Load Addr: %x \n", modules_lkm[i].name, modules_lkm[i].load_addr);
		}
	}
	cprintf("\n");
}

void calculate_reverse_dependency(struct LKM_Module *lkm_mod, int mod_add_num)
{
	int i=0;
        for(i=0; i<MAX_LKM; i++){
                if(lkm_mod->depd_mod[i] == 1){
			dependency_graph[mod_add_num][i]=1;
                	calculate_reverse_dependency(&modules_lkm[i],mod_add_num);
		}
        }
}

void remove_reverse_dependency(struct LKM_Module *lkm_mod, int mod_rm_num)
{
	int i=0;
	for(i=0; i<MAX_LKM; i++){
		dependency_graph[mod_rm_num][i]=0;
	}
}
void remove_mod(char *file)
{
	int ret=0,i=0,j=0,flag=0;
        ret = check_module(file);
        if(ret == -1){
                cprintf("\n\t***** Module:%s is Not loaded *****\t\n\n",file);
                return;
        }
	if(modules_lkm[ret].load_status != 1){
		cprintf("\n\t***** Module:%s is Not loaded *****\t\n\n",file);
		return;
	}
	for(i=0; i<MAX_LKM; i++){
		if(dependency_graph[i][ret] == 1 ){
			flag=1;
			break;
		}
	}
	if(flag == 1){
		cprintf("\n***Module cannot be removed because following modules are dependent on it ***\n");
		for(i=0; i<MAX_LKM; i++){
                	if(dependency_graph[i][ret] == 1 ){
				cprintf("Module Name: %s \n",modules_lkm[i].name);
                	}
        	}
		return;
	}
	modules_lkm[ret].unload_mod();
	remove_reverse_dependency(&modules_lkm[ret], ret);
	flush_module(&modules_lkm[ret], ret);
}

