#ifndef JOS_INC_ELF_H
#define JOS_INC_ELF_H
#include <inc/types.h>		//minesh
#define ELF_MAGIC 0x464C457FU	/* "\x7FELF" in little endian */
//Minesh from here
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
#define	EI_NIDENT	16

#define	EI_MAG0		0	/* e_ident[] indexes */
#define	EI_MAG1		1
#define	EI_MAG2		2
#define	EI_MAG3		3

#define	ELFMAG0		0x7f		/* EI_MAG */
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'
#define	ELFMAG		"\177ELF"

#define	ET_NONE		0		/* e_type */
#define	ET_REL		1
#define	ET_EXEC		2
#define	ET_DYN		3
// machine
#define	EM_AMD64	62		/* AMDs x86-64 architecture */
#define	EM_X86_64	EM_AMD64
//minesh Ends Here
struct Elf {
	uint32_t e_magic;	// must equal ELF_MAGIC
	uint8_t e_elf[12];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct Proghdr {
	uint32_t p_type;
    uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_va;
	uint64_t p_pa;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

struct Secthdr {
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
};


// Values for Proghdr::p_type
#define ELF_PROG_LOAD		1

// Flag bits for Proghdr::p_flags
#define ELF_PROG_FLAG_EXEC	1
#define ELF_PROG_FLAG_WRITE	2
#define ELF_PROG_FLAG_READ	4

// Values for Secthdr::sh_type
#define ELF_SHT_NULL		0
#define ELF_SHT_PROGBITS	1
#define ELF_SHT_SYMTAB		2
#define ELF_SHT_STRTAB		3

// Values for Secthdr::sh_name
#define ELF_SHN_UNDEF		0


//Minesh from here
#define	ELF_SHT_RELA		4
#define	ELF_SHT_HASH		5
#define	ELF_SHT_DYNAMIC		6
#define	ELF_SHT_NOTE		7
#define	ELF_SHT_NOBITS		8
#define	ELF_SHT_REL		9
#define	ELF_SHT_SHLIB		10
#define	ELF_SHT_DYNSYM		11
#define	ELF_SHT_UNKNOWN12	12
#define	ELF_SHT_UNKNOWN13	13
#define	ELF_SHT_INIT_ARRAY	14
#define	ELF_SHT_FINI_ARRAY	15
#define	ELF_SHT_PREINIT_ARRAY	16
#define	ELF_SHT_GROUP		17
#define	ELF_SHT_SYMTAB_SHNDX	18
#define	ELF_SHT_NUM		19

//Values for Secthdr::sh_flags
#define ELF_SHT_ALLOC		0x2
#define	ELF_SHN_UNDEF		0
#define	ELF_SHN_ABS		0xfff1
#define	ELF_SHN_COMMON		0xfff2

struct Symhdr
{
	Elf64_Word	st_name;
	unsigned char	st_info;	/* bind, type: ELF_64_ST_... */
	unsigned char	st_other;
	Elf64_Half	st_shndx;	/* SHN_... */
	Elf64_Addr	st_value;
	Elf64_Xword	st_size;
};

#define	STN_UNDEF	0
#define	ELF64_ST_BIND(info)		((info) >> 4)
#define	ELF64_ST_TYPE(info)		((info) & 0xf)
#define	ELF64_ST_INFO(bind, type)	(((bind)<<4)+((type)&0xf))


#define	STB_LOCAL	0		/* BIND */
#define	STB_GLOBAL	1
#define	STB_WEAK	2
#define	STB_NUM		3

#define	STB_LOPROC	13		/* processor specific range */
#define	STB_HIPROC	15

#define	STT_NOTYPE	0		/* TYPE */
#define	STT_OBJECT	1
#define	STT_FUNC	2
#define	STT_SECTION	3
#define	STT_FILE	4
#define	STT_COMMON	5
#define	STT_TLS		6
#define	STT_NUM		7


struct Elf64_Rel{
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
};

struct Elf64_Rela{
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
	Elf64_Sxword	r_addend;
};

#define	ELF64_R_SYM(info)	((info)>>32)
#define	ELF64_R_TYPE(info)    	((Elf64_Word)(info))
#define	ELF64_R_INFO(sym, type)	(((Elf64_Xword)(sym)<<32)+(Elf64_Xword)(type))
#define R_X86_64_64             1
#define R_X86_64_PC32           2 
//Minesh ends here

#endif /* !JOS_INC_ELF_H */
