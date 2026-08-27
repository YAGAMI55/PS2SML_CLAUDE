#include <tamtypes.h>
#include <kernel.h>
#include <loadfile.h>
#include <libpad.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <delaythread.h>

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "graphics.h"
#include "font.h"

#define PAD_PORT        0
#define PAD_SLOT        0
#define PAD_BUFFER_SIZE 256

static unsigned char pad_buffer[PAD_BUFFER_SIZE] __attribute__((aligned(64)));
static struct padButtonStatus padData;
static int selected = 0;
static int show_description = 0;
static u32 previous_buttons = 0;

static void fatal_error(const char *message) {
    printf("%s\n", message);
    while (1)
        SleepThread();
}

static int init_iop(void) {
    int retries = 0;
    SifInitRpc(0);
    while (!SifIopReset(NULL, 0)) {
        if (++retries > 200000)
            return 0;
    }
    retries = 0;
    while (!SifIopSync()) {
        if (++retries > 400000)
            return 0;
    }

    if (SifLoadModule("rom0:SIO2MAN", 0, NULL) < 0)
        return 0;
    if (SifLoadModule("rom0:PADMAN", 0, NULL) < 0)
        return 0;

    return 1;
}

static int wait_pad_ready(int port, int slot) {
    int state;
    int timeout = 5000;
    state = padGetState(port, slot);
    while (state != PAD_STATE_STABLE &&
           state != PAD_STATE_FINDCTP1) {
        if (--timeout <= 0)
            return 0;
        DelayThread(1000);
        state = padGetState(port, slot);
    }
    return 1;
}

static int init_pad(void) {
    if (!padInit(0))
        return 0;
    if (!padPortOpen(PAD_PORT, PAD_SLOT, pad_buffer))
        return 0;
    if (!wait_pad_ready(PAD_PORT, PAD_SLOT))
        return 0;
    return 1;
}

static void reset_input_state(void) {
    memset(&padData, 0, sizeof(padData));
    previous_buttons = 0;
}

static u32 read_buttons(void) {
    if (!padRead(PAD_PORT, PAD_SLOT, &padData))
        return 0;
    /*
     * PS2 libpad button bits are active-low.
     */
    return ~((u32)padData.btns);
}

static void launch_elf(const char *path) {
    t_ExecData elfData;
    memset(&elfData, 0, sizeof(elfData));
    printf("Loading ELF: %s\n", path);
    padPortClose(PAD_PORT, PAD_SLOT);
    padEnd();
    cleanup_font();
    cleanup_graphics();
    int result = SifLoadElf(path, &elfData);
    if (result != 0) {
        printf("ERROR: SifLoadElf(%s) failed: %d\n", path, result);
        fatal_error("ELF load failed.");
    }
    if (elfData.epc == 0) {
        printf("ERROR: SifLoadElf(%s) returned zero entry point "
               "(epc=0x%08X, gp=0x%08X). File likely missing/misnamed on disc.\n",
               path, (unsigned int)elfData.epc, (unsigned int)elfData.gp);
        fatal_error("ELF entry point invalid.");
    }
    FlushCache(0);
    FlushCache(2);
    {
        char *argv[1];
        argv[0] = (char *)path;
        ExecPS2((void *)elfData.epc,
                (void *)elfData.gp,
                1,
                argv);
    }
    fatal_error("ERROR: ExecPS2 returned.");
}

static void handle_input(void) {
    u32 buttons = read_buttons();
    u32 pressed = buttons & ~previous_buttons;
    if (!show_description && config.item_count > 0) {
        if (pressed & PAD_UP) {
            --selected;
            if (selected < 0)
                selected = config.item_count - 1;
        }
        if (pressed & PAD_DOWN) {
            ++selected;
            if (selected >= config.item_count)
                selected = 0;
        }
    }
    if (pressed & PAD_TRIANGLE)
        show_description = !show_description;
    if ((pressed & PAD_CROSS) || (pressed & PAD_CIRCLE)) {
        if (config.item_count > 0)
            launch_elf(config.items[selected].path);
    }
    previous_buttons = buttons;
}

int main(void) {
    reset_input_state();
    printf("CHECKPOINT 0: main() started\n");

    if (!init_iop())
        fatal_error("ERROR: IOP initialization failed.");
    printf("CHECKPOINT 1: init_iop() OK\n");

    if (!init_pad())
        fatal_error("ERROR: controller initialization failed.");
    printf("CHECKPOINT 2: init_pad() OK\n");

    if (!load_config())
        fatal_error("ERROR: cdrom0:\\PS2SML\\conf.cfg not found.");
    printf("CHECKPOINT 3: load_config() OK, item_count=%d bg_path=%s font_path=%s\n",
           config.item_count, config.bg_path, config.font_path);

    if (!init_graphics(config.video_mode))
        fatal_error("ERROR: graphics initialization failed.");
    printf("CHECKPOINT 4: init_graphics() OK\n");

    if (!init_font(config.font_path))
        fatal_error("ERROR: font loading failed.");
    printf("CHECKPOINT 5: init_font() OK - entering main loop\n");

    while (1) {
        handle_input();
        draw_menu(selected, show_description);
    }
    return 0;
}
