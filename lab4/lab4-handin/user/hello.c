// hello, world
#include <inc/lib.h>

int i;
void
umain(int argc, char **argv)
{
	i = 1;
	int x = sfork();
	if (x) {
		cprintf("parent : i = %d\n", i);
		sys_yield();
		sys_yield();
		sys_yield();
		sys_yield();
		sys_yield();
		sys_yield();
		sys_yield();
		sys_yield();
		cprintf("parent : i = %d\n", i);
		sys_env_destroy(x);
	} else {
		cprintf("child : i = %d\n", i);
		i++;
		i++;
	}
/*
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);*/
}
