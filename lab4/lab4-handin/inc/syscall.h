#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

/* system call numbers */
enum {
	SYS_cputs = 0,
	SYS_cgetc,
	SYS_getenvid,//2
	SYS_env_destroy,

	SYS_map_kernel_page,//4

	SYS_page_alloc,//5
	SYS_page_map,
	SYS_page_unmap,//7
	SYS_exofork,
	SYS_env_set_status,
	SYS_env_set_pgfault_upcall,//10
	SYS_yield,
	SYS_ipc_try_send,//12
	SYS_ipc_recv,

	SYS_sbrk,//14
	NSYSCALLS//15
};

#endif /* !JOS_INC_SYSCALL_H */
