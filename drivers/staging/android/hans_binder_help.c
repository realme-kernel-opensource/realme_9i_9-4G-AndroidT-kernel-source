/***********************************************************
** Copyright (C), 2008-2019, OPPO Mobile Comm Corp., Ltd.
** File: hans.c
** Description: Add for hans freeze manager
**
** Version: 1.0
** Date : 2020/11/03
**
** ------------------ Revision History:------------------------
** <author>      <data>      <version >       <desc>
** Kun Zhou    2020/11/03      1.0       OPLUS_ARCH_EXTENDS
****************************************************************/

/*tr, target_proc, proc, check point: true-sync binder, false-async binder*/
void hans_check_binder(struct binder_transaction_data *tr, struct binder_proc *proc, struct binder_proc *target_proc, bool check_point)
{
	char buf_data[INTERFACETOKEN_BUFF_SIZE];
	size_t buf_data_size;
	char buf[INTERFACETOKEN_BUFF_SIZE] = {0};
	int i = 0;
	int j = 0;

	if (check_point == true) {
		if (!(tr->flags & TF_ONE_WAY) /*report sync binder call*/
			&& target_proc
			&& (task_uid(target_proc->tsk).val > MIN_USERAPP_UID)
			&& (proc->pid != target_proc->pid)
			&& (is_frozen_tg(target_proc->tsk) || is_zombie_tg(target_proc->tsk))) {
			hans_report(SYNC_BINDER, task_tgid_nr(proc->tsk), task_uid(proc->tsk).val, task_tgid_nr(target_proc->tsk), task_uid(target_proc->tsk).val, "SYNC_BINDER", -1);
		}
	#if defined(CONFIG_CFS_BANDWIDTH)
		if (!(tr->flags & TF_ONE_WAY) /*report sync binder call*/
			&& target_proc
			&& (task_uid(target_proc->tsk).val > MIN_USERAPP_UID || task_uid(target_proc->tsk).val == HANS_SYSTEM_UID) //uid >10000
			&& (is_belong_cpugrp(target_proc->tsk) || is_zombie_tg(target_proc->tsk))) {
			hans_report(SYNC_BINDER_CPUCTL, task_tgid_nr(proc->tsk), task_uid(proc->tsk).val, task_tgid_nr(target_proc->tsk), task_uid(target_proc->tsk).val, "SYNC_BINDER_CPUCTL", -1);
		}
	#endif
	}

	if (check_point == false) {
		if ((tr->flags & TF_ONE_WAY) /*report async binder call*/
			&& target_proc
			&& (task_uid(target_proc->tsk).val > MIN_USERAPP_UID)
			&& (proc->pid != target_proc->pid)
			&& is_frozen_tg(target_proc->tsk)) {
			buf_data_size = tr->data_size>INTERFACETOKEN_BUFF_SIZE ?INTERFACETOKEN_BUFF_SIZE:tr->data_size;
			if (!copy_from_user(buf_data, (char*)tr->data.ptr.buffer, buf_data_size)) {
				/*1.skip first PARCEL_OFFSET bytes (useless data)
				  2.make sure the invalid address issue is not occuring(j =PARCEL_OFFSET+1, j+=2)
				  3.java layer uses 2 bytes char. And only the first bytes has the data.(p+=2)*/
				if (buf_data_size > PARCEL_OFFSET) {
					char *p = (char *)(buf_data) + PARCEL_OFFSET;
					j = PARCEL_OFFSET + 1;
					while (i < INTERFACETOKEN_BUFF_SIZE && j < buf_data_size && *p != '\0') {
						buf[i++] = *p;
						j += 2;
						p += 2;
					}
					if (i == INTERFACETOKEN_BUFF_SIZE) buf[i-1] = '\0';
				}
				hans_report(ASYNC_BINDER, task_tgid_nr(proc->tsk), task_uid(proc->tsk).val, task_tgid_nr(target_proc->tsk), task_uid(target_proc->tsk).val, buf, tr->code);
			}
		}
	}
}

