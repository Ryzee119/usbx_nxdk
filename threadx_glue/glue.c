#include "ux_api.h"
#include "tx_event_flags.h"
#include "tx_thread.h"
// #include "tx_port.h"
#include <windows.h>
#include <assert.h>

TX_THREAD _tx_timer_thread;
// Hmm do we need this running and doing something? timeouts?
// https://github.com/eclipse-threadx/threadx/blob/master/common_smp/src/tx_timer_thread_entry.c#L76
volatile UINT _tx_thread_preempt_disable = 0;
volatile ULONG _tx_thread_system_state = TX_INITIALIZE_IS_FINISHED;

#define TX_MUTEX_ID 0x4D555445UL
#define TX_SEMAPHORE_ID ((ULONG)0x53454D41)

/* Not sure what the win32 equivalent of these event flags is. It is like 32 event objects in one using bitmasks
 * I just use a manual bitmask wrapped in critical sections to make it thread safe. The get function polls and yields
 its thread until the bit mask is satisfied or times out */
UINT _tx_event_flags_create(TX_EVENT_FLAGS_GROUP *group_ptr, CHAR *name_ptr) {
#ifndef TX_DISABLE_ERROR_CHECKING
    if (group_ptr == NULL || group_ptr->tx_event_flags_group_id == TX_EVENT_FLAGS_ID) {
        return TX_GROUP_ERROR;
    }
#endif

    memset(group_ptr, 0, (sizeof(TX_EVENT_FLAGS_GROUP)));
    group_ptr->tx_event_flags_group_id = TX_EVENT_FLAGS_ID;
    group_ptr->tx_event_flags_group_name = name_ptr;
    group_ptr->tx_event_flags_group_current = 0;
    InitializeCriticalSection(&group_ptr->c);

    return TX_SUCCESS;
}

UINT _tx_event_flags_delete(TX_EVENT_FLAGS_GROUP *group_ptr) {
#ifndef TX_DISABLE_ERROR_CHECKING
    if (group_ptr == NULL) {
        return TX_GROUP_ERROR;
    }
#endif

    group_ptr->tx_event_flags_group_id = TX_CLEAR_ID;
    DeleteCriticalSection(&group_ptr->c);

    return TX_SUCCESS;
}

UINT _tx_event_flags_get(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG requested_flags, UINT get_option,
                         ULONG *actual_flags_ptr, ULONG wait_option) {
    ULONG timeout;
    ULONG flags_satisfied;
    ULONG current_flags;
    ULONG and_request;

    if (requested_flags == 0) {
        return TX_SUCCESS;
    }

#ifndef TX_DISABLE_ERROR_CHECKING
    if (actual_flags_ptr == NULL) {
        return TX_PTR_ERROR;
    }

    if (group_ptr == NULL || group_ptr->tx_event_flags_group_id != TX_EVENT_FLAGS_ID) {
        return TX_GROUP_ERROR;
    }
#endif

    if (wait_option == TX_NO_WAIT) {
        timeout = GetTickCount();
    } else if (wait_option == TX_WAIT_FOREVER) {
        timeout = INFINITE;
    } else {
        timeout = GetTickCount() + (wait_option * (1000 / TX_TIMER_TICKS_PER_SECOND));
    }

    and_request = (get_option & TX_AND);

    // Fixme, could possibly wait on a semaphore here so we don't have to poll
    do {
        EnterCriticalSection(&group_ptr->c);

        current_flags = group_ptr->tx_event_flags_group_current;
        flags_satisfied = (current_flags & requested_flags);
        *actual_flags_ptr = current_flags;

        if (and_request == TX_AND && flags_satisfied != requested_flags) {
            flags_satisfied = 0;
        }

        if (flags_satisfied) {
            if (get_option & TX_EVENT_FLAGS_CLEAR_MASK) {
                current_flags &= ~requested_flags;
                group_ptr->tx_event_flags_group_current = current_flags;
            }
        }

        LeaveCriticalSection(&group_ptr->c);

        if (wait_option != TX_NO_WAIT) {
            SwitchToThread();
            if (group_ptr->tx_event_flags_group_id != TX_EVENT_FLAGS_ID) {
                return TX_DELETED;
            }
        }

    } while (!flags_satisfied && (GetTickCount() < timeout));

    return (flags_satisfied) ? TX_SUCCESS : TX_NO_EVENTS;
}

