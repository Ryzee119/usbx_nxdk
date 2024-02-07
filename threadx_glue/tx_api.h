#ifndef TX_API_H
#define TX_API_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ux_user.h"

#ifndef ALIGN_TYPE_DEFINED
#define ALIGN_TYPE ULONG
#define ALIGN_TYPE_DEFINED
#endif

#define TX_WAIT_FOREVER ((ULONG)0xFFFFFFFFUL)
#define TX_AND ((UINT)2)
#define TX_AND_CLEAR ((UINT)3)
#define TX_OR ((UINT)0)
#define TX_OR_CLEAR ((UINT)1)
#define TX_NULL ((void *)0)
#define TX_INHERIT ((UINT)1)
#define TX_NO_INHERIT ((UINT)0)
#define TX_NO_WAIT ((ULONG)0)
#define TX_EMPTY ((ULONG)0)
#define TX_CLEAR_ID ((ULONG)0)

#define TX_READY ((UINT)0)
#define TX_COMPLETED ((UINT)1)
#define TX_TERMINATED ((UINT)2)
#define TX_SUSPENDED ((UINT)3)
#define TX_SLEEP ((UINT)4)
#define TX_QUEUE_SUSP ((UINT)5)
#define TX_SEMAPHORE_SUSP ((UINT)6)
#define TX_EVENT_FLAG ((UINT)7)
#define TX_BLOCK_MEMORY ((UINT)8)
#define TX_BYTE_MEMORY ((UINT)9)
#define TX_IO_DRIVER ((UINT)10)
#define TX_FILE ((UINT)11)
#define TX_TCP_IP ((UINT)12)
#define TX_MUTEX_SUSP ((UINT)13)
#define TX_PRIORITY_CHANGE ((UINT)14)

#define TX_SUCCESS ((UINT)0x00)
#define TX_DELETED ((UINT)0x01)
#define TX_POOL_ERROR ((UINT)0x02)
#define TX_PTR_ERROR ((UINT)0x03)
#define TX_WAIT_ERROR ((UINT)0x04)
#define TX_SIZE_ERROR ((UINT)0x05)
#define TX_GROUP_ERROR ((UINT)0x06)
#define TX_NO_EVENTS ((UINT)0x07)
#define TX_OPTION_ERROR ((UINT)0x08)
#define TX_QUEUE_ERROR ((UINT)0x09)
#define TX_QUEUE_EMPTY ((UINT)0x0A)
#define TX_QUEUE_FULL ((UINT)0x0B)
#define TX_SEMAPHORE_ERROR ((UINT)0x0C)
#define TX_NO_INSTANCE ((UINT)0x0D)
#define TX_THREAD_ERROR ((UINT)0x0E)
#define TX_PRIORITY_ERROR ((UINT)0x0F)
#define TX_NO_MEMORY ((UINT)0x10)
#define TX_START_ERROR ((UINT)0x10)
#define TX_DELETE_ERROR ((UINT)0x11)
#define TX_RESUME_ERROR ((UINT)0x12)
#define TX_CALLER_ERROR ((UINT)0x13)
#define TX_SUSPEND_ERROR ((UINT)0x14)
#define TX_TIMER_ERROR ((UINT)0x15)
#define TX_TICK_ERROR ((UINT)0x16)
#define TX_ACTIVATE_ERROR ((UINT)0x17)
#define TX_THRESH_ERROR ((UINT)0x18)
#define TX_SUSPEND_LIFTED ((UINT)0x19)
#define TX_WAIT_ABORTED ((UINT)0x1A)
#define TX_WAIT_ABORT_ERROR ((UINT)0x1B)
#define TX_MUTEX_ERROR ((UINT)0x1C)
#define TX_NOT_AVAILABLE ((UINT)0x1D)
#define TX_NOT_OWNED ((UINT)0x1E)
#define TX_INHERIT_ERROR ((UINT)0x1F)
#define TX_NOT_DONE ((UINT)0x20)
#define TX_CEILING_EXCEEDED ((UINT)0x21)
#define TX_INVALID_CEILING ((UINT)0x22)
#define TX_FEATURE_NOT_ENABLED ((UINT)0xFF)

#define tx_thread_identify _tx_thread_identify
#define tx_thread_sleep _tx_thread_sleep

#define tx_event_flags_create _tx_event_flags_create
#define tx_event_flags_delete _tx_event_flags_delete
#define tx_event_flags_get _tx_event_flags_get
#define tx_event_flags_set _tx_event_flags_set

