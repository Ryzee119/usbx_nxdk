#pragma once

#include <windows.h>

#ifdef TX_INCLUDE_USER_DEFINE_FILE
#include "tx_user.h"
#endif

#ifndef UX_MAX_HCD
// Only retail OHCI controller
#define UX_MAX_HCD 1
#endif

#ifndef UX_MAX_DEVICES
// 1 daughterboard + 4 controllers + 4 internal controller hubs + 8 accessories
#define UX_MAX_DEVICES 17
#endif

#ifndef UX_MAX_CLASS_DRIVER
// Plan is to not use any internal class drivers normally except hub class.
#define UX_MAX_CLASS_DRIVER 1
#endif

// Maximum number of endpoints.
#ifndef UX_MAX_ED
// OHCI needs a minimum of 63 EDs (32 + 16 + 8 + 4 + 2 + 1) to build periodic trees
// plus 3 per device (Control + 2 endpoints)
#define UX_MAX_ED (63 + (UX_MAX_DEVICES * 3))
#endif

// Maximum number of control, bulk and interrupt transfers to endpoints
#ifndef UX_MAX_TD
#define UX_MAX_TD 32
#endif

// Maximum number of isochronous transfers to endpoints.
#ifndef UX_MAX_ISO_TD
// Isochronous transfers; allow for 4 headsets with 2 endpoints each (mic + speaker)
#define UX_MAX_ISO_TD 8
#endif

// WinAPI allocates its on stack for each thread. Set this a low as possible
// to prevent the USB stack allocating excessive thread stack memory itself
#define UX_THREAD_STACK_SIZE 1

static inline void ux_test_assert_hit(char *file, int line)
{
    DbgPrint("Assertion failed at %s:%d\n", file, line);
    RtlAssert((PVOID)0, (PVOID)file, line, "ASSERTION FAILED");
}

#define UX_ASSERT_FAIL ux_test_assert_hit(__FILE__, __LINE__);
