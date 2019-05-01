#ifndef BABELTRACE_GRAPH_NOTIFICATION_STREAM_H
#define BABELTRACE_GRAPH_NOTIFICATION_STREAM_H

/*
 * BabelTrace - Plug-in Stream-related Notifications
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_get() */
#include <babeltrace/ref.h>

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;
struct bt_private_connection_private_notification_iterator;
struct bt_clock_class;
struct bt_clock_value;
struct bt_stream;

extern
struct bt_notification *bt_notification_stream_begin_create(
		struct bt_private_connection_private_notification_iterator *notification_iterator,
		struct bt_stream *stream);

extern
struct bt_notification *bt_notification_stream_end_create(
		struct bt_private_connection_private_notification_iterator *notification_iterator,
		struct bt_stream *stream);

extern struct bt_stream *bt_notification_stream_begin_borrow_stream(
		struct bt_notification *notification);

static inline
struct bt_stream *bt_notification_stream_begin_get_stream(
		struct bt_notification *notification)
{
	return bt_get(bt_notification_stream_begin_borrow_stream(notification));
}

extern int bt_notification_stream_begin_set_clock_value(
		struct bt_notification *notif,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default);

extern struct bt_clock_value *bt_notification_stream_begin_borrow_default_clock_value(
		struct bt_notification *notif);

extern struct bt_stream *bt_notification_stream_end_borrow_stream(
		struct bt_notification *notification);

static inline
struct bt_stream *bt_notification_stream_end_get_stream(
		struct bt_notification *notification)
{
	return bt_get(bt_notification_stream_end_borrow_stream(notification));
}

extern int bt_notification_stream_end_set_clock_value(
		struct bt_notification *notif,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default);

extern struct bt_clock_value *bt_notification_stream_end_borrow_default_clock_value(
		struct bt_notification *notif);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_NOTIFICATION_STREAM_H */
