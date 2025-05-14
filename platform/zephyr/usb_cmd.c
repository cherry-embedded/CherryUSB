#include <zephyr/shell/shell.h>

#if CONFIG_CHERRYUSB_HOST
#include "usbh_core.h"
static void shell_lsusb_handle(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
    lsusb(argc, argv);
}

SHELL_CMD_REGISTER(lsusb, NULL, "Usage: lsusb [options]...\r\n", shell_lsusb_handle);
#endif