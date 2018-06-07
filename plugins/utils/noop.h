#ifndef BABELTRACE_PLUGIN_UTILS_NOOP_H
#define BABELTRACE_PLUGIN_UTILS_NOOP_H

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

BT_HIDDEN
enum bt_notification_iterator_status noop_notif_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *output_priv_port);

BT_HIDDEN
enum bt_notification_iterator_status noop_notif_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count);

BT_HIDDEN
enum bt_component_status noop_port_connected(
		struct bt_private_component *priv_comp,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port);

BT_HIDDEN
enum bt_component_status noop_init(
		struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_data);

#endif /* BABELTRACE_PLUGIN_UTILS_NOOP_H */
