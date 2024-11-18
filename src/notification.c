#include "../include/notification.h"

#include "glib.h"
#include <libnotify/notification.h>
#include <libnotify/notify.h>
#include <math.h>

const char icons[4][20] = {
    "audio-volume-muted",
    "audio-volume-low",
    "audio-volume-medium",
    "audio-volume-high",
};

// Feel free to add some more cute emoticons x3
const char kawaii_emoticon[][30] = {
    "≧☉ᆺ☉≦",
    "(*≧ω≦)",
    "(✿^‿^)",
    "(≧◡≦✿)",
    "ʕ •ᴥ•ʔ",
    "(=◑ᆺ◐=)",
    "≽^•⩊•^≼",
    "/ᐠ｡ꞈ｡ᐟ\\",
    "( ˶ˆ꒳ˆ˵ )",
    "(´｡• ᵕ •｡`)",
    "(＾• ω •＾)",
    "(づ｡◕‿‿◕｡)づ",
    "(づ￣ ³￣)づ",
    "(ﾉ^ヮ^)ﾉ*:・ﾟ✧",
    "(˶˃ ᵕ ˂˶) .ᐟ.ᐟ",
    "ヘ(^_^ヘ) ヘ(^o^ヘ)",
};

void notify_state(state_t *state) {
    // Print the percentage (ie: 69%)
    char percentage[20];
    const char* icon;
    const char* body = NULL;
    const char* summary;

    float third = 100. / 3;

    if (state->muted) {
        icon = icons[0];
        if (state->uwu) {
            snprintf(percentage, 15, "(MUwUted) %d%%", state->volume);
        } else {
            snprintf(percentage, 13, "(Muted) %d%%", state->volume);
        }
    } else {
        snprintf(percentage, 5, "%d%%", state->volume);
        icon = icons[(int)ceil(state->volume / third)];
    }

    if (state->kawaii) {
        int kawaiicount = (int)(sizeof(kawaii_emoticon)/sizeof(kawaii_emoticon[0])/sizeof(kawaii_emoticon[0][0]));
        body = percentage;
        summary = kawaii_emoticon[rand() % kawaiicount];
    } else {
        summary = percentage;
    }

    if (state->notification == NULL || notify_notification_get_closed_reason(state->notification) != -1) {
        state->notification = notify_notification_new(summary, body, icon);
    } else {
        notify_notification_update(state->notification, summary, body, icon);
    }

    notify_notification_set_hint(state->notification, "value", g_variant_new_int32(state->volume));
    notify_notification_set_timeout(state->notification, 3500);

    GError* error = NULL;
    if (!notify_notification_show(state->notification, &error)) {
        if (error->code != 2) return;
        notify_uninit();
        notify_init(APP_NAME);
    }
}