UINT _tx_event_flags_set(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG flags_to_set, UINT set_option) {
#ifndef TX_DISABLE_ERROR_CHECKING
    if (group_ptr == NULL) {
        return TX_GROUP_ERROR;
    }

    if (set_option != TX_OR && set_option != TX_AND) {
        return TX_OPTION_ERROR;
    }
#endif
    EnterCriticalSection(&group_ptr->c);
    if (set_option == TX_OR) {
        group_ptr->tx_event_flags_group_current |= flags_to_set;
    } else if (set_option == TX_AND) {
        group_ptr->tx_event_flags_group_current &= flags_to_set;
    }
    LeaveCriticalSection(&group_ptr->c);
    return TX_SUCCESS;
}

UINT _tx_mutex_create(TX_MUTEX *mutex_ptr, CHAR *name_ptr, UINT inherit) {
    assert(inherit == TX_NO_INHERIT);
    memset(mutex_ptr, 0, (sizeof(TX_MUTEX)));
    mutex_ptr->mutex = CreateMutexA(NULL, FALSE, name_ptr);
    if (mutex_ptr->mutex == NULL) {
        return TX_MUTEX_ERROR;
    }
    mutex_ptr->tx_mutex_name = name_ptr;
    mutex_ptr->tx_mutex_id = TX_MUTEX_ID;
    return TX_SUCCESS;
}

UINT _tx_mutex_delete(TX_MUTEX *mutex_ptr) {
    if (mutex_ptr->tx_mutex_id != TX_MUTEX_ID) {
        return TX_MUTEX_ERROR;
    }
    mutex_ptr->tx_mutex_id = TX_CLEAR_ID;
    CloseHandle(mutex_ptr->mutex);
    return TX_SUCCESS;
}

UINT _tx_mutex_put(TX_MUTEX *mutex_ptr) {
    UINT status = ReleaseMutex(mutex_ptr->mutex) ? TX_SUCCESS : TX_NOT_OWNED;
    return status;
}

UINT _tx_mutex_get(TX_MUTEX *mutex_ptr, ULONG wait_option) {
    DWORD waitResult;
    if (wait_option == TX_NO_WAIT) {
        wait_option = 0;
    } else if (wait_option == TX_WAIT_FOREVER) {
        wait_option = INFINITE;
    } else {
        wait_option = wait_option * (1000 / TX_TIMER_TICKS_PER_SECOND);
    }

    waitResult = WaitForSingleObject(mutex_ptr->mutex, wait_option);

    if (waitResult == WAIT_OBJECT_0) {
        return TX_SUCCESS;
    } else if (waitResult == WAIT_TIMEOUT) {
        return TX_NOT_AVAILABLE;
    } else {
        return TX_DELETED;
    }
}

UINT _tx_semaphore_create(TX_SEMAPHORE *semaphore_ptr, CHAR *name_ptr, ULONG initial_count) {
    memset(semaphore_ptr, 0, (sizeof(TX_SEMAPHORE)));
    semaphore_ptr->semaphore = CreateSemaphore(NULL, initial_count, 4096, name_ptr);
    if (semaphore_ptr->semaphore == NULL) {
        return TX_SEMAPHORE_ERROR;
    }
    semaphore_ptr->tx_semaphore_id = TX_SEMAPHORE_ID;
    semaphore_ptr->tx_semaphore_count = initial_count;
    return TX_SUCCESS;
}

UINT _tx_semaphore_delete(TX_SEMAPHORE *semaphore_ptr) {
    CloseHandle(semaphore_ptr->semaphore);
    semaphore_ptr->tx_semaphore_id = TX_CLEAR_ID;
    return TX_SUCCESS;
}

UINT _tx_semaphore_get(TX_SEMAPHORE *semaphore_ptr, ULONG wait_option) {
    DWORD waitResult;
    if (wait_option == TX_NO_WAIT) {
        wait_option = 0;
    } else if (wait_option == TX_WAIT_FOREVER) {
        wait_option = INFINITE;
    } else {
        wait_option = wait_option * (1000 / TX_TIMER_TICKS_PER_SECOND);
    }

    waitResult = WaitForSingleObject(semaphore_ptr->semaphore, wait_option);
    semaphore_ptr->tx_semaphore_count--;

    if (waitResult == WAIT_OBJECT_0) {
        return TX_SUCCESS;
    } else if (waitResult == WAIT_TIMEOUT) {
        return TX_NO_INSTANCE;
    } else {
        return TX_MUTEX_ERROR;
    }
}

