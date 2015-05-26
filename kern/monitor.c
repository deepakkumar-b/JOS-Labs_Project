// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/dwarf.h>
#include <kern/kdebug.h>
#include <kern/dwarf_api.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display Stack BackTrace info", mon_backtrace}
};

#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Stack bracktrace: \n");
	const char *file_name;
	const char *function_name;
	uint64_t rbp,rbp2;//deepak
	int i=1,linenumber=0;
	int function_arg=0;
	int number=1;
	uint64_t rip,offset1;
	uint64_t esp;
	uintptr_t rip2,offset2;
	int rdi_i;
	uint32_t rdx, rdi, rsi;
	struct Ripdebuginfo info;
        __asm __volatile("movq %%rbp,%0" : "=r" (rbp)::"cc","memory");
//	__asm __volatile("movq 0x8(%%rbp),%0" : "=r" (rip)::"cc","memory");
	__asm __volatile("leaq (%%rip), %0" : "=r" (rip)::"cc","memory");
//	__asm __volatile("movq -0x8(%%rip),%0" : "=r" (rbp)::"cc","memory");

	

	while(rbp!=0){
		rbp2=rbp;
		rip2=rip;
		debuginfo_rip(rip2, &info);
		file_name=info.rip_file;
		linenumber=info.rip_line;
		function_name=info.rip_fn_name;
		function_arg=info.rip_fn_narg;
		offset2=info.rip_fn_addr;
		offset1=rip-offset2;
		number = 1;
		while(number <= function_arg )//deepak
		{
//			cprintf("Number is:  %d\n",number); 
			switch(number){
			case 3:
				//mov rdx
//				cprintf("In 3\n");
	//			 __asm __volatile("mov -0x4(%%rsi),%0" : "=r" (rdx)::"cc","memory");
				
//				rdx = *(uint64_t *)rdx;
				//rdx = *(rdx);
				rdx =*(uint32_t *)(rbp - 0xb);
	//			cprintf("rdx %016x\n ",rdx);
				break;
			case 2:
				//mov rsi
//				cprintf("In 2\n");
//				 __asm __volatile("mov -0x4(%%rdi),%0" : "=r" (rsi)::"cc","memory");
//				rsi = *(uint64_t *)rsi;
				rsi =*(uint32_t *)(rbp - 0x8);
	//			cprintf("rsi %016x\n", rsi);
				break;
			case 1:
				//mov rdi
//				cprintf("In 1\n");
				// __asm __volatile("movq -0x4(%%rbp),%0" : "=r" (rdi)::"cc","memory");
				rdi =*(uint32_t *)(rbp - 0x4);
	//			rdi_i = *(uint64_t *)rdi;
	//			cprintf("rdi %016x\n", rdi);
				break;
			default:
				break;
			}
			number++;
		}
//		cprintf("rbp 000000%llx  rip 000000%llx\n      %s:%d: %s+%llx  args:%d  %11x %11x  %11x\n",rbp,rip,file_name,linenumber,function_name,offset1,function_arg, number, rdx, rsi, rdi);
		if(function_arg == 3)
		{
			cprintf("rbp %016x  rip %016x\n      %s:%d: %s+%016x  args:%d  %016x %016x %016x\n",rbp,rip,file_name,linenumber,function_name,offset1,function_arg, rdx, rsi, rdi);
		}
		if(function_arg == 2)
		{
			cprintf("rbp 000000%llx  rip 000000%llx\n      %s:%d: %s+%016x  args:%d  %016x %016x\n",rbp,rip,file_name,linenumber,function_name,offset1,function_arg, rsi, rdi);
		}
		if(function_arg == 1)
		{
			cprintf("rbp 000000%llx  rip 000000%llx\n      %s:%d: %s+%016x  args:%d  %016x\n",rbp,rip,file_name,linenumber,function_name,offset1,function_arg, rdi);
		}
		if(function_arg == 0)
		{
			cprintf("rbp 000000%llx  rip 000000%llx\n      %s:%d: %s+%016x  args:%d\n",rbp,rip,file_name,linenumber,function_name,offset1,function_arg);
		}
		
		esp=*(uint64_t *)rbp;
		rbp=esp;
		__asm __volatile("movq 0x8(%1),%0" : "=r" (esp) :"r"(rbp2));
		rip=esp;
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
