#include <inc/lib.h>
#include <inc/elf.h>
#include <kern/ksym.h>

#define MAX_LKM_PAGES		256
#define UTEMP_K        0xEEDFD000      //10 Mb below normal stack
//#define TEMP_START_ADDR		0xE93FD000
void insmod(char *file, char *file_k)
{
	unsigned char elf_buf[512];
	unsigned char elf_buf_k[512];
	int fd,i,r,ret,j;		//related to file
	int fd_k,i_k,r_k,ret_k,j_k;	//related to kernel
	struct Stat stat, stat_k;
	struct Elf *elf, *elf_k;
	int perm = PTE_P | PTE_U | PTE_W;
	if((r = open(file, O_RDONLY)) < 0){
		cprintf("Unable to Fetch the module. \n",file);
		return;
	}
	fd=r;
	elf = (struct Elf*)elf_buf;
	if (readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
            || elf->e_magic != ELF_MAGIC) {
                close(fd);
                cprintf("File is InAppropriate. Elf magic is: %08x Required is: %08x\n", elf->e_magic, ELF_MAGIC);
                return;
        }
	j = fstat(r, &stat);
	cprintf("File size is: %d bytes \n",stat.st_size);
	if(stat.st_size > (1024*1024)){
		cprintf("File size exceeded the limit of 1 MB \n");
		return;
	}
	i=0;
        while(i < MAX_LKM_PAGES){
                ret = sys_page_alloc(0, (UTEMP + i*PGSIZE), perm);
                if(ret < 0){
                        cprintf("sys_page_alloc error in insmod \n");
                        return;
                }
                seek(r, i*PGSIZE);
                ret = readn(r, (UTEMP + i*PGSIZE), PGSIZE);
                if(ret < PGSIZE){
			if(ret < 0){
				cprintf("Erro while reading Module \n");
				return;
			}else{
				break;
			}
		}
		i++;
        }
	//Fetching kernel image file and passing it in buffer for syscall
	if((r_k = open(file_k, O_RDONLY)) < 0){
		cprintf("Unable to Fetch Kernel Info \n");
		return;
	}
	fd_k = r_k;
        elf_k = (struct Elf*)elf_buf_k;
        if (readn(fd_k, elf_buf_k, sizeof(elf_buf_k)) != sizeof(elf_buf_k)
            || elf_k->e_magic != ELF_MAGIC) {
                close(fd_k);
                cprintf("File is InAppropriate. Elf magic is: %08x Required is: %08x\n", elf_k->e_magic, ELF_MAGIC);
                return;
        }
//	cprintf("Elf magic is: %08x Required is: %08x\n", elf_k->e_magic, ELF_MAGIC);
	i_k=0;
	j_k = fstat(r_k, &stat_k);
	stat_k.st_size = ROUNDUP(stat_k.st_size, PGSIZE);
	int kn_pages = (int)(stat_k.st_size / PGSIZE);
	while(i_k < kn_pages){
		ret_k = sys_page_alloc(0, ((void *)UTEMP_K + i_k*PGSIZE), perm);
                if(ret_k < 0){
                        cprintf("sys_page_alloc error in insmod for kernel \n");
                        return;
                }
                seek(r_k, i_k*PGSIZE);
                ret_k = readn(r_k, ((void *)UTEMP_K + i_k*PGSIZE), PGSIZE);
/*                if(ret_k < PGSIZE){
                        if(ret < 0){
                                cprintf("Erro while reading Module \n");
                                return;
                        }else{
                                break;
                        }
                }
*/                i_k++;
	}
	sys_insmod(UTEMP, (char *)UTEMP_K, file);
}

void umain(int argc, char **argv)
{
	binaryname = "insmod";
	if(argc > 1){
		insmod(argv[1], "kernel");
	}else{
		cprintf("Module name missing \n");
	}
}
