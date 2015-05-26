
#ifndef JOS_KERN_MODULE_H
#define JOS_KERN_MODULE_H

#define MAX_LKM 10	/* Maximum Number of LKM's Possible */
#include <kern/pci.h>
#include <inc/elf.h>
struct LKM_Module
{
	char name[32];
	uint64_t *load_addr;
	int load_status;
//	struct LKM_Module *next;
	struct Elf *elf_hdr;
	struct Secthdr *sect_hdr;
	struct Symhdr *sym_hdr;
	char *shname_st_table;
	char *st_table;
	int sym_tab_index;
	int st_tab_index;
	int sym_cnt;
	void *tmp_buf;	
	void *data_addr;
	void *text_addr;
	void *rodata_addr;
	void *bss_addr;
	int num_pages;
	void (*init_mod) (void);
	void (*unload_mod) (void);
//	char *depd_mod[MAX_LKM];
	int depd_count;
	int depd_mod[MAX_LKM];
};

struct LKM_Module *modules_lkm;
void initialize_lkm(void);
int check_module(char *mod);
int load_module(char *buf, char *file);
int calculate_sect_hdr(struct LKM_Module *lkm_mod);
int calculate_sym_hdr(struct LKM_Module *lkm_mod, int e_shnum);
int allocate_module(struct LKM_Module *lkm_mod, int e_shnum);
int apply_relocation(struct LKM_Module *lkm_mod, int sect_num);
uint64_t find_symbol(struct LKM_Module *lkm_mod, struct Secthdr *secthdr, struct Symhdr *symhdr, struct Elf64_Rela *rela);
int  calculate_load_addr(struct LKM_Module *lkm_mod, int e_shnum);
int load_final_module(struct LKM_Module *lkm_mod, int e_shnum);
int prepare_mod_for_load(struct LKM_Module *lkm, int sym_index, int str_index);
void flush_module(struct LKM_Module *lkm_mod, int num_mod);
void calculate_dependencies(struct LKM_Module *lkm_mod);
void print_dependencies(struct LKM_Module *lkm_mod);
void print_mod_dep(char *file);
void print_loaded_modules(void);
void remove_mod(char *file);
void calculate_reverse_dependency(struct LKM_Module *lkm_mod, int mod_add_num);
void remove_reverse_dependency(struct LKM_Module *lkm_mod, int mod_rm_num);
#endif // JOS_KERN_MODULE_H

