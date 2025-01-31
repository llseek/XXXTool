#include <stdio.h>
#include <ncurses.h>
#include <signal.h>
#include "window.h"
#include "device.h"
#include "context.h"

int requeset_exit = 0;

static void signal_handler(int signal)
{
    requeset_exit = 1;
}

int main(int argc, char *argv[])
{
    int ch = 0;
    int ret = 0;

    struct Context *context = getContext();
    struct DeviceContext *dctx = NULL;
    struct WindowContext *wctx = NULL;

    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGPIPE, signal_handler);

    dctx = AllocDeviceContext();
    InitDeviceContext(dctx);
    context->dctx = dctx;
    wctx = AllocWindowContext();
    context->wctx = wctx;
    InitNcurse(wctx);
    InitWindowContext(wctx);

    while (ch = getch()) {
        if (requeset_exit)
            goto out;
        switch (ch) {
            case 'q':
                goto out;
                break;
            case KEY_RESIZE:
                erase();
                ret = WindowsUpdateUi(wctx);
                if (ret)
                    return ret;
                break;
            case ERR:
                ret = WindowsUpdate(wctx, 0);
                break;
            default:
                ret = WindowsDispatchInput(wctx, ch);
                break;
        }
    }
out:
    FreeWindowContext(wctx);
    FreeDeviceContext(dctx);
    return 0;
}
