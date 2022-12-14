#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "lib/string.h"
#include "threads/palloc.h"

/*Project 3*/
#include "include/vm/vm.h"
#include "include/vm/file.h"

/*projcet 4*/
#include "include/filesys/inode.h"
#include "include/filesys/directory.h"


void syscall_entry(void);
void syscall_handler(struct intr_frame *);
void check_address(void *addr);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int exec(const char *cmd_line);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
struct file *find_file(int fd);
int fork(const char *thread_name, struct intr_frame *if_);
int wait(int pid);
void close(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);
int add_file(struct file *file);
int dup2(int oldfd, int newfd);
void remove_file(int fd);
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);
struct page * check_address2(void *addr);
void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write);
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);
int symlink (const char *target, const char *linkpath);

/*project 4*/
bool sys_isdir(int fd);
bool sys_chdir(const char *path_name);
bool sys_mkdir(const char *dir);
bool sys_readdir(int fd, char *name);
cluster_t sys_inumber(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */



void syscall_init(void)
{
	lock_init(&filesys_lock);
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
#ifdef VM
	thread_current()->rsp = f->rsp;
#endif
	// TODO: Your implementation goes here.
	uint64_t number = f->R.rax;
	switch (number)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 0);
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 1);
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_DUP2: // project2 - extra
		f->R.rax = dup2(f->R.rdi, f->R.rsi);
		break;
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(f->R.rdi);
		break;
	case SYS_ISDIR:
		f->R.rax = sys_isdir(f->R.rdi);
		break;
	case SYS_CHDIR:
		f->R.rax = sys_chdir(f->R.rdi);
		break;
	case SYS_MKDIR:
		f->R.rax = sys_mkdir(f->R.rdi);
		break;
	case SYS_READDIR:
		f->R.rax = sys_readdir(f->R.rdi, f->R.rsi);
		break;
	case SYS_INUMBER:
		f->R.rax = sys_inumber(f->R.rdi);
		break;
	default:
		thread_exit();
		break;
	}
}

/*project 4*/
bool sys_isdir(int fd) {
	struct file *file = find_file(fd);
	if (file == NULL) {
		return false;
	}
	return inode_is_dir(file_get_inode(file));
}

bool sys_chdir(const char *path_name) {
    if (path_name == NULL) {
        return false;
	}

    // name??? ?????? ?????? ??? cp_name??? ??????
    char *cp_name = (char *)malloc(strlen(path_name) + 1);
    strlcpy(cp_name, path_name, strlen(path_name) + 1);

    struct dir *chdir = NULL;

    if (cp_name[0] == "/") {	// ?????? ????????? ???????????? ?????? ?????????
        chdir = dir_open_root();
    }
    else {						// ?????? ????????? ???????????? ?????? ?????????
        chdir = dir_reopen(thread_current()->cur_dir);
	}

    // dir????????? ???????????? ??????????????? ??????
    char *token, *savePtr;
    token = strtok_r(cp_name, "/", &savePtr);

    struct inode *inode = NULL;
    while (token != NULL) {
        // dir?????? token????????? ????????? ???????????? inode??? ????????? ??????
        if (!dir_lookup(chdir, token, &inode)) {
            dir_close(chdir);
            return false;
        }

        // inode??? ????????? ?????? NULL ??????
        if (!inode_is_dir(inode)) {
            dir_close(chdir);
            return false;
        }

        // dir??? ???????????? ????????? ??????????????? ??????
        dir_close(chdir);
        
        // inode??? ???????????? ????????? dir?????????
        chdir = dir_open(inode);

        // token??????????????????????????????
        token = strtok_r(NULL, "/", &savePtr);
    }
    // ?????????????????????????????????????????????
    dir_close(thread_current()->cur_dir);
    thread_current()->cur_dir = chdir;
    free(cp_name);
    return true;
}

bool sys_mkdir(const char *dir) {
    lock_acquire(&lock);
    bool new_dir = filesys_create_dir(dir);
    lock_release(&lock);
    return new_dir;
}
bool sys_readdir(int fd, char *name) {
    if (name == NULL) {
        return false;
	}

    // fd??????????????? fd??? ?????? file?????? ?????????
	struct file *target = find_file(fd);
    if (target == NULL) {
        return false;
	}

    // fd??? file->inode??? ?????????????????? ??????
    if (!inode_is_dir(file_get_inode(target))) {
        return false;
	}

    // p_file??? dir??????????????? ?????????
    struct dir *p_file = target;
    if (p_file->pos == 0) {
        dir_seek(p_file, 2 * sizeof(struct dir_entry));		// ".", ".." ??????
	}

    // ??????????????? ??????????????? ".", ".." ????????? ????????? ??????????????? name??? ??????
    bool result = dir_readdir(p_file, name);

    return result;
}


