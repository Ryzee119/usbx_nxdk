#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>

#include "ux_api.h"
#include "ux_system.h"
#include "ux_utility.h"
#include "ux_host_class_hub.h"
#include "ux_hcd_ohci.h"

int main(void)
{
    XVideoSetMode(720, 480, 16, REFRESH_DEFAULT);
    nxUsbInit();

    while (1)
    {
        Sleep(1000);
    }

    nxUsbShutdown();
    return 0;
}
