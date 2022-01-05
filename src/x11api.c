#include <gtk/gtk.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>
#include "x11api.h"

#define DEFAULT_MICRO_SLEEP 1

void mask_config(Display *display, int mode)
{
    XIEventMask mask[2];
    XIEventMask *m;
    Window win = DefaultRootWindow(display);

    m = &mask[0];
    m->deviceid = XIAllDevices;
    m->mask_len = XIMaskLen(XI_LASTEVENT);
    m->mask = calloc(m->mask_len, sizeof(char));

    if (mode == MASK_CONFIG_MOUSE)
        XISetMask(m->mask, XI_ButtonPress);
    else if (mode == MASK_CONFIG_KEYBOARD)
        XISetMask(m->mask, XI_KeyPress);

    m = &mask[1];
    m->deviceid = XIAllMasterDevices;
    m->mask_len = XIMaskLen(XI_LASTEVENT);
    m->mask = calloc(m->mask_len, sizeof(char));
    if (mode == MASK_CONFIG_MOUSE)
        XISetMask(m->mask, XI_RawButtonPress);

    XISelectEvents(display, win, &mask[0], 2);
    XSync(display, FALSE);

    free(mask[0].mask);
    free(mask[1].mask);
}

int get_next_key_state(Display *display)
{
    int button = 0;
    XEvent event;
    XGenericEventCookie *cookie = (XGenericEventCookie *)&event.xcookie;
    XNextEvent(display, (XEvent *)&event);

    if (XGetEventData(display, cookie) && cookie->type == GenericEvent)
    {
        XIDeviceEvent *event = cookie->data;
        if (!(event->flags & XIKeyRepeat))
            button = event->detail;
    }

    XFreeEventData(display, cookie);
    return button;
}

void get_cursor_coords(Display *display, int *x, int *y)
{
    XButtonEvent event;
    XQueryPointer(display, DefaultRootWindow(display),
                  &event.root, &event.window,
                  &event.x_root, &event.y_root,
                  &event.x, &event.y,
                  &event.state);
    *x = event.x;
    *y = event.y;
}

void move_to(Display *display, int x, int y)
{
    int cur_x, cur_y;
    get_cursor_coords(display, &cur_x, &cur_y);
    XWarpPointer(display, None, None, 0, 0, 0, 0, -cur_x, -cur_y); // For absolute position
    XWarpPointer(display, None, None, 0, 0, 0, 0, x, y);
    usleep(DEFAULT_MICRO_SLEEP);
}

/**
 * Custom Xevent.
 * Does everything needed for xsendevent.
 */
int cxevent(Display *display, long mask, XButtonEvent event)
{
    if (!XSendEvent(display, PointerWindow, True, mask, (XEvent *)&event))
        return FALSE;
    XFlush(display);
    usleep(DEFAULT_MICRO_SLEEP);
    return TRUE;
}

int click(Display *display, int button, int mode, int duration)
{
    printf("Inside click method\n");
    printf("Mode %d\n", mode);
    switch (mode)
    {
    case CLICK_MODE_XEVENT:
        XButtonEvent event;
        memset(&event, 0, sizeof(event));
        event.button = button;
        event.same_screen = True;
        event.subwindow = DefaultRootWindow(display);

        while (event.subwindow)
        {
            event.window = event.subwindow;
            XQueryPointer(display, event.window,
                          &event.root, &event.subwindow,
                          &event.x_root, &event.y_root,
                          &event.x, &event.y,
                          &event.state);
        }

        // Press
        event.type = ButtonPress;
        if (!cxevent(display, ButtonPressMask, event))
            return FALSE;

        // Needs xevent true to come here?
        printf("Duration %d\n", duration);
        if (duration > 0)
            usleep(duration * 1000); // Used for click and hold

        // Release
        event.type = ButtonRelease;
        if (!cxevent(display, ButtonReleaseMask, event))
            return FALSE;
        break;

    case CLICK_MODE_XTEST:
        XTestFakeButtonEvent(display, button, True, CurrentTime);
        XFlush(display);
        usleep(DEFAULT_MICRO_SLEEP);
        XTestFakeButtonEvent(display, button, False, CurrentTime);
        XFlush(display);
    }

    return TRUE;
}

char *keycode_to_string(Display *display, int keycode)
{
    return XKeysymToString(XkbKeycodeToKeysym(display, keycode, 0, 0));
}

Display *get_display()
{
    return XOpenDisplay(NULL);
}