cluster_t sys_inumber(int fd)
{
    struct file *file = find_file(fd);

	if (file == NULL) {
		return false;
	}

    return inode_get_inumber(file_get_inode(file));
}

void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write) {
	// printf("======check valid buffer\n");
    for (int i = 0; i < size; i++) {
        struct page* page = check_address2(buffer + i);    // ????????? ?????? buffer?????? buffer + size????????? ????????? ??? ???????????? ????????? ???????????? ??????
        if(page == NULL)
            exit(-1);
        if(to_write == true && page->writable == false)
            exit(-1);
    }
}

struct page * check_address2(void *addr) {
    if (is_kernel_vaddr(addr))
    {
        exit(-1);
    }
    return spt_find_page(&thread_current()->spt, addr);
}

void *mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
	if (offset % PGSIZE != 0)
		return NULL;
	if (addr == NULL || (long long)length <= 0)
		return NULL;
	if (pg_round_down(addr) != addr || is_kernel_vaddr(addr))
		return NULL;
	if (spt_find_page(&thread_current()->spt, addr))
		return NULL;
	if (fd == 0 || fd == 1)
		exit(-1);
	struct file *file = find_file(fd);
	if (file == NULL)
		return NULL;

	return do_mmap(addr, length, writable, file, offset);
}

void munmap(void *addr)
{
	do_munmap(addr);
}

int dup2(int oldfd, int newfd)
{
	// ????????? ?????? ??????????????? oldfd??? ????????? newfd??? ???????????? ???????????? ??????.
	// newfd??? ????????? ????????????, ??????????????? ?????? ???????????? ?????????.
	// oldfd??? ??????????????? ??? ????????? ?????? ???????????? -1??? ??????, newfd??? ????????? ?????????.
	// oldfd??? ???????????? newfd??? oldfd??? ?????? ?????? ????????????, dup2() ????????? ???????????? ?????? newfd?????? ????????? ??????
	struct file *file = find_file(oldfd);
	if (file == NULL)
	{
		return -1;
	}
	if (oldfd == newfd)
	{
		return newfd;
	}

	struct thread *curr = thread_current();
	struct file **curr_fd_table = curr->fd_table;
	if (file == STDIN)
	{
		curr->stdin_count++;
	}
	else if (file == STDOUT)
	{
		curr->stdout_count++;
	}
	else
	{
		file->dup_count++;
	}

	close(newfd);
	curr_fd_table[newfd] = file;
	return newfd;
}

/* Project2-2 User Memory Access */
void check_address(void *addr)
{
	struct thread *curr = thread_current();
	if (!is_user_vaddr(addr) || addr == NULL || spt_find_page(&curr->spt, addr) == NULL)
	{
		exit(-1);
	}
}

/* Project2-3 System Call */
void halt(void)
{
	power_off();
}

/* Project2-3 System Call */
void exit(int status)
{
	/* status??? 1??? ????????? ????????? ?????? ?????? */
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

/* Project2-3 System Call */
bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	lock_acquire(&filesys_lock);
	bool t = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return t;
}

