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
#define LKM_ADDR_START  0x800A400000    //100 Mbs above KERNBASE

struct LKM_Module *modules_lkm=NULL;
struct Elf *elf;
int mod_num=-1;

void initialize_lkm(void)
{
        int i;
        if(modules_lkm == NULL){
                panic("Failed to Initialize Modules \n");
        }
        for(i = 0; i < MAX_LKM; i++){
                modules_lkm[i].load_status = 0;
                modules_lkm[i].load_addr = (uint64_t *)(LKM_ADDR_START + (i*(uint64_t)(PGSIZE + (1024*1024)))); //Keeping 1 page extra then 1 MB.
        }
}

int load_module(char *buf, char *file)
{
        //      cprintf("load_module called with file name: %s \n",file);
        int ret=0,i=0,j=0,r=0, e_shnum=0;
        elf = (struct Elf *)buf;
        if(elf->e_magic != ELF_MAGIC){
                cprintf("Error Occured while Loading the module. ELF Header is:%08x, Required is: %08x\n", elf->e_magic, ELF_MAGIC);
                return -1;
        }
        //      cprintf("\n\n\t**********Inserting Module: %s **********\t\n\n",file);
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
	e_shnum = ret;
        if(ret > 0){
                calculate_sym_hdr(&modules_lkm[mod_num], ret);
        }else{
                cprintf("Failed to Load Sections and Symbols..\n");
                return -1;
        }
        r = calculate_load_addr(&modules_lkm[mod_num], ret);
        if(r < 0){
                cprintf("Failed to allocate Memory for Module. Aborting...\n");
                return -1;
        }
	cprintf("Data Addr: %x ,Text Addr: %x, Rodata Addr: %x, Bss Addr: %x\n", modules_lkm[mod_num].data_addr, modules_lkm[mod_num].text_addr, modules_lkm[mod_num].rodata_addr, modules_lkm[mod_num].bss_addr);
	//Temp Code
	i = 0;
	struct Secthdr *secthdr = modules_lkm[mod_num].sect_hdr;
	for(i = 0; i<e_shnum; i++,secthdr++){
		if(secthdr->sh_type == ELF_SHT_RELA && (strncmp(modules_lkm[mod_num].shname_st_table + secthdr->shname, ".rela.eh_frame", strlen(".rela.eh_frame"))!=0)){
			cprintf("Rela Section at : %d \n",i);
			apply_relocation(&modules_lkm[mod_num],i);

		}else if(secthdr->sh_type == ELF_SHT_REL){
			cprintf("REL Section at : %d \n",i);
		}
	}
	//End Code
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
        cprintf("symtab index @ %d and strtab index @ %d\n", lkm_mod->sym_tab_index, lkm_mod->st_tab_index);
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
                        if((ret = strncmp(secthdr->sh_name + lkm_mod->shname_st_table, ".data", strlen(".data") ))==0){
                                lkm_mod->data_addr = start;
                                va = va + (PGSIZE * (size_p / PGSIZE));
                        }else if((ret = strncmp(secthdr->sh_name + lkm_mod->shname_st_table, ".text", strlen(".text") ))==0){
                                lkm_mod->text_addr = start;
                                va = va + (PGSIZE * (size_p / PGSIZE));
                        }else if((ret = strncmp(secthdr->sh_name + lkm_mod->shname_st_table, ".rodata", strlen(".rodata")))==0){
                                lkm_mod->rodata_addr = start;
                                va = va + (PGSIZE * (size_p / PGSIZE));
                        }else if((ret = strncmp(secthdr->sh_name + lkm_mod->shname_st_table, ".bss", strlen(".bss")))==0){
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
        struct Elf64_Rela *rela = ((char *)lkm_mod->tmp_buf + secthdr->sh_offset);
        size_t size = secthdr->sh_size / sizeof(*rela);
        struct Symhdr *symhdr =((char *)lkm_mod->tmp_buf + lkm_mod->sect_hdr[secthdr->sh_link].sh_offset); //secthdr->sh_link will give section header index of symbol table. sh_offset will give me offset of symbol table. Add it to base of file to get the actual address of symbol table.
        to_rel_sect = &lkm_mod->sect_hdr[secthdr->sh_info];
        uint64_t to_rel_size = to_rel_sect->sh_size;
        cprintf("There are %d entries in rela section\n", size);
        struct Elf64_Rela *ptr_rela = rela;
        while(i < size){
                if(ELF64_R_TYPE(ptr_rela->r_info) == R_X86_64_64){
                        sym_val = find_symbol(lkm_mod, to_rel_sect, symhdr, ptr_rela);
                        if(sym_val == 0){
                                cprintf("Unable to find symbol value..\n");
                                return -1;
                        }
                        cprintf("\n ******* \n");
                        cprintf("(S+A) = %x, and roff = %x\n", sym_val, ptr_rela->r_offset);
                        cprintf("\n ******* \n");
                //      cprintf("Value at secthdr : 0x%016x \n", *((uint64_t *)((char *)secthdr + ptr_rela->r_offset)));
                        *((uint64_t *)((char *)secthdr + ptr_rela->r_offset)) = sym_val;
                //      cprintf("Value at secthdr : 0x%016x \n", *((uint64_t *)((char *)secthdr + ptr_rela->r_offset)));
        //              cprintf("From lkm_mod: Secthdr : 0x%016x \n",(char *)modules_lkm[mod_num].sect_hdr[sect_num] + ptr_rela->r_offset);
                }else if(ELF64_R_TYPE(ptr_rela->r_info) == R_X86_64_PC32){
                        cprintf("In Elf64_PC32 \n");
                }
                i++;
                ptr_rela++;
        }
        return 0;
}

