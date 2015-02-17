#ifndef ROS_KERN_SYSCALL_H
#define ROS_KERN_SYSCALL_H
#ifndef ROS_KERNEL
# error "This is ROS kernel header; user programs should not #include it"
#endif

#include <ros/common.h>
#include <process.h>

#define SYSTRACE_ON					0x01
#define SYSTRACE_LOUD				0x02
#define SYSTRACE_ALLPROC			0x04

#define MAX_NUM_TRACED				10
#define MAX_SYSTRACES				1024

#define SYSCALL_STRLEN				128

#define MAX_ASRC_BATCH				10

#define SYSTR_RECORD_SZ				256
#define SYSTR_BUF_SZ 				PGSIZE
#define SYSTR_PRETTY_BUF_SZ			(SYSTR_BUF_SZ -                            \
                                     sizeof(struct systrace_record))
struct systrace_record {
	struct systrace_record_anon {
		uint64_t		start_timestamp, end_timestamp;
		uintreg_t		syscallno;
		uintreg_t		arg0;
		uintreg_t		arg1;
		uintreg_t		arg2;
		uintreg_t		arg3;
		uintreg_t		arg4;
		uintreg_t		arg5;
		uintreg_t		retval;
		int				pid;
		uint32_t		coreid;
		uint32_t		vcoreid;
		char			*pretty_buf;
		uint8_t			datalen;
	};
	uint8_t			data[SYSTR_RECORD_SZ - sizeof(struct systrace_record_anon)];
};

/* Syscall table */
typedef intreg_t (*syscall_t)(struct proc *, uintreg_t, uintreg_t, uintreg_t,
                              uintreg_t, uintreg_t, uintreg_t);
struct sys_table_entry {
	syscall_t call;
	char *name;
};
extern const struct sys_table_entry syscall_table[];
extern const int max_syscall;
/* Syscall invocation */
void prep_syscalls(struct proc *p, struct syscall *sysc, unsigned int nr_calls);
void run_local_syscall(struct syscall *sysc);
intreg_t syscall(struct proc *p, uintreg_t sc_num, uintreg_t a0, uintreg_t a1,
                 uintreg_t a2, uintreg_t a3, uintreg_t a4, uintreg_t a5);
void set_errno(int errno);
int get_errno(void);
void unset_errno(void);
void set_errstr(const char *errstr, ...);
char *current_errstr(void);
struct errbuf *get_cur_errbuf(void);
void set_cur_errbuf(struct errbuf *ebuf);
char *get_cur_genbuf(void);
void __signal_syscall(struct syscall *sysc, struct proc *p);

/* Tracing functions */
void systrace_start(bool silent);
int systrace_trace_pid(struct proc *p);
void systrace_stop(void);
int systrace_reg(bool all, struct proc *p);
int systrace_dereg(bool all, struct proc *p);
void systrace_print(bool all, struct proc *p);
void systrace_clear_buffer(void);

/* Utility */
bool syscall_uses_fd(struct syscall *sysc, int fd);
void print_sysc(struct proc *p, struct syscall *sysc);

#endif /* !ROS_KERN_SYSCALL_H */
