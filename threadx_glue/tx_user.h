#pragma once

#include <windows.h>

typedef struct TX_EVENT_FLAGS_GROUP_STRUCT
{
    char *tx_event_flags_group_name;
    ULONG tx_event_flags_group_current;
    ULONG tx_event_flags_group_id;
    CRITICAL_SECTION c;
} TX_EVENT_FLAGS_GROUP;

typedef struct TX_TIMER_INTERNAL_STRUCT
{
    ULONG tx_timer_internal_remaining_ticks;

} TX_TIMER_INTERNAL;

struct threadx
{
    HANDLE thread;
    DWORD win32_thread_id;
    VOID *tx_thread_stack_start;
    VOID *tx_thread_stack_end;
    ULONG tx_thread_id;
    ULONG tx_thread_entry_parameter;
    CHAR *tx_thread_name;
    UINT tx_thread_state;
    UINT tx_thread_suspending;
    UINT tx_thread_suspend_status;
    ULONG tx_thread_suspend_info;
    VOID *tx_thread_additional_suspend_info;
    VOID (*tx_thread_suspend_cleanup)(struct threadx *thread_ptr);
    struct threadx *tx_thread_suspended_next, *tx_thread_suspended_previous;
    VOID *tx_thread_suspend_control_block;
    void (*tx_thread_entry)(ULONG id);

    TX_TIMER_INTERNAL   tx_thread_timer;
    LIST_ENTRY entry;
};

struct semaphorex
{
    HANDLE semaphore;
    ULONG tx_semaphore_id;
    ULONG tx_semaphore_count;
};

struct mutexx
{
    HANDLE mutex;
    CHAR *tx_mutex_name;
    UINT tx_mutex_ownership_count;
    ULONG tx_mutex_id;
};

struct timerx
{
    HANDLE timer;
    ULONG tx_timer_id;
};

UINT _tx_thread_interrupt_disable(void);
void _tx_thread_interrupt_restore(UINT old_posture);
#define TX_INTERRUPT_SAVE_AREA UINT interrupt_save;
#define TX_DISABLE interrupt_save = _tx_thread_interrupt_disable();

#define TX_RESTORE _tx_thread_interrupt_restore(interrupt_save);
#define _tx_thread_current_ptr _tx_thread_identify()

#define TX_THREAD struct threadx
#define TX_SEMAPHORE struct semaphorex
#define TX_MUTEX struct mutexx
#define TX_TIMER struct timerx