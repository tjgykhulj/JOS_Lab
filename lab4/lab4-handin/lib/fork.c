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

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0)
		panic("pgfault: expect a write but not");
	if ((vpd[PDX(addr)] & PTE_P) == 0)
		panic("pgfault: expect page directory but not exists.");
	if ((vpt[PGNUM(addr)] & PTE_COW) == 0)
		panic("pgfault: expect copy-on-write but not.");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	if ((r = sys_page_alloc(0, (void*)PFTEMP, PTE_U|PTE_W|PTE_P)) < 0)
		panic("pgfault -> sys_page_alloc error : %e\n", r);
	void* va = (void*)ROUNDDOWN(addr, PGSIZE);
	memmove((void*)PFTEMP, va, PGSIZE);
	if ((r = sys_page_map(0, (void*)PFTEMP, 0, va, PTE_U|PTE_W|PTE_P)) < 0)
		panic("pgfault -> sys_page_map error : %e", r);
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
	// LAB 4: Your code here.
	void *addr = (void *) (pn * PGSIZE);
	if (vpt[PGNUM(addr)] & (PTE_W | PTE_COW))
	{
		if ((r = sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P)) < 0 ||
		    (r = sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P)) < 0)
			goto bad;
	} else
		if ((r = sys_page_map(0, addr, envid, addr, PTE_U|PTE_P)) < 0)
			goto bad;
	return 0;
bad:
	panic("duppage -> sys_page_map error : %e\n", r);
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
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
//1
	set_pgfault_handler(pgfault);
//2
	envid_t child_pid = sys_exofork();
	if (child_pid < 0) panic("fork -> sys_exofork() error : %e\n", child_pid);
	if (child_pid == 0) {
		thisenv = envs + ENVX(sys_getenvid());
		return 0;
	}
//3
	int i;
	for (i = UTEXT/PGSIZE; i != UTOP/PGSIZE; i++)
		if (i != (UXSTACKTOP-PGSIZE)/PGSIZE &&
		    (vpd[i/NPTENTRIES] & PTE_P) &&
		    (vpt[i] & PTE_P) &&
		    (vpt[i] & PTE_U)) duppage(child_pid, i);

	int r = sys_page_alloc(child_pid, (void *) UXSTACKTOP - PGSIZE,
					PTE_U | PTE_W | PTE_P);
	if (r < 0) panic("fork -> sys_page_alloc() error : %e\n", r);
//4
	extern void _pgfault_upcall(void);
	if ((r = sys_env_set_pgfault_upcall(child_pid, (void *)_pgfault_upcall)) < 0)
		panic("fork -> sys_env_set_pgfault_upcall() error : %e\n", r);
//5	
	if ((r = sys_env_set_status(child_pid, ENV_RUNNABLE)) < 0)
		panic("fork -> sys_env_set_status() error : %e\n", r);
	
	return child_pid;	   
}

static int
sdup(envid_t envid, unsigned pn, int need_cow)
{
	int r;
	// LAB 4: Your code here.
	void *addr = (void *) (pn * PGSIZE);
	pte_t pte = vpt[PGNUM(addr)];
	uint32_t flags = PTE_U | PTE_P;
	if (need_cow || (pte & PTE_COW))
	{
		if ((r = sys_page_map(0, addr, envid, addr, PTE_COW | flags)) < 0 ||
		    (r = sys_page_map(0, addr, 0, addr, PTE_COW | flags)) < 0)
			goto bad;
	} else
		if ((r = sys_page_map(0, addr, envid, addr, flags | (pte & PTE_W))) < 0)
			goto bad;
	return 0;
bad:
	panic("sduppage -> sys_page_map error : %e\n", r);
}

// Challenge!
int
sfork(void)
{
	set_pgfault_handler(pgfault);
	envid_t child_pid = sys_exofork();
	if (child_pid < 0) panic("fork -> sys_exofork() error : %e\n", child_pid);
	if (child_pid == 0) {
		thisenv = envs + ENVX(sys_getenvid());
		return 0;
	}
/////////sth changes here/////////////////////
	uint32_t i, stack = 1;
	for (i = USTACKTOP - PGSIZE; i >= UTEXT; i -= PGSIZE)
		if ((vpd[PDX(i)] & PTE_P) &&
		    (vpt[PGNUM(i)] & PTE_P) &&
		    (vpt[PGNUM(i)] & PTE_U)) sdup(child_pid, PGNUM(i), stack);
		else
			stack = 0;
/////////////////////////////////////////////
	int r = sys_page_alloc(child_pid, (void *) UXSTACKTOP - PGSIZE,
					PTE_U | PTE_W | PTE_P);
	if (r < 0) panic("fork -> sys_page_alloc() error : %e\n", r);
	extern void _pgfault_upcall(void);
	if ((r = sys_env_set_pgfault_upcall(child_pid, (void *)_pgfault_upcall)) < 0)
		panic("fork -> sys_env_set_pgfault_upcall() error : %e\n", r);
	if ((r = sys_env_set_status(child_pid, ENV_RUNNABLE)) < 0)
		panic("fork -> sys_env_set_status() error : %e\n", r);
	return child_pid;	   
}
