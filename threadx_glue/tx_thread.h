#ifndef TX_THREAD_H
#define TX_THREAD_H

#define TX_THREAD_ID ((ULONG)0x54485244)
#define THREAD_DECLARE extern

THREAD_DECLARE TX_THREAD *_tx_thread_current_ptr;

THREAD_DECLARE volatile ULONG _tx_thread_system_state;

// FIXME; actualy stop preempt threads
THREAD_DECLARE volatile UINT _tx_thread_preempt_disable;

VOID _tx_thread_system_resume(TX_THREAD *thread_ptr);
VOID _tx_thread_system_suspend(TX_THREAD *thread_ptr);
VOID _tx_thread_system_preempt_check(VOID);
#endif