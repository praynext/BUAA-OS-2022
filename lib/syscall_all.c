#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>
#include <sched.h>
#include <error.h>

extern char *KERNEL_SP;
extern struct Env *curenv;
struct variable {
    char name[64], values[64];
    int readonly, v, vis;
    LIST_ENTRY(variable) var_link;
};
LIST_HEAD(var_list, variable);
struct var_list total_vars[1024];
struct variable vars[64];
/* Overview:
 * 	This function is used to print a character on screen.
 *
 * Pre-Condition:
 * 	`c` is the character you want to print.
 */
void sys_putchar(int sysno, int c, int a2, int a3, int a4, int a5) {
    printcharc((char) c);
    return;
}

/* Overview:
 * 	This function enables you to copy content of `srcaddr` to `destaddr`.
 *
 * Pre-Condition:
 * 	`destaddr` and `srcaddr` can't be NULL. Also, the `srcaddr` area
 * 	shouldn't overlap the `destaddr`, otherwise the behavior of this
 * 	function is undefined.
 *
 * Post-Condition:
 * 	the content of `destaddr` area(from `destaddr` to `destaddr`+`len`) will
 * be same as that of `srcaddr` area.
 */
void *memcpy(void *destaddr, void const *srcaddr, u_int len) {
    char *dest = destaddr;
    char const *src = srcaddr;

    while (len-- > 0) {
        *dest++ = *src++;
    }

    return destaddr;
}

/* Overview:
 *	This function provides the environment id of current process.
 *
 * Post-Condition:
 * 	return the current environment id
 */
u_int sys_getenvid(void) {
    return curenv->env_id;
}

/* Overview:
 *	This function enables the current process to give up CPU.
 *
 * Post-Condition:
 * 	Deschedule current process. This function will never return.
 *
 * Note:
 *  For convenience, you can just give up the current time slice.
 */
/*** exercise 4.6 ***/
void sys_yield(void) {
    bcopy(KERNEL_SP - sizeof(struct Trapframe),
          TIMESTACK - sizeof(struct Trapframe),
          sizeof(struct Trapframe));
    sched_yield();
}

/* Overview:
 * 	This function is used to destroy the current environment.
 *
 * Pre-Condition:
 * 	The parameter `envid` must be the environment id of a
 * process, which is either a child of the caller of this function
 * or the caller itself.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 when error occurs.
 */
int sys_env_destroy(int sysno, u_int envid) {
    /*
        printf("[%08x] exiting gracefully\n", curenv->env_id);
        env_destroy(curenv);
    */
    int r;
    struct Env *e;

    if ((r = envid2env(envid, &e, 1)) < 0) {
        return r;
    }

    // printf("[%08d] destroying %08d\n", curenv->env_id, e->env_id);
    env_destroy(e);
    return 0;
}

/* Overview:
 * 	Set envid's pagefault handler entry point and exception stack.
 *
 * Pre-Condition:
 * 	xstacktop points one byte past exception stack.
 *
 * Post-Condition:
 * 	The envid's pagefault handler will be set to `func` and its
 * 	exception stack will be set to `xstacktop`.
 * 	Returns 0 on success, < 0 on error.
 */
/*** exercise 4.12 ***/
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop) {
    // Your code here.
    struct Env *env;
    int ret;

    if ((ret = envid2env(envid, &env, 0)) < 0) return ret;
    env->env_pgfault_handler = func;
    env->env_xstacktop = xstacktop;

    return 0;
    //	panic("sys_set_pgfault_handler not implemented");
}

/* Overview:
 * 	Allocate a page of memory and map it at 'va' with permission
 * 'perm' in the address space of 'envid'.
 *
 * 	If a page is already mapped at 'va', that page is unmapped as a
 * side-effect.
 *
 * Pre-Condition:
 * perm -- PTE_V is required,
 *         PTE_COW is not allowed(return -E_INVAL),
 *         other bits are optional.
 *
 * Post-Condition:
 * Return 0 on success, < 0 on error
 *	- va must be < UTOP
 *	- env may modify its own address space or the address space of its children
 */
/*** exercise 4.3 ***/
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm) {
    // Your code here.
    struct Env *env;
    struct Page *ppage;
    int ret;
    ret = 0;
    if ((perm & PTE_V) == 0) return -E_INVAL;
    if (perm & PTE_COW) return -E_INVAL;
    if (va >= UTOP) return -E_INVAL;
    ret = envid2env(envid, &env, 1);
    if (ret < 0) return ret;
    ret = page_alloc(&ppage);
    if (ret < 0) return ret;
    ret = page_insert(env->env_pgdir, ppage, va, perm);
    if (ret < 0) return ret;
    return 0;

}

