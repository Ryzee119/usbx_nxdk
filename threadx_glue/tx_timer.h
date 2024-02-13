#ifndef TX_TIMER_H
#define TX_TIMER_H

#ifdef TX_TIMER_INIT
#define TIMER_DECLARE
#else
#define TIMER_DECLARE extern
#endif

TIMER_DECLARE TX_THREAD         _tx_timer_thread;

#endif