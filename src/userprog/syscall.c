#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <debug.h>
#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static unsigned int get_user_fb(const uint8_t *uaddr);
static void verify_addr_usable(const void * addr);
static struct fd * search_by_fd(int fd);
static void verify_addr_writable(const void * addr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  unsigned int syscall_number = get_user_fb(f->esp);
  unsigned int result = f->eax;

  switch(syscall_number){
    case SYS_HALT:
		halt();
		NOT_REACHED();
	case SYS_EXIT:
		exit(get_user_fb(f->esp+4));
		NOT_REACHED();
	case SYS_EXEC:
		result = exec((const char *)get_user_fb(f->esp+4));
		break;
	case SYS_WAIT:
		result = wait((pid_t)get_user_fb(f->esp+4));
		break;
	case SYS_CREATE:
		result = create((const char *)get_user_fb(f->esp+4),(unsigned)get_user_fb(f->esp+8));
	    break;
	case SYS_REMOVE:
		result = remove((const char *)get_user_fb(f->esp+4));
		break;
	case SYS_OPEN:
		result = open((const char *)get_user_fb(f->esp+4));
		break;
	case SYS_FILESIZE:
		result = filesize((int)get_user_fb(f->esp+4));
		break;
	case SYS_READ:
		result = read((int)get_user_fb(f->esp+4),(void *)get_user_fb(f->esp+8),(unsigned)get_user_fb(f->esp+12));
		break;
	case SYS_WRITE:
		result = write((int)get_user_fb(f->esp+4), (const void *)get_user_fb(f->esp+8), (unsigned)get_user_fb(f->esp+12));
		break;
	case SYS_SEEK:
		seek((int)get_user_fb(f->esp+4),(unsigned)get_user_fb(f->esp+8));
		break;
	case SYS_TELL:
		result = tell((int)get_user_fb(f->esp+4));
	case SYS_CLOSE:
		close((int)get_user_fb(f->esp+4));
		break;
	default:
		NOT_REACHED();
  }
  f->eax = result;

  return;
}

void halt(void){
  shutdown_power_off();
  NOT_REACHED();
}

void exit(int status){
  struct thread * current_thread = thread_current();
  struct thread * parent_thread = NULL;
  struct child_process * t = NULL;

  printf("%s: exit(%d)\n", thread_name(), status);

  if(current_thread->ppid != -1){
    parent_thread = search_by_pid(current_thread->ppid);
	enum intr_level old_level = intr_disable();
	struct list_elem * e = NULL;
	for(e = list_begin(&parent_thread->child_process_list); e != list_end(&parent_thread->child_process_list); e = list_next(e))
	{
	  t = list_entry(e, struct child_process, elem);
	  if(t->pid == current_thread->tid){
	    break;
	  }
	}
	intr_set_level(old_level);

	t->exit_status = status;
	t->is_exited = true;
	sema_up(&t->wait_sema);
  }

  thread_exit();
}

pid_t exec(const char * cmd_line){
  verify_addr_usable(cmd_line);
  return process_execute(cmd_line);
}

int wait(pid_t pid){
  return process_wait(pid);
}

bool create(const char * file, unsigned initial_size){
  verify_addr_usable(file);
  
  lock_acquire(&filesys_lock);
  bool res = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  return res;
}

bool remove(const char * file){
  verify_addr_usable(file);
  
  lock_acquire(&filesys_lock);
  bool res = filesys_remove(file);
  lock_release(&filesys_lock);

  return res;
}

int open(const char * file){
  verify_addr_usable(file);
  int index = -1;

  lock_acquire(&filesys_lock);

  struct fd * new_fd = malloc(sizeof(struct fd));

  if(new_fd == NULL){
    goto done;
  }

  new_fd->file = filesys_open(file);

  if(new_fd->file == NULL){
    goto done;
  }

  index = 1;
  while(search_by_fd(++index) != NULL);

  new_fd->fd = index;
  
  list_push_back(&thread_current()->fd_table, &new_fd->elem);

done:
  lock_release(&filesys_lock);
  return index;
}