/* Overview:
 * 	Map the page of memory at 'srcva' in srcid's address space
 * at 'dstva' in dstid's address space with permission 'perm'.
 * Perm must have PTE_V to be valid.
 * (Probably we should add a restriction that you can't go from
 * non-writable to writable?)
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Note:
 * 	Cannot access pages above UTOP.
 */
/*** exercise 4.4 ***/
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
                u_int perm) {
    int ret;
    u_int round_srcva, round_dstva;
    struct Env *srcenv;
    struct Env *dstenv;
    struct Page *ppage;
    Pte *ppte;

    ppage = NULL;
    ret = 0;
    round_srcva = ROUNDDOWN(srcva, BY2PG);
    round_dstva = ROUNDDOWN(dstva, BY2PG);

    //your code here
    if ((perm & PTE_V) == 0) return -E_INVAL;
    if (srcva >= UTOP || dstva >= UTOP) return -E_INVAL;
    if ((ret = envid2env(srcid, &srcenv, 0)) < 0) return ret;
    if ((ret = envid2env(dstid, &dstenv, 0)) < 0) return ret;
    ppage = page_lookup(srcenv->env_pgdir, round_srcva, &ppte);
    if (ppage == NULL) return -E_INVAL;
    /* add a restriction that you can't go from non-writable to writable */
    if (!(*ppte & PTE_R) && (perm & PTE_R)) return -E_INVAL;
    ret = page_insert(dstenv->env_pgdir, ppage, round_dstva, perm);

    return ret;
}

/* Overview:
 * 	Unmap the page of memory at 'va' in the address space of 'envid'
 * (if no page is mapped, the function silently succeeds)
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Cannot unmap pages above UTOP.
 */
/*** exercise 4.5 ***/
int sys_mem_unmap(int sysno, u_int envid, u_int va) {
    // Your code here.
    int ret;
    struct Env *env;
    if (va >= UTOP) return -E_INVAL;
    if ((ret = envid2env(envid, &env, 1)) < 0) return ret;
    page_remove(env->env_pgdir, va);

    return ret;
    //	panic("sys_mem_unmap not implemented");
}

/* Overview:
 * 	Allocate a new environment.
 *
 * Pre-Condition:
 * The new child is left as env_alloc created it, except that
 * status is set to ENV_NOT_RUNNABLE and the register set is copied
 * from the current environment.
 *
 * Post-Condition:
 * 	In the child, the register set is tweaked so sys_env_alloc returns 0.
 * 	Returns envid of new environment, or < 0 on error.
 */
/*** exercise 4.8 ***/
int sys_env_alloc(void) {
    // Your code here.
    int r;
    struct Env *e;

    if ((r = env_alloc(&e, curenv->env_id)) < 0) return r;
    bcopy(KERNEL_SP - sizeof(struct Trapframe),
          &(e->env_tf), sizeof(struct Trapframe));
    e->env_tf.pc = e->env_tf.cp0_epc;
    e->env_tf.regs[2] = 0; // $v0 <- return value
    e->env_pri = curenv->env_pri;
    e->env_status = ENV_NOT_RUNNABLE;

    return e->env_id;
    //	panic("sys_env_alloc not implemented");
}

/* Overview:
 * 	Set envid's env_status to status.
 *
 * Pre-Condition:
 * 	status should be one of `ENV_RUNNABLE`, `ENV_NOT_RUNNABLE` and
 * `ENV_FREE`. Otherwise return -E_INVAL.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if status is not a valid status for an environment.
 * 	The status of environment will be set to `status` on success.
 */
/*** exercise 4.14 ***/
int sys_set_env_status(int sysno, u_int envid, u_int status) {
    // Your code here.
    struct Env *env;
    int ret;

    if (status != ENV_NOT_RUNNABLE && status != ENV_RUNNABLE && status != ENV_FREE) return -E_INVAL;
    if ((ret = envid2env(envid, &env, 0)) < 0) return ret;
    if (status == ENV_RUNNABLE && env->env_status != ENV_RUNNABLE)
        LIST_INSERT_HEAD(&env_sched_list[0], env, env_sched_link);
    else if (status == ENV_NOT_RUNNABLE && env->env_status == ENV_RUNNABLE)
        LIST_REMOVE(env, env_sched_link); // ?

    env->env_status = status;

    return 0;
    //	panic("sys_env_set_status not implemented");
}

/* Overview:
 * 	Set envid's trap frame to tf.
 *
 * Pre-Condition:
 * 	`tf` should be valid.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if the environment cannot be manipulated.
 *
 * Note: This hasn't be used now?
 */
int sys_set_trapframe(int sysno, u_int envid, struct Trapframe *tf) {

    return 0;
}