static void get_uid_pid(int *from_pid, int *from_uid, int *to_pid, int *to_uid, struct binder_transaction *tr)
{
	if (tr->from != NULL && tr->from->proc != NULL
		&& tr->from->proc->tsk != NULL) {
		*from_pid = task_tgid_nr(tr->from->proc->tsk);
	} else {
		*from_pid = -1;
	}
        *from_uid = tr->sender_euid.val;
        *to_pid = tr->to_thread ?  tr->to_thread->proc->pid : -1;
        *to_uid = tr->to_thread ? task_uid(tr->to_thread->task).val : -1;
}

static void hans_check_uid_proc_status(struct binder_proc *proc, enum message_type type)
{
	struct rb_node *n = NULL;
	struct binder_thread *thread = NULL;
	int from_uid = -1;
	int from_pid = -1;
	int to_uid = -1;
	int to_pid = -1;
	struct binder_transaction *btrans = NULL;
	bool empty = true;
	int need_reply = -1;
	struct binder_work *w = NULL;

	/*check binder_thread/transaction_stack/binder_proc ongoing transaction*/
	binder_inner_proc_lock(proc);
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n)) {
		thread = rb_entry(n, struct binder_thread, rb_node);
		empty = binder_worklist_empty_ilocked(&thread->todo);

		if (thread->task != NULL) {
			/*has "todo" binder thread in worklist?*/
			to_uid = task_uid(thread->task).val;
			if (!empty) {
				/*scan thread->todo list*/
                                list_for_each_entry(w, &thread->todo, entry) {
					if (w != NULL && w->type == BINDER_WORK_TRANSACTION) {
						btrans = container_of(w, struct binder_transaction, work);
						spin_lock(&btrans->lock);
						if (btrans != NULL && btrans->to_thread == thread) {
							need_reply = (int)(btrans->need_reply == 1);
							get_uid_pid(&from_pid, &from_uid, &to_pid, &to_uid, btrans);
							spin_unlock(&btrans->lock);
							hans_report(type, from_pid, from_uid, to_pid, to_uid, "FROZEN_TRANS_THREAD", need_reply);
						} else {
							spin_unlock(&btrans->lock);
						}
					}
				}
			}

			// has transcation in transaction_stack?
			btrans = thread->transaction_stack;
			if (btrans) {
				spin_lock(&btrans->lock);
				if (btrans->to_thread == thread) {
					/*only report incoming binder call*/
					need_reply = (int)(btrans->need_reply == 1);
					get_uid_pid(&from_pid, &from_uid, &to_pid, &to_uid, btrans);
					spin_unlock(&btrans->lock);
					hans_report(type, from_pid, from_uid, to_pid, to_uid, "FROZEN_TRANS_STACK", need_reply);
				} else {
					spin_unlock(&btrans->lock);
				}
			}
		}
	}

	/*has "todo" binder proc in worklist*/
	empty = binder_worklist_empty_ilocked(&proc->todo);
	if (proc->tsk != NULL && !empty) {
		to_uid = task_uid(proc->tsk).val;
		list_for_each_entry(w, &proc->todo, entry) {
			if (w != NULL && w->type == BINDER_WORK_TRANSACTION) {
				btrans = container_of(w, struct binder_transaction, work);
				spin_lock(&btrans->lock);
				if (btrans != NULL && btrans->to_thread == thread) {
					need_reply = (int)(btrans->need_reply == 1);
					get_uid_pid(&from_pid, &from_uid, &to_pid, &to_uid, btrans);
					spin_unlock(&btrans->lock);
					hans_report(type, from_pid, from_uid, to_pid, to_uid, "FROZEN_TRANS_PROC", need_reply);
				} else {
					spin_unlock(&btrans->lock);
				}
			}
		}
	}
	binder_inner_proc_unlock(proc);
}

void hans_check_frozen_transcation(uid_t uid, enum message_type type)
{
	struct binder_proc *proc;

	mutex_lock(&binder_procs_lock);
	hlist_for_each_entry(proc, &binder_procs, proc_node) {
		if (proc != NULL && (task_uid(proc->tsk).val == uid)) {
			hans_check_uid_proc_status(proc, type);
		}
	}
	mutex_unlock(&binder_procs_lock);
}