UINT _tx_semaphore_put(TX_SEMAPHORE *semaphore_ptr) {
    UINT status = TX_NOT_DONE;
    status = (ReleaseSemaphore(semaphore_ptr->semaphore, 1, NULL) == 0) ? TX_SEMAPHORE_ERROR : TX_SUCCESS;
    semaphore_ptr->tx_semaphore_count++;
    return status;
}

LIST_ENTRY thread_list_head = {&thread_list_head, &thread_list_head};

TX_THREAD *_tx_thread_identify(VOID) {
    // Thread indentity needs to return the threadx struct instead of the win32 thread ID
    // so we have created a linked list of threadx threads to search through.
    DWORD win32_thread_id = GetThreadId(GetCurrentThread());

    PLIST_ENTRY entry = thread_list_head.Flink;
    while (entry != &thread_list_head) {
        TX_THREAD *thread_ptr = CONTAINING_RECORD(entry, TX_THREAD, entry);
        if (thread_ptr->win32_thread_id == win32_thread_id) {
            return thread_ptr;
        }
        entry = entry->Flink;
    }

    assert(0);
    return TX_NULL;
}

UINT _tx_thread_sleep(ULONG timer_ticks) {
    Sleep(timer_ticks * 1000 / TX_TIMER_TICKS_PER_SECOND);
    return TX_SUCCESS;
}

UINT _tx_thread_interrupt_disable(void) { return KeRaiseIrqlToDpcLevel(); }

void _tx_thread_interrupt_restore(UINT old_posture) { KfLowerIrql(old_posture); }

static DWORD WINAPI thread_entry(LPVOID lpParameter) {
    TX_THREAD *thread_ptr = (TX_THREAD *)lpParameter;
    thread_ptr->tx_thread_entry(thread_ptr->tx_thread_entry_parameter);
    return 0;
}

UINT _tx_thread_create(TX_THREAD *thread_ptr, CHAR *name_ptr, VOID (*entry_function)(ULONG id), ULONG entry_input,
                       VOID *stack_start, ULONG stack_size, UINT priority, UINT preempt_threshold, ULONG time_slice,
                       UINT auto_start) {
    (void)preempt_threshold;
    (void)time_slice;
    (void)priority;
    memset(thread_ptr, 0, (sizeof(TX_THREAD)));

    thread_ptr->tx_thread_entry = entry_function;
    thread_ptr->tx_thread_entry_parameter = entry_input;
    thread_ptr->tx_thread_name = name_ptr;
    thread_ptr->tx_thread_state = (auto_start == TX_AUTO_START) ? TX_READY : TX_SUSPENDED;
    thread_ptr->tx_thread_id = TX_THREAD_ID;
    thread_ptr->thread = CreateThread(NULL, PAGE_SIZE, thread_entry, thread_ptr, (auto_start) ? 0 : CREATE_SUSPENDED,
                                      &thread_ptr->win32_thread_id);

    // ThreadX compares a variables memory location with a threads stack memory to determine ownership in certain cases.
    // We can use the stack base and limit to get the same effect
    PETHREAD ethread;
    if (ObReferenceObjectByHandle(thread_ptr->thread, &PsThreadObjectType, (PVOID *)&ethread) == STATUS_SUCCESS) {
        PKTHREAD current_thread = &ethread->Tcb;
        thread_ptr->tx_thread_stack_start = current_thread->StackLimit;
        thread_ptr->tx_thread_stack_end = current_thread->StackBase;
    }

    InsertTailList(&thread_list_head, &thread_ptr->entry);
    return TX_SUCCESS;
}

VOID _tx_thread_relinquish(VOID) { SwitchToThread(); }

UINT _tx_thread_terminate(TX_THREAD *thread_ptr) {
    assert(0);
    return TX_FEATURE_NOT_ENABLED;
}