/* Overview:
 * 	Kernel panic with message `msg`.
 *
 * Pre-Condition:
 * 	msg can't be NULL
 *
 * Post-Condition:
 * 	This function will make the whole system stop.
 */
void sys_panic(int sysno, char *msg) {
    // no page_fault_mode -- we are trying to panic!
    panic("%s", TRUP(msg));
}

/* Overview:
 * 	This function enables caller to receive message from
 * other process. To be more specific, it will flag
 * the current process so that other process could send
 * message to it.
 *
 * Pre-Condition:
 * 	`dstva` is valid (Note: NULL is also a valid value for `dstva`).
 *
 * Post-Condition:
 * 	This syscall will set the current process's status to
 * ENV_NOT_RUNNABLE, giving up cpu.
 */
/*** exercise 4.7 ***/
void sys_ipc_recv(int sysno, u_int dstva) {
    if (dstva >= UTOP) return;
    curenv->env_ipc_recving = 1;
    curenv->env_ipc_dstva = dstva;
    curenv->env_status = ENV_NOT_RUNNABLE;
    sys_yield();
}

/* Overview:
 * 	Try to send 'value' to the target env 'envid'.
 *
 * 	The send fails with a return value of -E_IPC_NOT_RECV if the
 * target has not requested IPC with sys_ipc_recv.
 * 	Otherwise, the send succeeds, and the target's ipc fields are
 * updated as follows:
 *    env_ipc_recving is set to 0 to block future sends
 *    env_ipc_from is set to the sending envid
 *    env_ipc_value is set to the 'value' parameter
 * 	The target environment is marked runnable again.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Hint: the only function you need to call is envid2env.
 */
/*** exercise 4.7 ***/
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
                     u_int perm) {

    int r;
    struct Env *e;
    struct Page *p;

    if (srcva >= UTOP) return -E_INVAL;
    if ((r = envid2env(envid, &e, 0)) < 0) return r;
    if (e->env_ipc_recving == 0) return -E_IPC_NOT_RECV;
    e->env_ipc_value = value;
    e->env_ipc_from = curenv->env_id;
    e->env_ipc_perm = perm;
    e->env_ipc_recving = 0;
    e->env_status = ENV_RUNNABLE;
    if (srcva != 0) {
        Pte *pte;
        p = page_lookup(curenv->env_pgdir, srcva, &pte);
        if (p == NULL) return -E_INVAL;
        if ((r = page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm)) < 0) return r;
    }

    return 0;
}
/* Overview:
 * 	This function is used to write data to device, which is
 * 	represented by its mapped physical address.
 *	Remember to check the validity of device address (see Hint below);
 *
 * Pre-Condition:
 *      'va' is the starting address of source data, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 *
 * Post-Condition:
 *      copy data from 'va' to 'dev' with length 'len'
 *      Return 0 on success.
 *	Return -E_INVAL on address error.
 *
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 *	 Physical device address:
 *	* ---------------------------------*
 *	|   device   | start addr | length |
 *	* -----------+------------+--------*
 *	|  console   | 0x10000000 | 0x20   |
 *	|    IDE     | 0x13000000 | 0x4200 |
 *	|    rtc     | 0x15000000 | 0x200  |
 *	* ---------------------------------*
 */
/*** exercise 5.1 ***/
int sys_write_dev(int sysno, u_int va, u_int dev, u_int len) {
    // Your code here
    if (va >= ULIM) return -E_INVAL;
    if ((dev >= 0x10000000 && dev + len <= 0x10000020)
        || (dev >= 0x13000000 && dev + len <= 0x13004200)
        || (dev >= 0x15000000 && dev + len <= 0x15000200)) {
        bcopy(va, dev + 0xa0000000, len);
        return 0;
    }
    return -E_INVAL;
}

/* Overview:
 * 	This function is used to read data from device, which is
 * 	represented by its mapped physical address.
 *	Remember to check the validity of device address (same as sys_write_dev)
 *
 * Pre-Condition:
 *      'va' is the starting address of data buffer, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 *
 * Post-Condition:
 *      copy data from 'dev' to 'va' with length 'len'
 *      Return 0 on success, < 0 on error
 *
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 */
/*** exercise 5.1 ***/
int sys_read_dev(int sysno, u_int va, u_int dev, u_int len) {
    // Your code here
    if (va >= ULIM) return -E_INVAL;
    if ((dev >= 0x10000000 && dev + len <= 0x10000020)
        || (dev >= 0x13000000 && dev + len <= 0x13004200)
        || (dev >= 0x15000000 && dev + len <= 0x15000200)) {
        bcopy(dev + 0xa0000000, va, len);
        return 0;
    }
    return -E_INVAL;
}

