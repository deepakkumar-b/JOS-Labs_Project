#include <kern/module.h>

#define DEBUG 1
static struct elf_module e_mod[MAX_LOAD_MOD];
static int no_modules = 0;
// 80 is not arbitrary, to represent 10 MB for the loadable kernel modules, you need an array of 80 int32_t
int32_t lkm_bitmap[80];
uint64_t get_symbol_val_from_hashtable(char *);
int init_kern_hash_table(void*, size_t);



static int populate_section_hdrs(struct elf_module *e_mod)
{
    Elf64_Off e_shoff;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
    Elf64_Shdr *s_shname;

    e_shoff = e_mod->e_hdr->e_shoff;
    e_shnum = e_mod->e_hdr->e_shnum;
    e_shstrndx = e_mod->e_hdr->e_shstrndx;
#if DEBUG
    cprintf("section header is %d bytes from the start of the file and no of sections is %d\n", (uint32_t)e_shoff, e_shnum);
#endif
    e_mod->s_hdr = PTR_ADD(e_mod->f_buf , e_shoff);

#if DEBUG
    cprintf("section header start @ 0x%08x and file start @ 0x%08x\n", e_mod->s_hdr, e_mod->f_buf);
#endif
    s_shname = &e_mod->s_hdr[e_shstrndx];
    e_mod->shname_str_table = PTR_ADD(e_mod->f_buf, s_shname->sh_offset);

#if DEBUG
    cprintf("Section string table @ index : %d\n", (int)e_shstrndx);
    int i;
    for (i = 0; i < e_shnum; i++) {
        Elf64_Shdr *shdr;
        shdr = &e_mod->s_hdr[i];
        cprintf("section name is %s and size is %x\n", e_mod->shname_str_table + shdr->sh_name, shdr->sh_size );
    }
#endif
    return 0;
}

int populate_sym_tab(struct elf_module *e_mod)
{
    int n_sections = e_mod->e_hdr->e_shnum, i;
    Elf64_Shdr *s_hdr = e_mod->s_hdr;

    for (i = 0; i < n_sections; i++, s_hdr ++) {
        if (s_hdr->sh_type == SHT_STRTAB) {
            if (i != e_mod->e_hdr->e_shstrndx) {
                e_mod->strtab_index = i;
                e_mod->str_table = PTR_ADD(e_mod->f_buf, s_hdr->sh_offset);
            }
        }

        if (s_hdr->sh_type == SHT_SYMTAB) {
            e_mod->symtab_index = i;
            e_mod->sym_tab = PTR_ADD(e_mod->f_buf, s_hdr->sh_offset);
        }

    }
#if DEBUG
    cprintf("symtab index @ %d and strtab index @ %d\n", e_mod->symtab_index, e_mod->strtab_index);
#endif
    return 0;
}

int64_t get_symval(Elf64_Rela * rela, Elf64_Sym *symtab, struct elf_module *e_mod, Elf64_Shdr *to_rel)
{
#if DEBUG
    cprintf("symbol table index is %d\n", ELF64_R_SYM(rela->r_info));
#endif
    Elf64_Sym *r_sym = &symtab[ELF64_R_SYM(rela->r_info)];
    int64_t sym_val;
    Elf64_Shdr *sym_sec;
    char *strtab = e_mod->str_table;
    Elf64_Shdr *rr_hdr;
    uint64_t load_addr = 0;

    if (r_sym->st_shndx >=0 && r_sym->st_shndx < e_mod->e_hdr->e_shstrndx)  {

        rr_hdr = &e_mod->s_hdr[r_sym->st_shndx];

        if (strcmp(".data", PTR_ADD(e_mod->shname_str_table, rr_hdr->sh_name)) == 0) {
            load_addr = e_mod->data_load_addr;
        }
        else if (strcmp(".rodata", PTR_ADD(e_mod->shname_str_table, rr_hdr->sh_name)) == 0) {
            load_addr = e_mod->rodata_load_addr;
        }
        else if (strcmp(".text", PTR_ADD(e_mod->shname_str_table, rr_hdr->sh_name)) == 0) {
            load_addr = e_mod->text_load_addr;
        }
        else if (strcmp(".bss", PTR_ADD(e_mod->shname_str_table, rr_hdr->sh_name)) == 0) {
            load_addr = e_mod->bss_load_addr;
        }
    }
switch (r_sym->st_shndx) {

        case SHN_UNDEF:
#if DEBUG
            cprintf("got an undefined symbol %s\n", PTR_ADD(e_mod->str_table, r_sym->st_name));
#endif
            sym_val = get_symbol_val_from_hashtable(PTR_ADD(strtab, r_sym->st_name));
            if (sym_val == 0) {
                cprintf("Unable to find symbol in kernel symbol table\n");
                return 0;
            }
            sym_val += rela->r_addend;
#if DEBUG
            cprintf("symname is %s and symval is 0x%x\n", PTR_ADD(strtab, r_sym->st_name), sym_val);
#endif
            break;
        case SHN_COMMON:
            cprintf("SHN_COMMON symbol found...remove common symbols\n");
            sym_val = 0;
            break;
        case SHN_ABS:
#if DEBUG
            cprintf("abs symbol's value is %x\n", r_sym->st_value);
#endif
            sym_val = r_sym->st_value;
            break;
        default:
            // do some sanity check
            if (r_sym->st_shndx > e_mod->e_hdr->e_shnum) {
                cprintf("Mishandling some special symbols or section indx out of range : %x\n", r_sym->st_shndx);
                return 0;
            }
            // get the value of the symbol
            sym_sec = &e_mod->s_hdr[r_sym->st_shndx];
            void *sym_addr = PTR_ADD(e_mod->f_buf, sym_sec->sh_offset);
            sym_val =  load_addr + r_sym->st_value + rela->r_addend;
            cprintf(" load_addr = %x symbol name is %s symbol value is %x and st_value = %d\n",load_addr,  PTR_ADD(strtab, r_sym->st_name), sym_val, r_sym->st_value);
            break;
    }

    return sym_val;
}

