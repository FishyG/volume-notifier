#include <getopt.h>
#include <libnotify/notification.h>
#include <libnotify/notify.h>
#include <math.h>
#include <pipewire/pipewire.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glib.h"
#include "pipewire/context.h"
#include "pipewire/core.h"
#include "pipewire/extensions/metadata.h"
#include "pipewire/node.h"
#include "pipewire/proxy.h"
#include "spa/pod/builder.h"
#include "spa/pod/iter.h"
#include "spa/pod/pod.h"
#include "spa/utils/defs.h"
#include "spa/utils/dict.h"
#include "spa/utils/json-pod.h"
#include "spa/utils/string.h"
#include "spa/utils/type.h"

#define APP_NAME "VolumeNotifier"

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

const struct spa_type_info spa_type_param_default[] = {
    {0, SPA_TYPE_OBJECT_ParamProfile, "default", NULL},
    {0, 0, NULL, NULL},
};

typedef struct {
    struct pw_main_loop* loop;
    struct pw_context* context;
    struct pw_core* core;

    struct pw_registry* registry;
    struct spa_hook registry_listener;

    struct pw_metadata* metadata;
    struct spa_hook metadata_listener;

    NotifyNotification* notification;

    struct pw_node* node;
    struct spa_hook node_listener;

    char* default_sink;
    int current_volume;
    bool muted;
    int uwu;
    int kawaii;
} state_t;

static void node_param(void* data, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod* param) {
    if (param->type != SPA_TYPE_Object) return;

    state_t* state = data;

    struct spa_pod_object* object = (struct spa_pod_object*)param;
    const struct spa_pod_prop* prop = spa_pod_object_find_prop(object, spa_pod_prop_first(&object->body), 65544);

    if (prop == NULL || prop->value.type != SPA_TYPE_Array) return;

    struct spa_pod_array* arr = (struct spa_pod_array*)&prop->value;
    struct spa_pod_float* elem = (struct spa_pod_float*)&arr->body.child;

    int volume = round(cbrt(elem->value) * 100);

    float third = 100. / 3;
    prop = spa_pod_object_find_prop(object, spa_pod_prop_first(&object->body), 65540);

    if (prop == NULL || prop->value.type != SPA_TYPE_Bool) return;

    bool muted = ((struct spa_pod_bool*)&prop->value)->value;

    if (volume == state->current_volume && muted == state->muted) return;

    state->current_volume = volume;
    state->muted = muted;

    // Print the percentage (ie: 69%)
    char percentage[20];
    const char* icon;
    const char* body = NULL;
    const char* summary;

    if (state->muted) {
        icon = icons[0];
        if (state->uwu) {
            snprintf(percentage, 15, "(MUwUted) %d%%", volume);
        } else {
            snprintf(percentage, 13, "(Muted) %d%%", volume);
        }
    } else {
        snprintf(percentage, 5, "%d%%", volume);
        icon = icons[(int)ceil(volume / third)];
    }

    if (state->kawaii) {
        int kawaiicount = (int)(sizeof(kawaii_emoticon)/sizeof(kawaii_emoticon[0])/sizeof(kawaii_emoticon[0][0]));
        body = percentage;
        summary = kawaii_emoticon[rand() % kawaiicount];
    } else {
        summary = percentage;
    }

    if (state->notification == NULL || notify_notification_get_closed_reason(state->notification) != -1) {
        // if (state->uwu) {
        state->notification = notify_notification_new(summary, body, icon);
    } else {
        notify_notification_update(state->notification, summary, body, icon);
    }

    notify_notification_set_hint(state->notification, "value", g_variant_new_int32(volume));
    notify_notification_set_timeout(state->notification, 3500);

    GError* error = NULL;
    if (!notify_notification_show(state->notification, &error)) {
        if (error->code != 2) return;
        notify_uninit();
        notify_init(APP_NAME);
    }
}

static const struct pw_node_events node_events = {
    PW_VERSION_NODE_EVENTS,
    .param = node_param,
};