int get_shell_id(u_int envid) {
    struct Env *env = &(envs[ENVX(envid)]);
    while (env->env_is_shell != 1) env = &(envs[ENVX(env->env_parent_id)]);
    return env->env_id;
}

struct variable *get_var() {
    int i = 0;
    for (; i < 64; i++) {
        if (vars[i].v == 0) return vars + i;
    }
}

int create(const char *name, const char *value, const u_int vis, const u_int readonly, const int type) {
    struct var_list *temp_list = &(total_vars[ENVX(get_shell_id(curenv->env_id))]);
    // printf("env_id: %d name: %s value: %s vis %d readonly: %d\n", get_shell_id(curenv->env_id), name, value, vis, readonly);
    struct variable *tmp;
    LIST_FOREACH(tmp, temp_list, var_link) {
        if (strcmp(tmp->name, name) == 0 && tmp->readonly) {
            return -E_ENV_VAR_READONLY;
        } else if (strcmp(tmp->name, name) == 0) {
            strcpy(tmp->values, value);
            tmp->vis = vis;
            tmp->readonly = readonly;
            return 0;
        }
    }
    if (type) return -E_ENV_VAR_NOT_FOUND;
    tmp = get_var();
    strcpy(tmp->values, value);
    tmp->vis = vis;
    tmp->readonly = readonly;
    strcpy(tmp->name, name);
    tmp->v = 1;
    LIST_INSERT_HEAD(temp_list, tmp, var_link);
    // printf("env_id: %d name: %s value: %s vis %d readonly: %d\n", get_shell_id(curenv->env_id), name, value, vis, readonly);
    return 0;
}

int get(char *name, char *value) {
    struct var_list *temp_list = &(total_vars[ENVX(get_shell_id(curenv->env_id))]);
    struct variable *tmp;
    LIST_FOREACH(tmp, temp_list, var_link) {
        if (strcmp(tmp->name, name) == 0) {
            strcpy(value, tmp->values);
            return 0;
        }
    }
    return -E_ENV_VAR_NOT_FOUND;
}

int unset(char *name) {
    struct var_list *temp_list = &(total_vars[ENVX(get_shell_id(curenv->env_id))]);
    struct variable *tmp;
    if (strcmp(name, "curpath") == 0) {
        printf("curpath can't unset!\n");
        return 0;
    }
    LIST_FOREACH(tmp, temp_list, var_link) {
        if (strcmp(tmp->name, name) == 0 && tmp->readonly == 0) {
            LIST_REMOVE(tmp, var_link);
            tmp->v = 0;
            return 0;
        } else if (strcmp(tmp->name, name) == 0) {
            return -E_ENV_VAR_READONLY;
        }
    }
    return -E_ENV_VAR_NOT_FOUND;
}

int get_list(char *name, char *value) {
    struct var_list *temp_list = &(total_vars[ENVX(get_shell_id(curenv->env_id))]);
    struct variable *tmp;
    int pos = 0;
    char **name_list = (char **) name, **value_list = (char **) value;
    LIST_FOREACH(tmp, temp_list, var_link) {
        name_list[pos] = tmp->name;
        value_list[pos++] = tmp->values;
    }
    name_list[pos] = 0;
    return 0;
}

void sys_env_set_shell(int sysno, u_int is_shell) {
    curenv->env_is_shell = is_shell;
}

void sys_env_inherit_var(int sysno, u_int envid) {
    struct var_list *p_list = &(total_vars[ENVX(get_shell_id(envs[ENVX(envid)].env_parent_id))]),
            *s_list = &(total_vars[ENVX(envid)]);
    struct variable *tmp, *s;
    LIST_FOREACH(tmp, p_list, var_link) {
        if (tmp->vis == 0) {
            s = get_var();
            memcpy(s, tmp, sizeof(struct variable));
            LIST_INSERT_HEAD(s_list, s, var_link);
        }
    }
}

u_int sys_env_destory_shell(int sysno) {
    u_int pid = get_shell_id(curenv->env_id);
    struct var_list *temp_list = &(total_vars[ENVX(pid)]);
    struct variable *tmp = LIST_FIRST(temp_list), *pre;
    while (!LIST_EMPTY(temp_list)) {
        pre = tmp;
        tmp = LIST_NEXT(tmp, var_link);
        LIST_REMOVE(pre, var_link);
        pre->v = 0;
    }
    env_destroy(envs + ENVX(pid));
    return pid;
}

int sys_env_var(int sysno, char *name, char *value, u_int vis, u_int readonly, u_int op) {
    switch (op) {
        case 0:
            return create(name, value, vis, readonly, 0);
        case 1:
            return get(name, value);
        case 2:
            return create(name, value, vis, readonly, 1);
        case 3:
            return unset(name);
        case 4:
            return get_list(name, value);
        default:
            return 0;
    }
}