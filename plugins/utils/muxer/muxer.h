#ifndef BABELTRACE_PLUGINS_UTILS_MUXER_H
#define BABELTRACE_PLUGINS_UTILS_MUXER_H

/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>

BT_HIDDEN
enum bt_self_component_status muxer_init(
		bt_self_component_filter *self_comp,
		const bt_value *params, void *init_data);

BT_HIDDEN
void muxer_finalize(bt_self_component_filter *self_comp);

BT_HIDDEN
enum bt_self_notification_iterator_status muxer_notif_iter_init(
		bt_self_notification_iterator *self_notif_iter,
		bt_self_component_filter *self_comp,
		bt_self_component_port_output *self_port);

BT_HIDDEN
void muxer_notif_iter_finalize(
		bt_self_notification_iterator *self_notif_iter);

BT_HIDDEN
enum bt_self_notification_iterator_status muxer_notif_iter_next(
		bt_self_notification_iterator *self_notif_iter,
		bt_notification_array_const notifs, uint64_t capacity,
		uint64_t *count);

BT_HIDDEN
enum bt_self_component_status muxer_input_port_connected(
		bt_self_component_filter *comp,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

BT_HIDDEN
void muxer_input_port_disconnected(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port);

#endif /* BABELTRACE_PLUGINS_UTILS_MUXER_H */