uint64_t get_paddr(struct elf_module *e_mod, Elf64_Shdr *shdr)
{
    //return the paddr

    char *strtab = e_mod->shname_str_table;
    char *secname = strtab + shdr->sh_name;

    if (strcmp(secname, ".data") == 0 && e_mod->data_load_addr != 0) {
        return e_mod->data_load_addr;
    }
    if (strcmp(secname, ".text") == 0 && e_mod->text_load_addr != 0) {
        return e_mod->text_load_addr;
    }
    if (strcmp(secname, ".rodata") == 0 && e_mod->rodata_load_addr != 0) {
        return e_mod->rodata_load_addr;
    }
    if (strcmp(secname,".bss") == 0 && e_mod->bss_load_addr != 0) {
        return e_mod->bss_load_addr;
    }

    return 0;
}

void fix_rela_relocation(struct elf_module *e_mod, int rel_index)
{
    Elf64_Shdr *rel = &e_mod->s_hdr[rel_index];
    Elf64_Shdr *sym, *roff_sec;
    Elf64_Sym *symtab;
    Elf64_Shdr *to_rel_sec;
    Elf64_Rela *rela = PTR_ADD(e_mod->f_buf, rel->sh_offset);
    size_t rel_size = rel->sh_size / sizeof(*rela);
    uint64_t i, symval;
    uint64_t to_rel_sec_size ;
    void *rsec;

    roff_sec = &e_mod->s_hdr[rel->sh_info];
    rsec = PTR_ADD(e_mod->f_buf, roff_sec->sh_offset);
    // do some sanity checks just to make sure that we are not misunderstanding
    // anything

    if (rel->sh_link != e_mod->symtab_index) {
        cprintf("Bleh!! symtab\n");
        return;
    }
    sym = &e_mod->s_hdr[rel->sh_link];
    symtab = PTR_ADD(e_mod->f_buf, sym->sh_offset);
    // the section to which relocation applies
    to_rel_sec = &e_mod->s_hdr[rel->sh_info];
    to_rel_sec_size = to_rel_sec->sh_size;
#if DEBUG
    cprintf("There are %d entries in rela section\n", rel_size);
#endif
    Elf64_Rela *ptr = rela;
    for (i = 0; i < rel_size; i++, ptr ++) {
        uint64_t paddr;
        switch(ELF64_R_TYPE(ptr->r_info)) {
            case R_X86_64_PC32:
#if DEBUG
                cprintf("got a R_X86_64_PC32 relocation\n");
#endif
                // get the symval and handle the relocation appropriately
                symval = get_symval( ptr, symtab, e_mod, to_rel_sec);
                if (symval == 0) {
                    cprintf("error in relocation\n");
                    return;
                }
                paddr = get_paddr(e_mod, roff_sec );

                if (paddr == 0 ) {
                    // this should never happen..ever
                    cprintf("Blah!!!\n");
                    return;
                }
                paddr += ptr->r_offset;
                //symval will contain S + A, need P
                // apply relocation @ roff_sec + rsym->r_offset
                cprintf("++++++++++++++\n");
                cprintf("(S+A) = %x, P = %x and roff = %x\n", symval, paddr,  ptr->r_offset);
                cprintf("++++++++++++++\n");

                *((uint64_t *) PTR_ADD(rsec, ptr->r_offset)) = symval - paddr;
                break;
            case R_X86_64_64:
#if DEBUG
                cprintf("got a R_X86_64_64 relocation\n");
#endif
                symval = get_symval( ptr, symtab, e_mod, to_rel_sec);
                if (symval == 0) {
                    cprintf("error in relocation!!\n");
                    return;
                }
                // symval will contain S + A, no need P
                cprintf("++++++++++++++\n");
                cprintf("(S+A) = %x, and roff = %x\n", symval, ptr->r_offset);
                cprintf("++++++++++++++\n");
                *((uint64_t *) PTR_ADD(rsec, ptr->r_offset)) = symval;
                break;
            default:
                cprintf("%x type symbol has no support (yet)\n", ELF64_R_TYPE(ptr->r_info));
                break;
        }
    }
}
void map_into_memory(struct elf_module *e_mod)
{
    Elf64_Shdr *shdr = e_mod->s_hdr, *ptr;
    int shnum = e_mod->e_hdr->e_shnum,  i,ret;
    uint64_t paddr , num_pages, j, va;
    void *secdata;
    struct PageInfo *pp;
    char *strtab = e_mod->shname_str_table;

    ptr = shdr;
    for (i = 0; i < shnum; i++, ptr++) {
        if ( (ptr->sh_flags & SHF_ALLOC) && ptr->sh_size != 0 && strcmp(".eh_frame", PTR_ADD(strtab, ptr->sh_name)) != 0 ) {
            paddr = get_paddr(e_mod, ptr);
            if (paddr == 0) {
                cprintf("paddr 0, lkm failed \n");
                return;
            }
            num_pages = ALIGN(ptr->sh_size, PAGE_SIZE) / PAGE_SIZE;
            va = paddr;
            cprintf("paddr = %x and va = %x\n", paddr, va);
            for (j = 0; j < num_pages; j++, va += PAGE_SIZE) {
                pp = page_alloc(ALLOC_ZERO);
                if (pp == NULL) {
                    cprintf("No memory...\n");
                    return;
                }

                ret = page_insert(curenv->env_pml4e, pp, (void *)va, PTE_P | PTE_W  );
                if (ret < 0) {
                    cprintf("ERror in inserting\n");
                    return ;
                }

            }
            secdata = PTR_ADD(e_mod->f_buf, ptr->sh_offset);
            memcpy((void *)paddr, secdata, ptr->sh_size);
        }

    }

}
void init_module(void *buf, void *k_buf, size_t size, size_t k_size,  char *filename)
{
    struct elf_module *e_ptr;
    int i, r;
    static int is_loaded = 0;
    int32_t sz = sizeof(lkm_bitmap) / sizeof(lkm_bitmap[0]);


    bitmap_init(lkm_bitmap, sz);
    if (is_loaded == 0) {
        r = init_kern_hash_table(k_buf, k_size);
        if (r < 0) {
            cprintf("error in init_kern_hash_table\n");
            return ;
        }
        is_loaded = 1;
    }
    e_ptr = e_mod;
    if (no_modules == MAX_LOAD_MOD) {
        cprintf("exceeded max number of loadable modules\n");
        return;
    }
    for (i = 0; i < MAX_LOAD_MOD; i ++, e_ptr ++) {
        if (e_ptr->is_free == 0) {
            e_ptr->is_free = 1;
            no_modules ++;
            break;
        }
    }

    if (e_ptr->is_free != 1) {
        // we should never get here, just sanity check this
        cprintf("Bleh!!\n");
        return;
    }

    // at this point our e_ptr is valid..
    strncpy(e_ptr->name, filename, sizeof(e_ptr->name) - 1);
    e_ptr->f_buf = buf;
    e_ptr->f_size = size;
    e_ptr->e_hdr = (Elf64_Ehdr *)buf;

    // do the actual work from here
    if ( (r = populate_section_hdrs(e_ptr)) < 0 ) {
        cprintf("error in populating section headers\n");
        return;
    }


    // get the load addr from the bitmap

    if ( (r = populate_sym_tab(e_ptr)) < 0) {
        cprintf("error in populating symbol table\n");
        return;
    }
    // do the relocation here
    Elf64_Shdr *s_hdr = e_ptr->s_hdr;
    int32_t ssize, n_bits, bit_pos, l;
    int32_t *ptr = lkm_bitmap;
    for (i = 0; i < e_ptr->e_hdr->e_shnum; i++, s_hdr ++) {
        if ((s_hdr->sh_flags & SHF_ALLOC) && strncmp(".eh_frame", PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), strlen(".eh_frame")) != 0) {
#if DEBUG
            cprintf("SECTION name is %s\n", PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name));
#endif
            if (strncmp(".data",PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), strlen(".data")) == 0) {

                ssize = s_hdr->sh_size;
                ssize = ALIGN(ssize, PAGE_SIZE);
                if (ssize == 0)
                    continue;
#if DEBUG
                cprintf("section name is %s and align size is %d\n", PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), ssize);
