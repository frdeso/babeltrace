/*
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/babeltrace.h>

#include "noop.h"

static struct bt_notification_iterator *g_iter;

BT_HIDDEN
enum bt_notification_iterator_status noop_notif_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *output_priv_port)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

BT_HIDDEN
enum bt_notification_iterator_status noop_notif_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_notification **out_notifs, uint64_t out_capacity,
		uint64_t *out_count)
{
	enum bt_notification_iterator_status status;
	struct bt_notification **in_notifs;
	uint64_t in_count;

	status = bt_private_connection_notification_iterator_next(g_iter,
		&in_notifs, &in_count);

	if (status == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		memcpy(out_notifs, in_notifs, sizeof(*in_notifs) * in_count);
		*out_count = in_count;
	}

	return status;
}

BT_HIDDEN
enum bt_component_status noop_port_connected(
		struct bt_private_component *priv_comp,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port)
{
	struct bt_port *self_port =
		bt_port_borrow_from_private(self_private_port);

	if (bt_port_is_output(self_port)) {
		return BT_COMPONENT_STATUS_OK;
	}

	struct bt_private_connection *priv_conn =
		bt_private_port_get_private_connection(self_private_port);

	bt_private_connection_create_notification_iterator(priv_conn, &g_iter);
	return BT_COMPONENT_STATUS_OK;
}

BT_HIDDEN
enum bt_component_status noop_init(
		struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_data)
{
	bt_private_component_filter_add_input_private_port(priv_comp,
		"in", NULL, NULL);
	bt_private_component_filter_add_output_private_port(priv_comp,
		"out", NULL, NULL);
	return BT_COMPONENT_STATUS_OK;
}
