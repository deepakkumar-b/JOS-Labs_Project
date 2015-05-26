#include <kern/hash.h>
#include <inc/lib.h>
#include <inc/elf.h>
struct ksym k_sym[MAX_SYM];

int kern_symbol_hash(char *buf)
{
	int ret=0,i=0,sym_index,str_index;
	struct Elf *elf = (struct Elf *)buf;
	struct Secthdr *secthdr, *temphdr;
	size_t sym_size;
	struct Symhdr *symhdr, *tsymhdr;
	char *sttab;
	Elf64_Off e_shoff;
        Elf64_Half e_shnum;
        Elf64_Half e_shstrndx;
	if(elf->e_magic != ELF_MAGIC){
                cprintf("Error Occured while Loading the Kernel Symboles. ELF Header is:%08x, Required is: %08x\n", elf->e_magic, ELF_MAGIC);
                return -1;
        }
	e_shoff = elf->e_shoff;
	e_shnum = elf->e_shnum;
	e_shstrndx = elf->e_shstrndx;
	secthdr = (void *)(buf + e_shoff);
	temphdr = secthdr;
	while(i < e_shnum){
		if(temphdr->sh_type == ELF_SHT_SYMTAB){
			sym_index = i;
		}
		if((temphdr->sh_type == ELF_SHT_STRTAB) && (i != e_shstrndx)){
			str_index = i;
		}
		temphdr++;
		i++;
	}
	symhdr = (void *)(buf + secthdr[sym_index].sh_offset);
	tsymhdr = symhdr;
	sttab = (void *)(buf + secthdr[str_index].sh_offset);
	sym_size = secthdr[sym_index].sh_size / (sizeof(struct Symhdr));
	i=0;
	while(i < sym_size){
		if((ELF64_ST_BIND(tsymhdr->st_info) == STB_GLOBAL) && (ELF64_ST_TYPE(tsymhdr->st_info) == STT_FUNC)){
			insert_symbol((sttab + tsymhdr->st_name),tsymhdr->st_value, "kernel");
		}
		i++;
		tsymhdr++;
	}
	return 0;
}

unsigned sax_hash (void *key, int len )
{
        unsigned char *p = key;
        unsigned h = 0;
        int i;
 
        for ( i = 0; i < len; i++ )
        h ^= ( h << 5 ) + ( h >> 2 ) + p[i];
        return (h % MAX_SYM);
}

int insert_symbol(char *sym_name, uintptr_t addr, char *tag)
{
	struct ksym *k_sym_ptr;
	int next = 7;
	size_t len = strlen(sym_name);
	if(len > 32){
		sym_name[32-1] = '\0';
		len = strlen(sym_name);
	}
	unsigned int hash = sax_hash((void *)sym_name, len);
	k_sym_ptr = k_sym;
	while(true){
		if(k_sym_ptr->addr == 0x0){
			k_sym_ptr->addr = addr;
			strncpy(k_sym_ptr->name, sym_name, len);
			strncpy(k_sym_ptr->tag_sym , tag, strlen(tag));
			break;
		}else{
			k_sym_ptr = &k_sym[(hash + next) % MAX_SYM];
			next = (next + 7) % MAX_SYM;
		}
	}
	return 0;
}

uint64_t fetch_symbol_addr(char *sym_name)
{
	struct ksym *k_sym_ptr, *t_sym_ptr;
	int next = 7,i=0;
        size_t len = strlen(sym_name);
        if(len > 32){
                sym_name[32-1] = '\0';
                len = strlen(sym_name);
        }
        unsigned int hash = sax_hash((void *)sym_name, len);
        k_sym_ptr = k_sym;
	t_sym_ptr = &k_sym_ptr[hash];
	while(i < MAX_SYM){
		if(strncmp(sym_name, t_sym_ptr->name, len) == 0){
			return t_sym_ptr->addr;
		}else{
			t_sym_ptr = &k_sym_ptr[(hash + next) % MAX_SYM];
			next = (next + 7) % MAX_SYM;
		}
		i++;
	}
	return 0;
}

char *fetch_symbol_tag(char *sym_name)
{
	struct ksym *k_sym_ptr, *t_sym_ptr;
        int next = 7,i=0;
        size_t len = strlen(sym_name);
        if(len > 32){
                sym_name[32-1] = '\0';
                len = strlen(sym_name);
        }
        unsigned int hash = sax_hash((void *)sym_name, len);
        k_sym_ptr = k_sym;
        t_sym_ptr = &k_sym_ptr[hash];
        while(i < MAX_SYM){
                if(strncmp(sym_name, t_sym_ptr->name, len) == 0){
                        return t_sym_ptr->tag_sym;
                }else{
                        t_sym_ptr = &k_sym_ptr[(hash + next) % MAX_SYM];
                        next = (next + 7) % MAX_SYM;
                }
                i++;
        }
        return NULL;

}

void print_hashed_symbols(void)
{
	int i=0,ret=0;
        while(i < MAX_SYM){
                if(k_sym[i].addr != 0 && ((ret = strncmp(k_sym[i].tag_sym, "kernel", strlen("kernel")))!=0)){
                        cprintf("Symbol Name: %s , Symbol Addr: 0x%08x Tag: %s\n ", k_sym[i].name, k_sym[i].addr, k_sym[i].tag_sym);
                }
                i++;
        }
}

void invalidate_hash_mod(char *tag_sym)
{
//	cprintf("Incoming %s \n",tag_sym);
	int i=0,ret=0;
        while(i < MAX_SYM){
                if(k_sym[i].addr != 0 && ((ret = strncmp(k_sym[i].tag_sym, tag_sym, strlen(tag_sym)))==0)){
 //                       cprintf("Symbol Name: %s , Symbol Addr: 0x%08x Tag: %s\n ", k_sym[i].name, k_sym[i].addr, k_sym[i].tag_sym);
			k_sym[i].addr = 0x0;
			memset(k_sym[i].name,'\0',sizeof(k_sym[i].name));
			memset(k_sym[i].tag_sym,'\0',sizeof(k_sym[i].tag_sym));
                }
                i++;
        }
}
