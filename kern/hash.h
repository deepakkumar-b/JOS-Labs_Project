#include <inc/elf.h>
#define MAX_SYM		1987


struct ksym{
        char name[32];
	char tag_sym[32];
        uintptr_t addr;
};
unsigned sax_hash (void *key, int len );
int kern_symbol_hash(char *buf);
int insert_symbol(char *sym_name, uintptr_t addr, char *tag_sym);
uint64_t fetch_symbol_addr(char *sym_name);
char * fetch_symbol_tag(char *sym_name);
void print_hashed_symbols(void);
void invalidate_hash_mod(char *tag_sym);
