/*
 * Author: Joseph Alhajri
 * License: BSD-2-Clause
 * omroot.io
 * Tested on Kernel v5.10.4
 **/ 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/list.h>
#include <linux/workqueue.h>

#define JIFFIES_DELAY 1
#define PTRACE_KILLER_QUEUE_NAME "ptrace_kill_worker"

static void intrpt_routine(struct work_struct*);
static DECLARE_DELAYED_WORK(ptracekill_task, &intrpt_routine);

static struct workqueue_struct* ptracekill_workqueue;

static int loaded;
/* 
 * loaded:
 * 1 : loaded.
 * 0 : unloaded.
 **/

static int enabled = 1;
/* 
 * enabled:
 * 1 : enabled.
 * 0 : not enabled.
 *
 * module_param :
 *            can be read and written by root only.
 * Defaults to enabled unless specified by the module parameter when loaded.
 **/
module_param(enabled, int, S_IWUSR | S_IRUSR);

/* 
 * Sends a SIGKILL fron the kernel space through a pointer to the process descriptor.
 **/
static void kill_task(struct task_struct* task){
	send_sig(SIGKILL, task, 1);	
}

/*
 * Returns 1 in case the process has any tracees.
 **/
static int is_a_tracer(struct list_head* ptraced_children){
	struct task_struct* task;
	struct list_head* list;
	list_for_each(list, ptraced_children){
		task = list_entry(list, struct task_struct, ptrace_entry);	
		if(task)
			return 1;
	}
	return 0;
}

/*
 * Takes a linked-list of the ptraced proccesses, loops through them & finally kills
 * them.
 **/
static void kill_tracee(struct list_head* ptraced_children){
	struct task_struct* task_ptraced;
	struct list_head* list;
	list_for_each(list, ptraced_children){
		task_ptraced = list_entry(list, struct task_struct, ptrace_entry);
		pr_info("ptracee -> comm: %s\tpid: %d\n\t\t\t\t  tgid: %d\tptrace: %d\n", 
				task_ptraced->comm, 
				task_ptraced->pid, 
				task_ptraced->tgid, 
				task_ptraced->ptrace);
		kill_task(task_ptraced);
	}
}

/* 
 * The core function of this module.
 **/
static void ptrace_checker(void){
	struct task_struct* task;
	for_each_process(task){
		if(is_a_tracer(&task->ptraced)){
			pr_info("ptracer -> comm: %s\tpid: %d\n\t\t\t\t  tgid: %d\tptrace: %d\n", 
					task->comm, 
					task->pid, 
					task->tgid, 
					task->ptrace);
			kill_tracee(&task->ptraced);
			pr_info("All tracees have been killed.\n");
			/*
			 * Kill the tracer once all tracees are killed
			 */
			kill_task(task);
			pr_info("Tracer's been killed.\n");
			pr_info("=====================================================\n"); // Separator
		}
	}
}

/*
 * A helper function executes the ptrace_checker
 * Every JIFFIES_DELAY jiffies.
 * The HZ equivalent of a jiffy can be obtained through:
 * $ getconf CLK_TCK
 * 100 
 * ************************************
 * seconds = jiffies/HZ = 1/100 = 0.01s
 * 100 HZ is 0.01 second.
 **/
static void intrpt_routine(struct work_struct* _){
	if(loaded && enabled)
		ptrace_checker();
	queue_delayed_work(ptracekill_workqueue, &ptracekill_task, JIFFIES_DELAY);
}

/*
 * Starting point: Loads the module.
 **/
static int __init lkm_init(void) {
	ptracekill_workqueue = create_workqueue(PTRACE_KILLER_QUEUE_NAME);
	queue_delayed_work(ptracekill_workqueue, &ptracekill_task, JIFFIES_DELAY);
	loaded = 1;
	pr_info("Loaded!\n");
	return 0;
}

/*
 * Ending point: Unloads the module.
 **/
static void __exit lkm_exit(void) {
	loaded = enabled = 0;
	// No new routines will be queued.	
	cancel_delayed_work(&ptracekill_task);
	// Waits till all routines finish.
	flush_workqueue(ptracekill_workqueue);
	destroy_workqueue(ptracekill_workqueue);
	pr_info("Unloaded.\n");
}

module_init(lkm_init);
module_exit(lkm_exit);

MODULE_AUTHOR("Joseph Alhajri");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("A simple linux kernel module that kills ptrace tracer and its tracees.");
