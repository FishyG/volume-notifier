#include <libnotify/notification.h>
#include <libnotify/notify.h>
#include <math.h>
#include <pipewire/pipewire.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glib.h"
#include "pipewire/context.h"
#include "pipewire/core.h"
#include "pipewire/extensions/metadata.h"
#include "pipewire/node.h"
#include "spa/pod/builder.h"
#include "spa/pod/iter.h"
#include "spa/pod/pod.h"
#include "spa/utils/defs.h"
#include "spa/utils/dict.h"
#include "spa/utils/json-pod.h"
#include "spa/utils/string.h"
#include "spa/utils/type.h"

const char icons[4][20] = {
    "audio-volume-muted",
    "audio-volume-low",
    "audio-volume-medium",
    "audio-volume-high",
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

    struct pw_node* node;
    struct spa_hook node_listener;

    struct pw_metadata* metadata;
    struct spa_hook metadata_listener;

    NotifyNotification* notification;

    char* default_sink;
    bool is_default_configured;
} state_t;

static void node_param(void* _data, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod* param) {
    if (param->type != SPA_TYPE_Object) return;

    state_t* data = _data;

    struct spa_pod_object* object = (struct spa_pod_object*)param;
    struct spa_pod_prop* prop;

    SPA_POD_OBJECT_FOREACH(object, prop) {
        if (prop->key != 65544) continue;
        if (prop->value.type != SPA_TYPE_Array) continue;

        struct spa_pod_array* arr = (struct spa_pod_array*)&prop->value;
        struct spa_pod_float* elem = (struct spa_pod_float*)&arr->body.child;

        int volume = round(cbrt(elem->value) * 100);
        float third = 100. / 3;
        const char* icon = icons[(int)ceil(volume / third)];
        char percentage[5];

        // Print the percentage (ie: 69%)
        snprintf(percentage, 5, "%d%%", volume);

        if (data->notification == NULL || notify_notification_get_closed_reason(data->notification) != -1) {
            data->notification = notify_notification_new(percentage, NULL, icon);
        } else {
            notify_notification_update(data->notification, percentage, NULL, icon);
        }

        notify_notification_set_hint(data->notification, "value", g_variant_new_int32(volume));
        notify_notification_set_timeout(data->notification, 10000);
        notify_notification_show(data->notification, NULL);
    }
}

static const struct pw_node_events node_events = {
    PW_VERSION_NODE_EVENTS,
    .param = node_param,
};

void search_default_sink(void* _data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props) {
    state_t* data = _data;

    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0) return;

    const char* name = spa_dict_lookup(props, "node.name");

    if (strcmp(name, data->default_sink)) return;

    data->node = pw_registry_bind(data->registry, id, type, PW_VERSION_CLIENT, 0);
    uint32_t param_id = 2;
    pw_node_subscribe_params(data->node, &param_id, 1);
    pw_node_add_listener(data->node, &data->node_listener, &node_events, data);
}

static const struct pw_registry_events registry_sink_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = search_default_sink,
};

int metadata_property(void* _data, uint32_t subject, const char* key, const char* type, const char* value) {
    state_t* data = _data;

    if (data->default_sink != NULL) return 0;

    bool is_configured = strstr(key, "configured") != NULL;
    if (spa_strstartswith(key, "default") && strstr(key, "audio.sink") == NULL) return 0;
    if (strcmp(type, "Spa:String:JSON") != 0) return 0;
    if (!is_configured && data->is_default_configured) return 0;

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
        data->default_sink = name;
        data->is_default_configured = is_configured;
    }

    // Destroy the existing registry and listener
    pw_proxy_destroy((struct pw_proxy *)data->registry);
    spa_hook_remove(&data->registry_listener);

    // Recreate the registry and re-register the listener
    data->registry = pw_core_get_registry(data->core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(data->registry, &data->registry_listener, &registry_sink_events, data);

    return 0;
}

static const struct pw_metadata_events metadata_events = {
    PW_VERSION_METADATA_EVENTS,
    .property = metadata_property,
};

static void search_default_metadata(void* _data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props) {
    state_t* data = _data;

    if (data->metadata != NULL || strcmp(type, PW_TYPE_INTERFACE_Metadata) != 0) return;

    const char* value = spa_dict_lookup(props, "metadata.name");

    if (strcmp(value, "default") != 0) return;

    data->metadata = pw_registry_bind(data->registry, id, type, PW_VERSION_METADATA, 0);
    pw_metadata_add_listener(data->metadata, &data->metadata_listener, &metadata_events, data);
}

static const struct pw_registry_events registry_metadata_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = search_default_metadata,
};

int main(int argc, char* argv[]) {
    state_t data;

    spa_zero(data);

    SPA_TYPE_ROOT;

    notify_init("VolumeNotifier");
    pw_init(&argc, &argv);

    data.loop = pw_main_loop_new(NULL);
    data.context = pw_context_new(pw_main_loop_get_loop(data.loop), NULL, 0);

    data.core = pw_context_connect(data.context, NULL, 0);

    data.registry = pw_core_get_registry(data.core, PW_VERSION_REGISTRY, 0);

    pw_registry_add_listener(data.registry, &data.registry_listener, &registry_metadata_events, &data);

    pw_main_loop_run(data.loop);

    pw_proxy_destroy((struct pw_proxy*)data.node);
    pw_proxy_destroy((struct pw_proxy*)data.metadata);
    pw_proxy_destroy((struct pw_proxy*)data.registry);
    pw_core_disconnect(data.core);
    pw_context_destroy(data.context);
    pw_main_loop_destroy(data.loop);
    notify_uninit();

    return 0;
}