#define tx_mutex_create _tx_mutex_create
#define tx_mutex_delete _tx_mutex_delete
#define tx_mutex_get _tx_mutex_get
#define tx_mutex_put _tx_mutex_put

#define tx_semaphore_create _tx_semaphore_create
#define tx_semaphore_delete _tx_semaphore_delete
#define tx_semaphore_get _tx_semaphore_get
#define tx_semaphore_put _tx_semaphore_put

#define tx_thread_create _tx_thread_create
#define tx_thread_delete _tx_thread_delete
#define tx_thread_info_get _tx_thread_info_get
#define tx_thread_terminate _tx_thread_terminate
#define tx_thread_relinquish _tx_thread_relinquish
#define tx_thread_resume _tx_thread_resume
#define tx_thread_priority_change _tx_thread_priority_change
#define tx_thread_suspend _tx_thread_suspend

#define tx_timer_create _tx_timer_create
#define tx_timer_delete _tx_timer_delete

#define TX_INITIALIZE_IN_PROGRESS ((ULONG)0xF0F0F0F0UL)
#define TX_INITIALIZE_ALMOST_DONE ((ULONG)0xF0F0F0F1UL)
#define TX_INITIALIZE_IS_FINISHED ((ULONG)0x00000000UL)
#define TX_THREAD_GET_SYSTEM_STATE() TX_INITIALIZE_IS_FINISHED

TX_THREAD *_tx_thread_identify(VOID);
UINT _tx_thread_sleep(ULONG timer_ticks);
UINT _tx_event_flags_set(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG flags_to_set,
                                UINT set_option);
UINT _tx_event_flags_delete(TX_EVENT_FLAGS_GROUP *group_ptr);
UINT _tx_event_flags_create(TX_EVENT_FLAGS_GROUP *group_ptr, CHAR *name_ptr);
UINT _tx_event_flags_get(TX_EVENT_FLAGS_GROUP *group_ptr, ULONG requested_flags,
                                UINT get_option, ULONG *actual_flags_ptr, ULONG wait_option);
UINT _tx_mutex_create(TX_MUTEX *mutex_ptr, CHAR *name_ptr, UINT inherit);
UINT _tx_mutex_delete(TX_MUTEX *mutex_ptr);
UINT _tx_mutex_get(TX_MUTEX *mutex_ptr, ULONG wait_option);
UINT _tx_mutex_put(TX_MUTEX *mutex_ptr);

UINT _tx_semaphore_create(TX_SEMAPHORE *semaphore_ptr, CHAR *name_ptr, ULONG initial_count);
UINT _tx_semaphore_delete(TX_SEMAPHORE *semaphore_ptr);
UINT _tx_semaphore_get(TX_SEMAPHORE *semaphore_ptr, ULONG wait_option);
UINT _tx_semaphore_put(TX_SEMAPHORE *semaphore_ptr);

UINT _tx_thread_create(TX_THREAD *thread_ptr, CHAR *name_ptr,
                        VOID (*entry_function)(ULONG entry_input), ULONG entry_input,
                        VOID *stack_start, ULONG stack_size,
                        UINT priority, UINT preempt_threshold,
                        ULONG time_slice, UINT auto_start);
UINT _tx_thread_delete(TX_THREAD *thread_ptr);
UINT _tx_thread_info_get(TX_THREAD *thread_ptr, CHAR **name, UINT *state, ULONG *run_count,
                                UINT *priority, UINT *preemption_threshold, ULONG *time_slice,
                                TX_THREAD **next_thread, TX_THREAD **next_suspended_thread);
UINT _tx_thread_priority_change(TX_THREAD *thread_ptr, UINT new_priority,
                                UINT *old_priority);
VOID _tx_thread_relinquish(VOID);
UINT _tx_thread_resume(TX_THREAD *thread_ptr);
UINT _tx_thread_suspend(TX_THREAD *thread_ptr);
UINT _tx_thread_terminate(TX_THREAD *thread_ptr);

UINT _tx_timer_create(TX_TIMER *timer_ptr, CHAR *name_ptr,
                        VOID (*expiration_function)(ULONG input), ULONG expiration_input,
                        ULONG initial_ticks, ULONG reschedule_ticks, UINT auto_activate);
UINT _tx_timer_delete(TX_TIMER *timer_ptr);

#ifdef __cplusplus
}
#endif

#endif