int filesize(int fd){
  struct fd * picked_fd = search_by_fd(fd);
  if(picked_fd == NULL){
    return -1;
  }

  lock_acquire(&filesys_lock);
  int length = file_length(picked_fd->file);
  lock_release(&filesys_lock);

  return length;
}

int read(int fd, void * buffer, unsigned size){
  verify_addr_writable(buffer);
  verify_addr_writable(buffer+size-1);

  if(fd == 0){
    int i;
	for(i = 0; i < size; i++){
	  char character = input_getc();
	  if(!put_user(buffer+i, character)){
	    exit(-1);
	  }
	}
	return size;
  }
  else{
    struct fd * picked_fd = search_by_fd(fd);
	if(picked_fd == NULL){
	  return -1;
	}
	
	lock_acquire(&filesys_lock);
	int res = file_read(picked_fd->file, buffer, size);
	lock_release(&filesys_lock);
	return res;
  }

  return -1;
}

int write(int fd, const void * buffer, unsigned size){
  verify_addr_usable(buffer);
  verify_addr_usable(buffer+size-1);

  if(fd == 1){
    putbuf(buffer,  size);
	return size;
  }
  else{
    struct fd * picked_fd = search_by_fd(fd);
	if(picked_fd == NULL){
	  return -1;
	}

	lock_acquire(&filesys_lock);
	int res = file_write(picked_fd->file, buffer, size);
	lock_release(&filesys_lock);
	return res;
  }

  return -1;
}

void seek(int fd, unsigned position){
  struct fd * picked_fd = search_by_fd(fd);

  if(picked_fd == NULL){
    return;
  }

  lock_acquire(&filesys_lock);
  file_seek(picked_fd->file, position);
  lock_release(&filesys_lock);

  return;
}

unsigned tell(int fd){
  struct fd * picked_fd = search_by_fd(fd);

  if(picked_fd == NULL){
    return  -1;
  }

  lock_acquire(&filesys_lock);
  int res = file_tell(picked_fd->file);
  lock_release(&filesys_lock);

  return res;
}

void close(int fd){
  struct fd * picked_fd = search_by_fd(fd);

  if(picked_fd == NULL){
    return;
  }

  lock_acquire(&filesys_lock);
  file_close(picked_fd->file);
  lock_release(&filesys_lock);

  list_remove(&picked_fd->elem);

  free(picked_fd);

  return;
}

static void verify_addr_usable(const void * addr){
  if(!is_user_vaddr(addr)){
    exit(-1);
  }
  if(get_user(addr) == -1){
    exit(-1);
  }
}

static void verify_addr_writable(const void * addr){
  if(!is_user_vaddr(addr)){
    exit(-1);
  }
  int bucket = get_user(addr);
  if(bucket == -1){
    exit(-1);
  }
  if(!put_user(addr,(uint8_t)bucket)){
    exit(-1);
  }
}

/* Reads a byte at user virtual address UADDR.
      UADDR must be below PHYS_BASE.
	     Returns the byte value if successful, -1 if a segfault
		    occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" 
	   : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
      UDST must be below PHYS_BASE.
	     Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* Get address, read from them, turn it in into unsigned int(4-byte)
      Endian is important. */
static unsigned int
get_user_fb(const uint8_t *uaddr)
{
  int iter;
  unsigned int result = 0;
  int temp = 0;

  if(uaddr < PHYS_BASE && uaddr+3 < PHYS_BASE){
    for(iter = 0; iter < 4; iter++){
	  temp = get_user(uaddr+iter);
	  if(temp != -1){
	    result = result + ((unsigned int)temp << (8 * iter));
  	  }else exit(-1);
   	}
  }else exit(-1);

  return result;
}

static struct fd * search_by_fd(int fd){
  struct thread * t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->fd_table); e != list_end (&t->fd_table);	e = list_next(e))
  {
    struct fd * f = list_entry (e, struct fd, elem);
	if(f->fd == fd){
	  return f;
	}
  }
  return NULL;
}
