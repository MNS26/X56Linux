#include <stdio.h>
#include <stdlib.h>
#include "usb.h"

int main() {
    struct usb_ctx *ctx = usb_init_ctx();
    if (!ctx) return 1;
    
    usb_scan_devices(ctx);
    
    if (ctx->devices[0] && ctx->devices[0]->active) {
        printf("Setting RGB red on device 1...\n");
        int ret = usb_set_rgb(ctx->devices[0], 255, 0, 0);
        printf("Result: %d\n", ret);
    }
    
    usb_free_ctx(ctx);
    return 0;
}