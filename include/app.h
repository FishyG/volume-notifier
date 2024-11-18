#ifndef VOLUME_APP
#define VOLUME_APP

#include <libnotify/notification.h>

#include "pipewire/context.h"

#define APP_NAME "VolumeNotifier"

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
    int volume;
    bool muted;

    int uwu;
    int kawaii;
} state_t;

#endif // VOLUME_APP