void search_default_sink(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props) {
    state_t* state = data;

    if (state->node != NULL || strcmp(type, PW_TYPE_INTERFACE_Node) != 0) return;

    const char* name = spa_dict_lookup(props, "node.name");

    if (strcmp(name, state->default_sink)) return;

    state->node = pw_registry_bind(state->registry, id, type, PW_VERSION_CLIENT, 0);
    uint32_t param_id = 2;
    pw_node_subscribe_params(state->node, &param_id, 1);
    pw_node_add_listener(state->node, &state->node_listener, &node_events, state);
}

static const struct pw_registry_events registry_sink_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = search_default_sink,
};

int get_default_sink_name(void* data, uint32_t subject, const char* key, const char* type, const char* value) {
    state_t* state = data;

    if (value == NULL) return 0;
    if (spa_strstartswith(key, "default") && strstr(key, "audio.sink") == NULL) return 0;
    if (strcmp(type, "Spa:String:JSON") != 0) return 0;

    struct spa_pod_builder builder;
    uint8_t buff[1024];
    spa_pod_builder_init(&builder, buff, sizeof(buff));

    int offset = spa_json_to_pod(&builder, 0, spa_type_param_default, value, strlen(value));
    if (offset < 0) return 0;

    struct spa_pod_object* object = (struct spa_pod_object*)spa_pod_builder_deref(&builder, offset);

    struct spa_pod_prop* item;
    SPA_POD_OBJECT_FOREACH(object, item) {
        if (item->key != 2) continue;
        char* name = (char*)&item->value + sizeof(struct spa_pod);
        state->default_sink = name;
    }

    // Destroy the existing registry and listener
    pw_proxy_destroy((struct pw_proxy*)state->registry);
    spa_hook_remove(&state->registry_listener);
    if (state->node != NULL) {
        pw_proxy_destroy((struct pw_proxy*)state->node);
        spa_hook_remove(&state->node_listener);
    }
    state->node = NULL;

    // Recreate the registry and re-register the listener
    state->registry = pw_core_get_registry(state->core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(state->registry, &state->registry_listener, &registry_sink_events, state);

    return 0;
}

static const struct pw_metadata_events metadata_events = {
    PW_VERSION_METADATA_EVENTS,
    .property = get_default_sink_name,
};

static void search_default_metadata(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props) {
    state_t* state = data;

    if (state->metadata != NULL || strcmp(type, PW_TYPE_INTERFACE_Metadata) != 0) return;

    const char* value = spa_dict_lookup(props, "metadata.name");

    if (strcmp(value, "default") != 0) return;

    state->metadata = pw_registry_bind(state->registry, id, type, PW_VERSION_METADATA, 0);
    pw_metadata_add_listener(state->metadata, &state->metadata_listener, &metadata_events, state);
}

static const struct pw_registry_events registry_metadata_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = search_default_metadata,
};

int main(int argc, char* argv[]) {
    state_t state;

    spa_zero(state);

    SPA_TYPE_ROOT;

    struct option options[] = {
        {"uwu", no_argument, &state.uwu, 1},
        {"kawaii", no_argument, &state.kawaii, 1},
        {NULL, 0, NULL, 0}
    };

    while (getopt_long(argc, argv, "", options, NULL) != -1);

    notify_init(APP_NAME);
    pw_init(&argc, &argv);

    state.loop = pw_main_loop_new(NULL);
    state.context = pw_context_new(pw_main_loop_get_loop(state.loop), NULL, 0);

    state.core = pw_context_connect(state.context, NULL, 0);

    state.registry = pw_core_get_registry(state.core, PW_VERSION_REGISTRY, 0);

    pw_registry_add_listener(state.registry, &state.registry_listener, &registry_metadata_events, &state);

    pw_main_loop_run(state.loop);

    pw_proxy_destroy((struct pw_proxy*)state.node);
    pw_proxy_destroy((struct pw_proxy*)state.metadata);
    pw_proxy_destroy((struct pw_proxy*)state.registry);
    pw_core_disconnect(state.core);
    pw_context_destroy(state.context);
    pw_main_loop_destroy(state.loop);
    notify_uninit();

    return 0;
}