UINT _tx_thread_delete(TX_THREAD *thread_ptr) {
    DWORD waitResult = WaitForSingleObject(thread_ptr->thread, 100);
    if (waitResult != WAIT_OBJECT_0) {
        return TX_DELETE_ERROR;
    }
    thread_ptr->tx_thread_id = TX_CLEAR_ID;
    CloseHandle(thread_ptr->thread);
    RemoveEntryList(&thread_ptr->entry);
    return TX_SUCCESS;
}

UINT _tx_thread_resume(TX_THREAD *thread_ptr) {
    // FIXME, use a winapi function instead of xboxkrnl NT function
    NTSTATUS status = NtResumeThread(thread_ptr->thread, NULL);
    if (NT_SUCCESS(status)) {
        thread_ptr->tx_thread_state = TX_READY;
        thread_ptr->tx_thread_suspending = TX_FALSE;
        return TX_SUCCESS;
    } else {
        return TX_RESUME_ERROR;
    }
}

UINT _tx_thread_suspend(TX_THREAD *thread_ptr) {
    // FIXME, use a winapi function instead of xboxkrnl NT function
    thread_ptr->tx_thread_suspending = TX_TRUE;
    NTSTATUS status = NtSuspendThread(thread_ptr->thread, NULL);
    if (NT_SUCCESS(status)) {
        thread_ptr->tx_thread_state = TX_SUSPENDED;
        thread_ptr->tx_thread_suspending = TX_FALSE;
        return TX_SUCCESS;
    } else {
        return TX_SUSPEND_ERROR;
    }
}

UINT _tx_thread_info_get(TX_THREAD *thread_ptr, CHAR **name, UINT *state, ULONG *run_count, UINT *priority,
                         UINT *preemption_threshold, ULONG *time_slice, TX_THREAD **next_thread,
                         TX_THREAD **next_suspended_thread) {
    if (name)
        *name = thread_ptr->tx_thread_name;
    if (state)
        *state = thread_ptr->tx_thread_state;
    if (run_count)
        *run_count = 0;
    if (priority)
        *priority = 1;
    if (preemption_threshold)
        *preemption_threshold = 0;
    if (time_slice)
        *time_slice = 0;
    if (next_thread)
        *next_thread = CONTAINING_RECORD(thread_ptr->entry.Flink, TX_THREAD, entry);
    if (next_suspended_thread)
        *next_suspended_thread = TX_NULL;
    return TX_SUCCESS;
}

/* Let's just keep threads at normal priority */
UINT _tx_thread_priority_change(TX_THREAD *thread_ptr, UINT new_priority, UINT *old_priority) { return TX_SUCCESS; }

VOID _tx_thread_system_resume(TX_THREAD *thread_ptr) {
    _tx_thread_resume(thread_ptr);
    return;
}

VOID _tx_thread_system_suspend(TX_THREAD *thread_ptr) {
    _tx_thread_suspend(thread_ptr);
    return;
}

VOID _tx_thread_system_preempt_check(VOID) { return; }

ULONG _tx_time_get(VOID) { return GetTickCount(); }

/* Only used in some test code; implemented so it compiles */
UINT _tx_timer_create(TX_TIMER *timer_ptr, CHAR *name_ptr, VOID (*expiration_function)(ULONG id),
                      ULONG expiration_input, ULONG initial_ticks, ULONG reschedule_ticks, UINT auto_activate) {
    assert(0);
    return TX_FEATURE_NOT_ENABLED;
}

/* Only used in some test code; implemented so it compiles */
UINT _tx_timer_delete(TX_TIMER *timer_ptr) {
    assert(0);
    return TX_FEATURE_NOT_ENABLED;
}

/* Only used in some test code; implemented so it compiles */
UINT _tx_timer_deactivate(TX_TIMER *timer_ptr) {
    assert(0);
    (void)timer_ptr;
    return TX_FEATURE_NOT_ENABLED;
}

/* Only used in some test code; implemented so it compiles */
UINT _tx_thread_preemption_change(TX_THREAD *thread_ptr, UINT new_threshold, UINT *old_threshold) {
    (void)thread_ptr;
    (void)new_threshold;
    (void)old_threshold;
    return TX_FEATURE_NOT_ENABLED;
}
