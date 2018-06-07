#ifndef BABELTRACE_PLUGIN_UTILS_FAKE_H
#define BABELTRACE_PLUGIN_UTILS_FAKE_H

#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

BT_HIDDEN
enum bt_component_status fake_init(struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_method_data);

BT_HIDDEN
void fake_finalize(struct bt_private_component *priv_comp);

BT_HIDDEN
enum bt_notification_iterator_status fake_notif_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *priv_port);

BT_HIDDEN
void fake_notif_iter_finalize(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter);

BT_HIDDEN
enum bt_notification_iterator_status fake_notif_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count);

#endif /* BABELTRACE_PLUGIN_UTILS_FAKE_H */