#endif
                n_bits = ssize / PAGE_SIZE;
                bit_pos = bitmap_find_clear_pos_cont(lkm_bitmap,sz, n_bits);
                e_ptr->data_load_addr = LKM_LOAD_ADDR + (bit_pos * PAGE_SIZE);
                bitmap_set_cont(lkm_bitmap, sz, bit_pos, n_bits);
            }
            else if (strncmp(".text",PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), strlen(".text")) == 0) {
                ssize = s_hdr->sh_size;
                ssize = ALIGN(ssize, PAGE_SIZE);
                if (ssize == 0)
                    continue;
#if DEBUG
                cprintf("section name is %s and align size is %d\n", PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), ssize);
#endif
                n_bits = ssize / PAGE_SIZE;
                bit_pos = bitmap_find_clear_pos_cont(lkm_bitmap,sz, n_bits);
                e_ptr->text_load_addr = LKM_LOAD_ADDR + (bit_pos * PAGE_SIZE);
                bitmap_set_cont(lkm_bitmap, sz, bit_pos, n_bits);
            }
            else if (strncmp(".rodata",PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), strlen(".rodata")) == 0) {
                ssize = s_hdr->sh_size;
                ssize = ALIGN(size, PAGE_SIZE);
                if (ssize == 0)
                    continue;
#if DEBUG
                cprintf("section name is %s and align size is %d\n", PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), ssize);
