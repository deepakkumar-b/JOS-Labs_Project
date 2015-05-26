// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	void *tmp = ROUNDDOWN(addr, PGSIZE);
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!(err & FEC_WR)){
		panic("Fault is not write");
	}else if(!(uvpt[VPN(addr)] & PTE_COW)){
		panic("Page is not Copy-on-Write");
	}
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	// LAB 4: Your code here.
	r = sys_page_alloc(0, (void *)PFTEMP, (PTE_P | PTE_U | PTE_W));
        if(r < 0)
                panic("error while allocating page in pgfault of fork.c");
        memmove((void *)PFTEMP, (const void *)tmp, PGSIZE);
        r = sys_page_map(0, (void *)PFTEMP, 0, tmp, (PTE_P | PTE_U | PTE_W));
        if(r < 0)
                panic("error while moving page in pgfault of fork.c");
	r = sys_page_unmap(0, (void *)PFTEMP);
	if(r)
		panic("error while unmapping page in pgfault of fork.c");
//	panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	pte_t pte = uvpt[pn];
	uintptr_t va = (uintptr_t)(pn << PGSHIFT);
	// LAB 4: Your code here.
	//lab 5 minesh only added (pte & PTE_SHARE if condition)
	if(pte & PTE_SHARE)
         {
                 r = sys_page_map(0, (void *)va, envid, (void *)va, (pte & PTE_SYSCALL)); 
                 if(r)
                        panic("panic in duppage");
                return 0;
         }
	if(!((pte & PTE_W) || (pte & PTE_COW))) 
	 { 
		 r = sys_page_map(0, (void *)va, envid, (void *)va, PGOFF(pte)); 
		 if(r) 
	 		panic("panic in duppage"); 
	 	return 0;
	 }  
	 r = sys_page_map(0, (void *)va, envid, (void *)va, (PTE_P | PTE_U | PTE_COW));
	 if(r)
		panic("panic in duppage");
	 r = sys_page_map(0, (void *)va, 0, (void *)va, (PTE_P | PTE_U | PTE_COW));
	 if(r) 
		panic("panic in duppage");	

//	panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
	int r, pgno2, pgno = PPN(UTEXT);
	uint64_t i;

	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if(envid < 0)
	{
		panic("panic in fork for sys_exofork");
		return -1;
	}else if(envid == 0)
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	
	while(pgno < PPN(UTOP)){
		if(!((uvpml4e[VPML4E(pgno)] & PTE_P) && (uvpde[pgno >> 18] & PTE_P) && (uvpd[pgno >> 9] & PTE_P))){
			pgno = pgno + 512;
		}else{
			for(pgno2 = pgno + 512; pgno < pgno2; pgno++){
				if(!((uvpt[pgno] & PTE_P) != PTE_P) && !(pgno == PPN(UXSTACKTOP - 1))) 
                           		duppage(envid, pgno); 
			}
		}
	}

	r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), (PTE_P | PTE_U | PTE_W)); 
	if(r)
	{
	 	panic("panic in fork. Cannot allocate page at UXSTACKTOP");
	 	return r;
	}
	
	r = sys_page_alloc(envid, (void*)(USTACKTOP - PGSIZE), (PTE_P | PTE_U | PTE_W));
	if(r < 0)  
		panic("panic in fork. Cannot allocate page at USTACKTOP");
	
	r = sys_page_map(envid, (void*)(USTACKTOP - PGSIZE), 0, UTEMP, (PTE_P | PTE_U | PTE_W));
	if(r < 0) 
		panic("panic in fork. Cannot map page at USTACKTOP");

	memmove(UTEMP, (void*)(USTACKTOP - PGSIZE), PGSIZE);
	r = sys_page_unmap(0, UTEMP);
	if(r < 0) 
		panic("panic in fork. Cannot unmap page from UTEMP");

        r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall); 
	if(r)
	{ 
		panic("panic in fork. Cannot set pgfault upcall"); 
	  	return r;  
        } 
	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if(r)
        { 
	 	panic("panic in fork. Cannot set childs status as runnable");
		return r;
	}
	return envid;

//	panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
