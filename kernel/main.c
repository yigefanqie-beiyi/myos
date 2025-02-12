#include "print.h"
#include "init.h"
#include "debug.h"
#include "string.h"
#include "memory.h"
#include "stdint.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"
#include "stdio.h"
#include "fs.h"
#include "file.h"
#include "shell.h"


void k_thread_a(void* );
void k_thread_b(void* );
void u_prog_a (void);
void u_prog_b (void);

int main(void) {
   put_str("I am kernel\n");
   init_all();

   intr_enable();
   cls_screen();
   console_put_str("[tomato@localhost /]$ ");

//    process_execute(u_prog_a, "u_prog_a");
//    process_execute(u_prog_b, "u_prog_b"); 
//    thread_start("k_thread_a", 31, k_thread_a, "argA ");
//    thread_start("k_thread_b", 31, k_thread_b, "argB ");  
//    uint32_t fd = sys_open("/file1",O_CREAT);
//    sys_close(fd);
//   uint32_t fd = sys_open("/file1",O_RDONLY);
//   printk("fd:%d\n",fd);
//    sys_write(fd,"hello,world\n",12);
//    sys_close(fd);
//    printk("fd_no : %d has closed\n",fd);
   while(1);
   return 0;
}

void init(void)
{
    uint32_t ret_pid = fork();
    if(ret_pid)
        //while(1);
		my_shell();
    else
        my_shell();
    PANIC("init: should not be here");
}


void k_thread_a(void* arg){

	void* addr1 = sys_malloc(256);
	void* addr2 = sys_malloc(255);
	void* addr3 = sys_malloc(254);
	console_put_str(" thread_a malloc addr:0x");
	console_put_int((int)addr1);
	console_put_char(',');
	console_put_int((int)addr2);
	console_put_char(',');
	console_put_int((int)addr3);
	console_put_char('\n');
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);
	while(1);
}

void k_thread_b(void* arg){
	void* addr1 = sys_malloc(256);
	void* addr2 = sys_malloc(255);
	void* addr3 = sys_malloc(254);
	console_put_str(" thread_b malloc addr:0x");
	console_put_int((int)addr1);
	console_put_char(',');
	console_put_int((int)addr2);
	console_put_char(',');
	console_put_int((int)addr3);
	console_put_char('\n');
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	sys_free(addr1);
	sys_free(addr2);
	sys_free(addr3);
	while(1);
}

void u_prog_a(void) {
	void* addr1 = malloc(256);
	void* addr2 = malloc(255);
	void* addr3 = malloc(254);
	printf(" prog_a malloc addr:0x%x, 0x%x, 0x%x\n", (int)addr1,(int)addr2,(int)addr3);
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	free(addr1);
	free(addr2);
	free(addr3);
	while(1);
}

void u_prog_b(void) {
	void* addr1 = malloc(256);
	void* addr2 = malloc(255);
	void* addr3 = malloc(254);
	printf(" prog_b malloc addr:0x%x, 0x%x, 0x%x\n", (int)addr1,(int)addr2,(int)addr3);
	int cpu_delay = 100000;
	while(cpu_delay-->0);
	free(addr1);
	free(addr2);
	free(addr3);
	while(1);
}