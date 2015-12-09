// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
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
	{ "backtrace", "Test BackTrace", mon_backtrace },
	{ "time", "Test running time", mon_time },
	{ "c", "Debug : Continue", mon_c },
	{ "si", "Debug : Next Ins", mon_si },
	{ "x", "Debug : Print Mess", mon_x },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

uint64_t getCycle() 
{
	uint32_t old1, old2;
	__asm __volatile("rdtsc\nmov %%eax, %0\nmov %%edx, %1" 
		:"=r"(old1),"=d"(old2));
	uint64_t time = old2;
	return (time << 32) + old1;
}

int
mon_time(int argc, char **argv, struct Trapframe *tf) 
{
	if (argc == 1) return argc;
	uint64_t old = getCycle();
	
	int i;
	for (i = 0; i < NCOMMANDS; i++) 
		if (strcmp(argv[1], commands[i].name) == 0)
			commands[i].func(argc, argv, tf);

	uint64_t new = getCycle();
	cprintf("%s cycles: %llu\n", argv[1], new - old);
	return 0;
}

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
	extern char entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-entry+1023)/1024);
	return 0;
}


int
mon_c(int argc, char **argv, struct Trapframe *tf)
{
	if (!tf)
		cprintf("Debug error\n");
	else
		tf->tf_eflags &= (~FL_TF);
	return -1;
}

int
mon_si(int argc, char **argv, struct Trapframe *tf)
{
	if (!tf) 
		cprintf("Debug error\n");
	else {
		cprintf("tf_eip=%08x\n", tf->tf_eip);
		tf->tf_eflags |= FL_TF;
	}
	return -1;
}

int
mon_x(int argc, char **argv, struct Trapframe *tf)
{
	if (!tf) {
		cprintf("Debug error\n");
		return -1;
	}
	cprintf("%d\n", *((int *)strtol(argv[1], 0, 16)));
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
    cprintf("Stack backtrace:\n");
    int *bp;
    struct Eipdebuginfo info;
    for (bp=(int *)read_ebp(); bp; bp=(int *)(*bp)) 
    {
	cprintf("eip %08x ebp %08x args %08x %08x %08x %08x %08x\n", 
		bp[1], bp, bp[2], bp[3], bp[4], bp[5], bp[6]);
	debuginfo_eip(bp[1], &info);
	cprintf("  %s:%d: %s+%d\n",
		info.eip_file, info.eip_line, 
		info.eip_fn_name, info.eip_fn_namelen);
    }
    cprintf("Backtrace success\n");
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

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