#endif
                n_bits = ssize / PAGE_SIZE;
                bit_pos = bitmap_find_clear_pos_cont(lkm_bitmap,sz, n_bits);
                e_ptr->rodata_load_addr = LKM_LOAD_ADDR + (bit_pos * PAGE_SIZE);
                bitmap_set_cont(lkm_bitmap, sz, bit_pos, n_bits);

            }
            else if (strncmp(".bss",PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), strlen(".bss")) == 0) {
                ssize = s_hdr->sh_size;
                ssize = ALIGN(ssize, PAGE_SIZE);
                if (ssize == 0)
                    continue;
#if DEBUG
                cprintf("section name is %s and align size is %d\n", PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), ssize);
#endif
                n_bits = ssize / PAGE_SIZE;
                bit_pos = bitmap_find_clear_pos_cont(lkm_bitmap,sz, n_bits);
                e_ptr->bss_load_addr = LKM_LOAD_ADDR + (bit_pos * PAGE_SIZE);
                bitmap_set_cont(lkm_bitmap, sz, bit_pos, n_bits);

            }

            cprintf("textaddr = %x data addr = %x rodata addr = %x and bss addr = %x\n", e_ptr->text_load_addr, e_ptr->data_load_addr,
                    e_ptr->rodata_load_addr, e_ptr->bss_load_addr);

        }
    }
    s_hdr = e_ptr->s_hdr;

    for (i = 0; i < e_ptr->e_hdr->e_shnum; i++, s_hdr ++) {
        if (s_hdr->sh_type == SHT_RELA && (strcmp(PTR_ADD(e_ptr->shname_str_table, s_hdr->sh_name), ".rela.eh_frame") != 0)) {
#if DEBUG
            cprintf("RELA section @ %d\n", i);
#endif
            fix_rela_relocation(e_ptr, i);
        }
        if (s_hdr->sh_type == SHT_REL) {
#if DEBUG
            cprintf("REL section @ %d\n", i);
#endif

        }
    }

    e_ptr->init_fn = (void (*) (void)) e_ptr->text_load_addr;

    map_into_memory(e_ptr);

    e_ptr->init_fn();

    return ;
}

