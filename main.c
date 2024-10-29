#include <libnotify/notification.h>
#include <math.h>
#include <pipewire/pipewire.h>
#include <stdint.h>
#include <stdlib.h>

#include "glib.h"
#include "pipewire/node.h"
#include "spa/pod/pod.h"
#include "spa/pod/iter.h"
#include "spa/utils/dict.h"

#include <libnotify/notify.h>

char icons[4][20] = {
    "audio-volume-muted",
    "audio-volume-low",
    "audio-volume-medium",
    "audio-volume-high",
};

struct data {
    struct pw_main_loop *loop;
    struct pw_context *context;
    struct pw_core *core;

    struct pw_registry *registry;
    struct spa_hook registry_listener;

    struct pw_node *node;
    struct spa_hook node_listener;

    NotifyNotification* notification;
};

static void node_param(void* _data, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod *param) {
    if (param->type != SPA_TYPE_Object) return;

    struct data* data = _data;

    struct spa_pod_object* object = (struct spa_pod_object*)param;
    struct spa_pod_prop* prop;

    SPA_POD_OBJECT_FOREACH(object, prop) {
        if (prop->key != 65544) continue;
        if (prop->value.type != SPA_TYPE_Array) continue;

        struct spa_pod_array* arr = (struct spa_pod_array*)&prop->value;
        struct spa_pod_float* elem = (struct spa_pod_float*)&arr->body.child;

        int volume = round(cbrt(elem->value) * 100);
        float third = 100./3;
        char* icon = icons[(int)ceil(volume/third)];

        if (data->notification == NULL || notify_notification_get_closed_reason(data->notification) != -1) {
            data->notification = notify_notification_new("Volume", NULL, icon);
        } else {
            notify_notification_update(data->notification, "Volume", NULL, icon);
        }

        notify_notification_set_hint(data->notification, "value", g_variant_new("u", volume));
        notify_notification_set_timeout(data->notification, 10000);
        notify_notification_show(data->notification, NULL);
    }
}

static const struct pw_node_events node_events = {
    PW_VERSION_NODE_EVENTS,
    .param = node_param,
};

static void registry_event_global(void *_data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props) {
    struct data *data = _data;
    if (data->node != NULL)
        return;

    if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0 && id == 46) {
        data->node = pw_registry_bind(data->registry, id, type, PW_VERSION_CLIENT, 0);
        uint32_t id = 2;
        pw_node_subscribe_params(data->node, &id, 1);
        pw_node_add_listener(data->node, &data->node_listener, &node_events, data);
    }
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
};

int main(int argc, char *argv[]) {
    struct data data;

    spa_zero(data);

    notify_init("VolumeNotifier");
    pw_init(&argc, &argv);

    data.loop = pw_main_loop_new(NULL /* properties */);
    data.context = pw_context_new(pw_main_loop_get_loop(data.loop), NULL /* properties */, 0 /* user_data size */);

    data.core = pw_context_connect(data.context, NULL /* properties */, 0 /* user_data size */);

    data.registry = pw_core_get_registry(data.core, PW_VERSION_REGISTRY, 0 /* user_data size */);

    pw_registry_add_listener(data.registry, &data.registry_listener, &registry_events, &data);

    pw_main_loop_run(data.loop);

    pw_proxy_destroy((struct pw_proxy *)data.node);
    pw_proxy_destroy((struct pw_proxy *)data.registry);
    pw_core_disconnect(data.core);
    pw_context_destroy(data.context);
    pw_main_loop_destroy(data.loop);
    notify_uninit();

    return 0;
}
