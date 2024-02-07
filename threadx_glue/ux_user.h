#pragma once

#include <windows.h>

typedef struct TX_EVENT_FLAGS_GROUP_STRUCT
{
    char *tx_event_flags_group_name;
    ULONG tx_event_flags_group_current;
    ULONG tx_event_flags_group_id;
    CRITICAL_SECTION c;
} TX_EVENT_FLAGS_GROUP;

struct threadx
{
    HANDLE thread;
    DWORD win32_thread_id;

    ULONG tx_thread_id;
    ULONG tx_thread_entry_parameter;
    CHAR *tx_thread_name;
    CHAR tx_thread_state;
    void (*tx_thread_entry)(ULONG id);
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

#define TX_THREAD struct threadx
#define TX_SEMAPHORE struct semaphorex
#define TX_MUTEX struct mutexx
#define TX_TIMER struct timerx

#define UX_PERIODIC_RATE 1000

#ifndef UX_MAX_HCD
// Retail OHCI controller and debug OHCI controller
#define UX_MAX_HCD 2
#endif

#ifndef UX_MAX_DEVICES
// 1 daughterboard + 4 controllers + 4 internal controller hubs + 8 accessories
#define UX_MAX_DEVICES 17
#endif

#ifndef UX_MAX_CLASS_DRIVER
// Plan is to not use any internal calss drivers normally except hub class.
#define UX_MAX_CLASS_DRIVER 1
#endif

#ifndef UX_MAX_ED
// OHCI needs a minimum of 63 EDs (32 + 16 + 8 + 4 + 2 + 1) to build periodic trees
// plus 3 per device (Control + 2 endpoints)
#define UX_MAX_ED (63 + (UX_MAX_DEVICES * 3))
#endif

#ifndef UX_MAX_ISO_TD
// Isochronous transfers; allow for 4 headsets with 2 endpoints each (mic + speaker)
#define UX_MAX_ISO_TD 8
#endif

// Windows has a miminum stack size of 4096 bytes allocated internally, set these low as possible
// to prevent USB stack allocating excessive thread stack memory itself
#define UX_THREAD_STACK_SIZE 1

static inline void ux_test_assert_hit(char *file, int line)
{
    DbgPrint("Assertion failed at %s:%d\n", file, line);
    RtlAssert((PVOID)0, (PVOID)file, line, "ASSERTION FAILED");
}

#define UX_ASSERT_FAIL ux_test_assert_hit(__FILE__, __LINE__);