/* Project2-3 System Call */
bool remove(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

/* Project2-3 System Call */
int exec(const char *cmd_line)
{
	check_address(cmd_line);
	char *copy = palloc_get_page(PAL_ZERO);
	if (copy == NULL)
	{
		exit(-1);
	}
	strlcpy(copy, cmd_line, strlen(cmd_line) + 1);
	struct thread *curr = thread_current();
	if (process_exec(copy) == -1)
	{
		return -1;
	}
	NOT_REACHED();
	return 0;
}

/* Project2-3 System Call */
int open(const char *file)
{
	check_address(file);
	lock_acquire(&filesys_lock);
	struct file *fileobj = filesys_open(file);
	lock_release(&filesys_lock);

	if (fileobj == NULL)
	{
		return -1;
	}

	int fd = add_file(fileobj); // fdt : file data table

	// fd table??? ?????? ?????????
	if (fd == -1)
	{
		file_close(fileobj);
	}

	return fd;
}

int add_file(struct file *file)
{
	struct thread *cur = thread_current();
	struct file **fdt = cur->fd_table;

	// int idx = cur->fd_idx;
	while (cur->fd_idx < FDCOUNT_LIMIT && fdt[cur->fd_idx])
	{
		cur->fd_idx++;
	}

	// fdt??? ?????? ?????????
	if (cur->fd_idx >= FDCOUNT_LIMIT)
		return -1;

	fdt[cur->fd_idx] = file;
	return cur->fd_idx;
}

/* Project2-3 System Call */
int filesize(int fd)
{
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
	{
		return -1;
	}
	struct file *file = find_file(fd);
	return file_length(file);
}

/* Project2-3 System Call */
int read(int fd, void *buffer, unsigned size)
{
	check_address(buffer);
	
	off_t char_count = 0;
	struct thread *cur = thread_current();
	struct file *file = find_file(fd);

	if (fd == NULL)
	{
		return -1;
	}

	if (file == NULL || file == STDOUT)
	{
		return -1;
	}

	/* Keyboard ?????? ?????? */
	if (file == STDIN)
	{
		if (cur->stdin_count == 0)
		{
			// ????????? ???????????? stdin fd??? ??????.
			NOT_REACHED();
			remove_file(fd);
			return -1;
		}
		while (char_count < size)
		{
			char key = input_getc();
			*(char *)buffer = key;
			char_count++;
			(char *)buffer++;
			if (key == '\0')
			{
				break;
			}
		}
	}
	else
	{
		// if (!lock_held_by_current_thread(&filesys_lock)){
		// 	lock_acquire(&filesys_lock);
		// }
		lock_acquire(&filesys_lock);
		char_count = file_read(file, buffer, size);
		lock_release(&filesys_lock);
		
	}
	return char_count;
}

/* Project2-3 System Call */
int write(int fd, const void *buffer, unsigned size)
{
	check_address(buffer);
	off_t write_size = 0;
	struct thread *cur = thread_current();
	struct file *file = find_file(fd);

	if (fd == NULL)
	{
		return -1;
	}

	if (file == NULL || file == STDIN)
	{
		return -1;
	}

	if (file == STDOUT)
	{
		if (cur->stdout_count == 0)
		{
			remove_file(fd);
			return -1;
		}
		putbuf(buffer, size);
		return size;
	}
	else
	{
		lock_acquire(&filesys_lock);
		write_size = file_write(file, buffer, size);
		lock_release(&filesys_lock);
	}
	return write_size;
}

/* Project2-3 System Call */
struct file *find_file(int fd)
{
	struct thread *curr = thread_current();
	if (fd < 0 || fd >= FDCOUNT_LIMIT)
	{
		return NULL;
	}
	return curr->fd_table[fd];
}

/* Project2-3 System Call */
int fork(const char *thread_name, struct intr_frame *if_)
{
	check_address(thread_name);
	return process_fork(thread_name, if_);
}

/* Project2-3 System Call */
int wait(int pid)
{
	return process_wait(pid);
}

/* Project2-3 System Call */
void close(int fd)
{
	struct file *close_file = find_file(fd);
	if (close_file == NULL)
	{
		return;
	}

	struct thread *curr = thread_current();

	if (fd == 0 || close_file == STDIN)
		curr->stdin_count--;
	else if (fd == 1 || close_file == STDOUT)
		curr->stdout_count--;

	remove_file(fd);

	if (fd < 2 || close_file <= 2)
	{
		return;
	}

	if (close_file->dup_count == 0)
	{
		file_close(close_file);
	}
	else
	{
		close_file->dup_count--;
	}
}

void remove_file(int fd)
{
	struct thread *cur = thread_current();

	if (fd < 0 || fd >= FDCOUNT_LIMIT)
		return;

	cur->fd_table[fd] = NULL;
}

/* Project2-3 System Call */
void seek(int fd, unsigned position)
{
	struct file *file = find_file(fd);
	if (file <= 2)
	{
		return;
	}
	file_seek(file, position);
}

unsigned tell(int fd)
{
	struct file *file = find_file(fd);
	if (file <= 2)
	{
		return;
	}
	return file_tell(file);
}

/* Project 4 */
/* ??????????????? ?????? ?????? ??????????????? ?????? ?????? ?????? ?????? ?????? dir??? ?????? */
bool chdir (const char *dir){
	char *path = dir;
    return filesys_change_dir(path);
}

bool mkdir (const char *dir){
	char *path = dir;
    return filesys_create_dir(path);
}
// bool readdir (int fd, char *name){

// }
bool isdir (int fd){
	struct file *f = find_file(fd);
	return f->inode->data.is_dir;
}
// int inumber (int fd);
// int symlink (const char *target, const char *linkpath);