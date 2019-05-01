/*
 * Babeltrace - CTF notification iterator
 *
 * Copyright (c) 2015-2016 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2016 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-NOTIF-ITER"
#include "logging.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include <string.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/common-internal.h>
#include <glib.h>
#include <stdlib.h>

#include "notif-iter.h"
#include "../btr/btr.h"

struct bt_notif_iter;

/* A visit stack entry */
struct stack_entry {
	/*
	 * Current base field, one of:
	 *
	 *   * string
	 *   * structure
	 *   * array
	 *   * sequence
	 *   * variant
	 *
	 * Field is borrowed.
	 */
	struct bt_field *base;

	/* index of next field to set */
	size_t index;
};

/* Visit stack */
struct stack {
	/* Entries (struct stack_entry) */
	GArray *entries;

	/* Number of active entries */
	size_t size;
};

/* State */
enum state {
	STATE_INIT,
	STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN,
	STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE,
	STATE_AFTER_TRACE_PACKET_HEADER,
	STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN,
	STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE,
	STATE_AFTER_STREAM_PACKET_CONTEXT,
	STATE_EMIT_NOTIF_NEW_STREAM,
	STATE_EMIT_NOTIF_NEW_PACKET,
	STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN,
	STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE,
	STATE_AFTER_STREAM_EVENT_HEADER,
	STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN,
	STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE,
	STATE_DSCOPE_EVENT_CONTEXT_BEGIN,
	STATE_DSCOPE_EVENT_CONTEXT_CONTINUE,
	STATE_DSCOPE_EVENT_PAYLOAD_BEGIN,
	STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE,
	STATE_EMIT_NOTIF_EVENT,
	STATE_EMIT_NOTIF_END_OF_PACKET,
	STATE_DONE,
	STATE_SKIP_PACKET_PADDING,
};

struct trace_field_path_cache {
	/*
	 * Indexes of the stream_id and stream_instance_id field in the packet
	 * header structure, -1 if unset.
	 */
	int stream_id;
	int stream_instance_id;
};

struct stream_class_field_path_cache {
	/*
	 * Indexes of the v and id fields in the stream event header structure,
	 * -1 if unset.
	 */
	int v;
	int id;

	/*
	 * index of the timestamp_end, packet_size and content_size fields in
	 * the stream packet context structure. Set to -1 if the fields were
	 * not found.
	 */
	int timestamp_end;
	int packet_size;
	int content_size;
};

struct field_cb_override {
	enum bt_btr_status (* func)(void *value,
			struct bt_field_type *type, void *data);
	void *data;
};

/* Clock value: clock class and raw value */
struct clock_value {
	struct bt_clock_class *clock_class; /* Weak */
	uint64_t raw_value;
};

/* CTF notification iterator */
struct bt_notif_iter {
	/* Visit stack */
	struct stack *stack;

	/* Current notification iterator to create notifications (weak) */
	struct bt_private_connection_private_notification_iterator *notif_iter;

	/*
	 * Current dynamic scope field pointer.
	 *
	 * This is set by read_dscope_begin_state() and contains the
	 * value of one of the pointers in `dscopes` below.
	 */
	struct bt_field *cur_dscope_field;

	/* Trace and classes (owned by this) */
	struct {
		struct bt_trace *trace;
		struct bt_stream_class *stream_class;
		struct bt_event_class *event_class;
	} meta;

	/* Current packet header field wrapper (NULL if not created yet) */
	struct bt_packet_header_field *packet_header_field;

	/* Current packet header field wrapper (NULL if not created yet) */
	struct bt_packet_context_field *packet_context_field;

	/* Current event header field (NULL if not created yet) */
	struct bt_event_header_field *event_header_field;

	/* Current packet (NULL if not created yet) */
	struct bt_packet *packet;

	/* Previous packet availability */
	enum bt_packet_previous_packet_availability prev_packet_avail;

	/* Previous packet (NULL if not available) */
	struct bt_packet *prev_packet;

	/* Current stream (NULL if not set yet) */
	struct bt_stream *stream;

	/* Current event (NULL if not created yet) */
	struct bt_event *event;

	/* Current event notification (NULL if not created yet) */
	struct bt_notification *event_notif;

	/*
	 * Current `timestamp_end` field (to consider before switching
	 * packets). If it's set, it's a field which is within
	 * `dscopes.stream_packet_context` below, which is in `packet`
	 * above.
	 */
	struct bt_field *cur_timestamp_end;

	/* Database of current dynamic scopes (owned by this) */
	struct {
		struct bt_field *trace_packet_header;
		struct bt_field *stream_packet_context;
		struct bt_field *stream_event_header;
		struct bt_field *stream_event_context;
		struct bt_field *event_context;
		struct bt_field *event_payload;
	} dscopes;

	/*
	 * Special field overrides.
	 *
	 * Overrides are used to implement the behaviours of special fields such
	 * as "timestamp_end" (which must be ignored until the end of the
	 * packet), "id" (event id) which can be present multiple times and must
	 * be updated multiple time.
	 *
	 * This should be used to implement the behaviour of integer fields
	 * mapped to clocks and other "tagged" fields (in CTF 2).
	 *
	 * bt_field_type to struct field_cb_override
	 */
	GHashTable *field_overrides;

	/* Current state */
	enum state state;

	/* Current medium buffer data */
	struct {
		/* Last address provided by medium */
		const uint8_t *addr;

		/* Buffer size provided by medium (bytes) */
		size_t sz;

		/* Offset within whole packet of addr (bits) */
		size_t packet_offset;

		/* Current position from addr (bits) */
		size_t at;

		/* Position of the last event header from addr (bits) */
		size_t last_eh_at;
	} buf;

	/* Binary type reader */
	struct bt_btr *btr;

	/* Current medium data */
	struct {
		struct bt_notif_iter_medium_ops medops;
		size_t max_request_sz;
		void *data;
	} medium;

	/* Stream beginning was emitted */
	bool stream_begin_emitted;

	/* Current packet size (bits) (-1 if unknown) */
	int64_t cur_packet_size;

	/* Current content size (bits) (-1 if unknown) */
	int64_t cur_content_size;

	/*
	 * Offset, in the underlying media, of the current packet's start
	 * (-1 if unknown).
	 */
	off_t cur_packet_offset;

	/* bt_clock_class to uint64_t. */
	GHashTable *clock_states;

	/*
	 * Cache of the trace-constant field paths (event header type)
	 * associated to the current trace.
	 */
	struct trace_field_path_cache trace_field_path_cache;

	/*
	 * Field path cache associated with the current stream class.
	 * Ownership of this structure belongs to the field_path_caches HT.
	 */
	struct stream_class_field_path_cache *cur_sc_field_path_cache;

	/* bt_stream_class to struct stream_class_field_path_cache. */
	GHashTable *sc_field_path_caches;
};

static inline
const char *state_string(enum state state)
{
	switch (state) {
	case STATE_INIT:
		return "STATE_INIT";
	case STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN:
		return "STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN";
	case STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE:
		return "STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE";
	case STATE_AFTER_TRACE_PACKET_HEADER:
		return "STATE_AFTER_TRACE_PACKET_HEADER";
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN:
		return "STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN";
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE:
		return "STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE";
	case STATE_AFTER_STREAM_PACKET_CONTEXT:
		return "STATE_AFTER_STREAM_PACKET_CONTEXT";
	case STATE_EMIT_NOTIF_NEW_PACKET:
		return "STATE_EMIT_NOTIF_NEW_PACKET";
	case STATE_EMIT_NOTIF_NEW_STREAM:
		return "STATE_EMIT_NOTIF_NEW_STREAM";
	case STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN:
		return "STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN";
	case STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE:
		return "STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE";
	case STATE_AFTER_STREAM_EVENT_HEADER:
		return "STATE_AFTER_STREAM_EVENT_HEADER";
	case STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN:
		return "STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN";
	case STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE:
		return "STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE";
	case STATE_DSCOPE_EVENT_CONTEXT_BEGIN:
		return "STATE_DSCOPE_EVENT_CONTEXT_BEGIN";
	case STATE_DSCOPE_EVENT_CONTEXT_CONTINUE:
		return "STATE_DSCOPE_EVENT_CONTEXT_CONTINUE";
	case STATE_DSCOPE_EVENT_PAYLOAD_BEGIN:
		return "STATE_DSCOPE_EVENT_PAYLOAD_BEGIN";
	case STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE:
		return "STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE";
	case STATE_EMIT_NOTIF_EVENT:
		return "STATE_EMIT_NOTIF_EVENT";
	case STATE_EMIT_NOTIF_END_OF_PACKET:
		return "STATE_EMIT_NOTIF_END_OF_PACKET";
	case STATE_DONE:
		return "STATE_DONE";
	case STATE_SKIP_PACKET_PADDING:
		return "STATE_SKIP_PACKET_PADDING";
	default:
		return "(unknown)";
	}
}

static
int bt_notif_iter_switch_packet(struct bt_notif_iter *notit);

static
enum bt_btr_status btr_timestamp_end_cb(void *value,
		struct bt_field_type *type, void *data);

static
struct stack *stack_new(struct bt_notif_iter *notit)
{
	struct stack *stack = NULL;

	stack = g_new0(struct stack, 1);
	if (!stack) {
		BT_LOGE_STR("Failed to allocate one stack.");
		goto error;
	}

	stack->entries = g_array_new(FALSE, TRUE, sizeof(struct stack_entry));
	if (!stack->entries) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	BT_LOGD("Created stack: notit-addr=%p, stack-addr=%p", notit, stack);
	return stack;

error:
	g_free(stack);
	return NULL;
}

static
void stack_destroy(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_LOGD("Destroying stack: addr=%p", stack);

	if (stack->entries) {
		g_array_free(stack->entries, TRUE);
	}

	g_free(stack);
}

static
int stack_push(struct stack *stack, struct bt_field *base)
{
	struct stack_entry *entry;

	BT_ASSERT(stack);
	BT_ASSERT(base);
	BT_LOGV("Pushing base field on stack: stack-addr=%p, "
		"stack-size-before=%zu, stack-size-after=%zu",
		stack, stack->size, stack->size + 1);

	if (stack->entries->len == stack->size) {
		g_array_set_size(stack->entries, stack->size + 1);
	}

	entry = &g_array_index(stack->entries, struct stack_entry, stack->size);
	entry->base = base;
	entry->index = 0;
	stack->size++;
	return 0;
}

static inline
unsigned int stack_size(struct stack *stack)
{
	BT_ASSERT(stack);
	return stack->size;
}

static
void stack_pop(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_ASSERT(stack_size(stack));
	BT_LOGV("Popping from stack: "
		"stack-addr=%p, stack-size-before=%zu, stack-size-after=%zu",
		stack, stack->size, stack->size - 1);
	stack->size--;
}

static inline
struct stack_entry *stack_top(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_ASSERT(stack_size(stack));
	return &g_array_index(stack->entries, struct stack_entry,
		stack->size - 1);
}

static inline
bool stack_empty(struct stack *stack)
{
	return stack_size(stack) == 0;
}

static
void stack_clear(struct stack *stack)
{
	BT_ASSERT(stack);
	stack->size = 0;
}

static inline
enum bt_notif_iter_status notif_iter_status_from_m_status(
		enum bt_notif_iter_medium_status m_status)
{
	return (int) m_status;
}

static inline
size_t buf_size_bits(struct bt_notif_iter *notit)
{
	return notit->buf.sz * 8;
}

static inline
size_t buf_available_bits(struct bt_notif_iter *notit)
{
	return buf_size_bits(notit) - notit->buf.at;
}

static inline
size_t packet_at(struct bt_notif_iter *notit)
{
	return notit->buf.packet_offset + notit->buf.at;
}

static inline
void buf_consume_bits(struct bt_notif_iter *notit, size_t incr)
{
	BT_LOGV("Advancing cursor: notit-addr=%p, cur-before=%zu, cur-after=%zu",
		notit, notit->buf.at, notit->buf.at + incr);
	notit->buf.at += incr;
}

static
enum bt_notif_iter_status request_medium_bytes(
		struct bt_notif_iter *notit)
{
	uint8_t *buffer_addr = NULL;
	size_t buffer_sz = 0;
	enum bt_notif_iter_medium_status m_status;

	BT_LOGV("Calling user function (request bytes): notit-addr=%p, "
		"request-size=%zu", notit, notit->medium.max_request_sz);
	m_status = notit->medium.medops.request_bytes(
		notit->medium.max_request_sz, &buffer_addr,
		&buffer_sz, notit->medium.data);
	BT_LOGV("User function returned: status=%s, buf-addr=%p, buf-size=%zu",
		bt_notif_iter_medium_status_string(m_status),
		buffer_addr, buffer_sz);
	if (m_status == BT_NOTIF_ITER_MEDIUM_STATUS_OK) {
		BT_ASSERT(buffer_sz != 0);

		/* New packet offset is old one + old size (in bits) */
		notit->buf.packet_offset += buf_size_bits(notit);

		/* Restart at the beginning of the new medium buffer */
		notit->buf.at = 0;
		notit->buf.last_eh_at = SIZE_MAX;

		/* New medium buffer size */
		notit->buf.sz = buffer_sz;

		/* New medium buffer address */
		notit->buf.addr = buffer_addr;

		BT_LOGV("User function returned new bytes: "
			"packet-offset=%zu, cur=%zu, size=%zu, addr=%p",
			notit->buf.packet_offset, notit->buf.at,
			notit->buf.sz, notit->buf.addr);
		BT_LOGV_MEM(buffer_addr, buffer_sz, "Returned bytes at %p:",
			buffer_addr);
	} else if (m_status == BT_NOTIF_ITER_MEDIUM_STATUS_EOF) {
		/*
		 * User returned end of stream: validate that we're not
		 * in the middle of a packet header, packet context, or
		 * event.
		 */
		if (notit->cur_packet_size >= 0) {
			if (packet_at(notit) == notit->cur_packet_size) {
				goto end;
			}
		} else {
			if (packet_at(notit) == 0) {
				goto end;
			}

			if (notit->buf.last_eh_at != SIZE_MAX &&
					notit->buf.at == notit->buf.last_eh_at) {
				goto end;
			}
		}

		/* All other states are invalid */
		BT_LOGW("User function returned %s, but notification iterator is in an unexpected state: "
			"state=%s, cur-packet-size=%" PRId64 ", cur=%zu, "
			"packet-cur=%zu, last-eh-at=%zu",
			bt_notif_iter_medium_status_string(m_status),
			state_string(notit->state),
			notit->cur_packet_size,
			notit->buf.at, packet_at(notit),
			notit->buf.last_eh_at);
		m_status = BT_NOTIF_ITER_MEDIUM_STATUS_ERROR;
	} else if (m_status < 0) {
		BT_LOGW("User function failed: status=%s",
			bt_notif_iter_medium_status_string(m_status));
	}

end:
	return notif_iter_status_from_m_status(m_status);
}

static inline
enum bt_notif_iter_status buf_ensure_available_bits(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;

	if (buf_available_bits(notit) == 0) {
		/*
		 * This _cannot_ return BT_NOTIF_ITER_STATUS_OK
		 * _and_ no bits.
		 */
		status = request_medium_bytes(notit);
	}

	return status;
}

static inline
void reset_clock_value(struct clock_value *cv)
{
	BT_ASSERT(cv);
	cv->clock_class = NULL;
	cv->raw_value = UINT64_C(-1);
}

static
enum bt_notif_iter_status read_dscope_begin_state(
		struct bt_notif_iter *notit,
		struct bt_field_type *dscope_field_type,
		enum state done_state, enum state continue_state,
		struct bt_field *dscope_field)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	enum bt_btr_status btr_status;
	size_t consumed_bits;

	notit->cur_dscope_field = dscope_field;
	BT_LOGV("Starting BTR: notit-addr=%p, btr-addr=%p, ft-addr=%p",
		notit, notit->btr, dscope_field_type);
	consumed_bits = bt_btr_start(notit->btr, dscope_field_type,
		notit->buf.addr, notit->buf.at, packet_at(notit),
		notit->buf.sz, &btr_status);
	BT_LOGV("BTR consumed bits: size=%zu", consumed_bits);

	switch (btr_status) {
	case BT_BTR_STATUS_OK:
		/* type was read completely */
		BT_LOGV_STR("Field was completely decoded.");
		notit->state = done_state;
		break;
	case BT_BTR_STATUS_EOF:
		BT_LOGV_STR("BTR needs more data to decode field completely.");
		notit->state = continue_state;
		break;
	default:
		BT_LOGW("BTR failed to start: notit-addr=%p, btr-addr=%p, "
			"status=%s", notit, notit->btr,
			bt_btr_status_string(btr_status));
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	/* Consume bits now since we know we're not in an error state */
	buf_consume_bits(notit, consumed_bits);

end:
	return status;
}

static
enum bt_notif_iter_status read_dscope_continue_state(
		struct bt_notif_iter *notit, enum state done_state)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	enum bt_btr_status btr_status;
	size_t consumed_bits;

	BT_LOGV("Continuing BTR: notit-addr=%p, btr-addr=%p",
		notit, notit->btr);

	status = buf_ensure_available_bits(notit);
	if (status != BT_NOTIF_ITER_STATUS_OK) {
		if (status < 0) {
			BT_LOGW("Cannot ensure that buffer has at least one byte: "
				"notif-addr=%p, status=%s",
				notit, bt_notif_iter_status_string(status));
		} else {
			BT_LOGV("Cannot ensure that buffer has at least one byte: "
				"notif-addr=%p, status=%s",
				notit, bt_notif_iter_status_string(status));
		}

		goto end;
	}


	consumed_bits = bt_btr_continue(notit->btr, notit->buf.addr,
		notit->buf.sz, &btr_status);
	BT_LOGV("BTR consumed bits: size=%zu", consumed_bits);

	switch (btr_status) {
	case BT_BTR_STATUS_OK:
		/* Type was read completely. */
		BT_LOGV_STR("Field was completely decoded.");
		notit->state = done_state;
		break;
	case BT_BTR_STATUS_EOF:
		/* Stay in this continue state. */
		BT_LOGV_STR("BTR needs more data to decode field completely.");
		break;
	default:
		BT_LOGW("BTR failed to continue: notit-addr=%p, btr-addr=%p, "
			"status=%s", notit, notit->btr,
			bt_btr_status_string(btr_status));
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	/* Consume bits now since we know we're not in an error state. */
	buf_consume_bits(notit, consumed_bits);
end:
	return status;
}

static
void release_event_dscopes(struct bt_notif_iter *notit)
{
	notit->dscopes.stream_event_header = NULL;

	if (notit->event_header_field) {
		bt_event_header_field_release(notit->event_header_field);
		notit->event_header_field = NULL;
	}

	notit->dscopes.stream_event_context = NULL;
	notit->dscopes.event_context = NULL;
	notit->dscopes.event_payload = NULL;
}

static
void release_all_dscopes(struct bt_notif_iter *notit)
{
	notit->dscopes.trace_packet_header = NULL;

	if (notit->packet_header_field) {
		bt_packet_header_field_release(notit->packet_header_field);
		notit->packet_header_field = NULL;
	}

	notit->dscopes.stream_packet_context = NULL;

	if (notit->packet_context_field) {
		bt_packet_context_field_release(notit->packet_context_field);
		notit->packet_context_field = NULL;
	}

	release_event_dscopes(notit);
}

static
enum bt_notif_iter_status read_packet_header_begin_state(
		struct bt_notif_iter *notit)
{
	struct bt_field_type *packet_header_type = NULL;
	enum bt_notif_iter_status ret = BT_NOTIF_ITER_STATUS_OK;

	if (bt_notif_iter_switch_packet(notit)) {
		BT_LOGW("Cannot switch packet: notit-addr=%p", notit);
		ret = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	/* Packet header type is common to the whole trace. */
	packet_header_type = bt_trace_borrow_packet_header_field_type(
			notit->meta.trace);
	if (!packet_header_type) {
		notit->state = STATE_AFTER_TRACE_PACKET_HEADER;
		goto end;
	}

	/*
	 * Create free packet header field from trace. This field is
	 * going to be moved to the packet once we create it. We cannot
	 * create the packet now because:
	 *
	 * 1. A packet is created from a stream.
	 * 2. A stream is created from a stream class.
	 * 3. We need the packet header field's content to know the ID
	 *    of the stream class to select.
	 */
	BT_ASSERT(!notit->packet_header_field);
	notit->packet_header_field = bt_trace_create_packet_header_field(
		notit->meta.trace);
	if (!notit->packet_header_field) {
		BT_LOGE_STR("Cannot create packet header field wrapper from trace.");
		ret = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	notit->dscopes.trace_packet_header =
		bt_packet_header_field_borrow_field(notit->packet_header_field);
	BT_ASSERT(notit->dscopes.trace_packet_header);
	BT_LOGV("Decoding packet header field:"
		"notit-addr=%p, trace-addr=%p, trace-name=\"%s\", ft-addr=%p",
		notit, notit->meta.trace,
		bt_trace_get_name(notit->meta.trace), packet_header_type);
	ret = read_dscope_begin_state(notit, packet_header_type,
		STATE_AFTER_TRACE_PACKET_HEADER,
		STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE,
		notit->dscopes.trace_packet_header);
	if (ret < 0) {
		BT_LOGW("Cannot decode packet header field: "
			"notit-addr=%p, trace-addr=%p, "
			"trace-name=\"%s\", ft-addr=%p",
			notit, notit->meta.trace,
			bt_trace_get_name(notit->meta.trace),
			packet_header_type);
	}

end:
	return ret;
}

static
enum bt_notif_iter_status read_packet_header_continue_state(
		struct bt_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
			STATE_AFTER_TRACE_PACKET_HEADER);
}

static
struct stream_class_field_path_cache *
create_stream_class_field_path_cache_entry(
		struct bt_notif_iter *notit,
		struct bt_stream_class *stream_class)
{
	int v = -1;
	int id = -1;
	int timestamp_end = -1;
	int packet_size = -1;
	int content_size = -1;
	struct stream_class_field_path_cache *cache_entry = g_new0(
			struct stream_class_field_path_cache, 1);
	struct bt_field_type *event_header = NULL, *packet_context = NULL;

	if (!cache_entry) {
		BT_LOGE_STR("Failed to allocate one stream class field path cache.");
		goto end;
	}

	event_header = bt_stream_class_borrow_event_header_field_type(
		stream_class);
	if (event_header && bt_field_type_is_structure(event_header)) {
		int i, count;

		count = bt_field_type_structure_get_field_count(
			event_header);
		BT_ASSERT(count >= 0);

		for (i = 0; i < count; i++) {
			int ret;
			const char *name;

			ret = bt_field_type_structure_borrow_field_by_index(
					event_header, &name, NULL, i);
			if (ret) {
				BT_LOGE("Cannot get event header structure field type's field: "
					"notit-addr=%p, stream-class-addr=%p, "
					"stream-class-name=\"%s\", "
					"stream-class-id=%" PRId64 ", "
					"ft-addr=%p, index=%d",
					notit, stream_class,
					bt_stream_class_get_name(stream_class),
					bt_stream_class_get_id(stream_class),
					event_header, i);
				goto error;
			}

			if (v != -1 && id != -1) {
				break;
			}

			if (v == -1 && strcmp(name, "v") == 0) {
				v = i;
			} else if (id == -1 && !strcmp(name, "id")) {
				id = i;
			}
		}
	}

	packet_context = bt_stream_class_borrow_packet_context_field_type(
			stream_class);
	if (packet_context && bt_field_type_is_structure(packet_context)) {
		int i, count;

		count = bt_field_type_structure_get_field_count(
			packet_context);
		BT_ASSERT(count >= 0);

		for (i = 0; i < count; i++) {
			int ret;
			const char *name;
			struct bt_field_type *field_type;

			if (timestamp_end != -1 && packet_size != -1 &&
					content_size != -1) {
				break;
			}

			ret = bt_field_type_structure_borrow_field_by_index(
					packet_context, &name, &field_type, i);
			if (ret) {
				BT_LOGE("Cannot get packet context structure field type's field: "
					"notit-addr=%p, stream-class-addr=%p, "
					"stream-class-name=\"%s\", "
					"stream-class-id=%" PRId64 ", "
					"ft-addr=%p, index=%d",
					notit, stream_class,
					bt_stream_class_get_name(stream_class),
					bt_stream_class_get_id(stream_class),
					event_header, i);
				goto error;
			}

			if (timestamp_end == -1 &&
					strcmp(name, "timestamp_end") == 0) {
				struct field_cb_override *override = g_new0(
						struct field_cb_override, 1);

				if (!override) {
					goto error;
				}

				override->func = btr_timestamp_end_cb;
				override->data = notit;
				g_hash_table_insert(notit->field_overrides,
					field_type, override);
				timestamp_end = i;
			} else if (packet_size == -1 &&
					!strcmp(name, "packet_size")) {
				packet_size = i;
			} else if (content_size == -1 &&
					!strcmp(name, "content_size")) {
				content_size = i;
			}
		}
	}

	cache_entry->v = v;
	cache_entry->id = id;
	cache_entry->timestamp_end = timestamp_end;
	cache_entry->packet_size = packet_size;
	cache_entry->content_size = content_size;

end:
	return cache_entry;

error:
	g_free(cache_entry);
	cache_entry = NULL;
	goto end;
}

static
struct stream_class_field_path_cache *get_stream_class_field_path_cache(
		struct bt_notif_iter *notit,
		struct bt_stream_class *stream_class)
{
	bool cache_entry_found;
	struct stream_class_field_path_cache *cache_entry;

	cache_entry_found = g_hash_table_lookup_extended(
			notit->sc_field_path_caches,
			stream_class, NULL, (gpointer) &cache_entry);
	if (unlikely(!cache_entry_found)) {
		cache_entry = create_stream_class_field_path_cache_entry(notit,
			stream_class);
		g_hash_table_insert(notit->sc_field_path_caches,
			stream_class, (gpointer) cache_entry);
	}

	return cache_entry;
}

static inline
enum bt_notif_iter_status set_current_stream_class(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field_type *packet_header_type = NULL;
	struct bt_field_type *stream_id_field_type = NULL;
	struct bt_stream_class *new_stream_class = NULL;
	uint64_t stream_id;

	/* Clear the current stream class field path cache. */
	notit->cur_sc_field_path_cache = NULL;

	/* Is there any "stream_id" field in the packet header? */
	packet_header_type = bt_trace_borrow_packet_header_field_type(
		notit->meta.trace);
	if (!packet_header_type) {
		/*
		 * No packet header, therefore no `stream_id` field,
		 * therefore only one stream class.
		 */
		goto single_stream_class;
	}

	BT_ASSERT(bt_field_type_is_structure(packet_header_type));

	// TODO: optimalize!
	stream_id_field_type =
		bt_field_type_structure_borrow_field_type_by_name(
			packet_header_type, "stream_id");
	if (stream_id_field_type) {
		/* Find appropriate stream class using current stream ID */
		int ret;
		struct bt_field *stream_id_field = NULL;

		BT_ASSERT(notit->dscopes.trace_packet_header);

		// TODO: optimalize!
		stream_id_field = bt_field_structure_borrow_field_by_name(
				notit->dscopes.trace_packet_header, "stream_id");
		BT_ASSERT(stream_id_field);
		ret = bt_field_integer_unsigned_get_value(
				stream_id_field, &stream_id);
		BT_ASSERT(!ret);
	} else {
single_stream_class:
		/* Only one stream: pick the first stream class */
		BT_ASSERT(bt_trace_get_stream_class_count(
				notit->meta.trace) == 1);
		stream_id = 0;
	}

	BT_LOGV("Found stream class ID to use: notit-addr=%p, "
		"stream-class-id=%" PRIu64 ", "
		"trace-addr=%p, trace-name=\"%s\"",
		notit, stream_id, notit->meta.trace,
		bt_trace_get_name(notit->meta.trace));

	new_stream_class = bt_trace_borrow_stream_class_by_id(
		notit->meta.trace, stream_id);
	if (!new_stream_class) {
		BT_LOGW("No stream class with ID of stream class ID to use in trace: "
			"notit-addr=%p, stream-class-id=%" PRIu64 ", "
			"trace-addr=%p, trace-name=\"%s\"",
			notit, stream_id, notit->meta.trace,
			bt_trace_get_name(notit->meta.trace));
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	if (notit->meta.stream_class) {
		if (new_stream_class != notit->meta.stream_class) {
			BT_LOGW("Two packets refer to two different stream classes within the same packet sequence: "
				"notit-addr=%p, prev-stream-class-addr=%p, "
				"prev-stream-class-name=\"%s\", "
				"prev-stream-class-id=%" PRId64 ", "
				"next-stream-class-addr=%p, "
				"next-stream-class-name=\"%s\", "
				"next-stream-class-id=%" PRId64 ", "
				"trace-addr=%p, trace-name=\"%s\"",
				notit, notit->meta.stream_class,
				bt_stream_class_get_name(notit->meta.stream_class),
				bt_stream_class_get_id(notit->meta.stream_class),
				new_stream_class,
				bt_stream_class_get_name(new_stream_class),
				bt_stream_class_get_id(new_stream_class),
				notit->meta.trace,
				bt_trace_get_name(notit->meta.trace));
			status = BT_NOTIF_ITER_STATUS_ERROR;
			goto end;
		}
	} else {
		notit->meta.stream_class = new_stream_class;
	}

	BT_LOGV("Set current stream class: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-class-id=%" PRId64,
		notit, notit->meta.stream_class,
		bt_stream_class_get_name(notit->meta.stream_class),
		bt_stream_class_get_id(notit->meta.stream_class));

	/*
	 * Retrieve (or lazily create) the current stream class field path
	 * cache.
	 */
	notit->cur_sc_field_path_cache = get_stream_class_field_path_cache(
		notit, notit->meta.stream_class);
	if (!notit->cur_sc_field_path_cache) {
		BT_LOGW("Cannot retrieve stream class field path from cache: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-name=\"%s\", stream-class-id=%" PRId64,
			notit, notit->meta.stream_class,
			bt_stream_class_get_name(notit->meta.stream_class),
			bt_stream_class_get_id(notit->meta.stream_class));
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static inline
uint64_t get_cur_stream_instance_id(struct bt_notif_iter *notit)
{
	struct bt_field *stream_instance_id_field = NULL;
	uint64_t stream_instance_id = -1ULL;
	int ret;

	if (!notit->dscopes.trace_packet_header) {
		goto end;
	}

	stream_instance_id_field = bt_field_structure_borrow_field_by_name(
		notit->dscopes.trace_packet_header, "stream_instance_id");
	if (!stream_instance_id_field) {
		goto end;
	}

	ret = bt_field_integer_unsigned_get_value(stream_instance_id_field,
		&stream_instance_id);
	if (ret) {
		stream_instance_id = -1ULL;
		goto end;
	}

end:
	return stream_instance_id;
}

static inline
enum bt_notif_iter_status set_current_stream(struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_stream *stream = NULL;

	BT_LOGV("Calling user function (get stream): notit-addr=%p, "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		notit, notit->meta.stream_class,
		bt_stream_class_get_name(notit->meta.stream_class),
		bt_stream_class_get_id(notit->meta.stream_class));
	stream = bt_get(notit->medium.medops.borrow_stream(
		notit->meta.stream_class, get_cur_stream_instance_id(notit),
		notit->medium.data));
	BT_LOGV("User function returned: stream-addr=%p", stream);
	if (!stream) {
		BT_LOGW_STR("User function failed to return a stream object for the given stream class.");
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	if (notit->stream && stream != notit->stream) {
		BT_LOGW("User function returned a different stream than the previous one for the same sequence of packets.");
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	BT_MOVE(notit->stream, stream);

end:
	bt_put(stream);
	return status;
}

static inline
enum bt_notif_iter_status set_current_packet(struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_packet *packet = NULL;

	BT_LOGV("Creating packet for packet notification: "
		"notit-addr=%p", notit);
	BT_LOGV("Creating packet from stream: "
		"notit-addr=%p, stream-addr=%p, "
		"stream-class-addr=%p, "
		"stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		notit, notit->stream, notit->meta.stream_class,
		bt_stream_class_get_name(notit->meta.stream_class),
		bt_stream_class_get_id(notit->meta.stream_class));

	/* Create packet */
	BT_ASSERT(notit->stream);
	packet = bt_packet_create(notit->stream, notit->prev_packet_avail,
		notit->prev_packet);
	if (!packet) {
		BT_LOGE("Cannot create packet from stream: "
			"notit-addr=%p, stream-addr=%p, "
			"stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64,
			notit, notit->stream, notit->meta.stream_class,
			bt_stream_class_get_name(notit->meta.stream_class),
			bt_stream_class_get_id(notit->meta.stream_class));
		goto error;
	}

	goto end;

error:
	BT_PUT(packet);
	status = BT_NOTIF_ITER_STATUS_ERROR;

end:
	BT_MOVE(notit->packet, packet);
	return status;
}

static
enum bt_notif_iter_status after_packet_header_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status;

	status = set_current_stream_class(notit);
	if (status != BT_NOTIF_ITER_STATUS_OK) {
		goto end;
	}

	notit->state = STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN;

end:
	return status;
}

static
enum bt_notif_iter_status read_packet_context_begin_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field_type *packet_context_type;

	BT_ASSERT(notit->meta.stream_class);
	packet_context_type = bt_stream_class_borrow_packet_context_field_type(
		notit->meta.stream_class);
	if (!packet_context_type) {
		BT_LOGV("No packet packet context field type in stream class: continuing: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-name=\"%s\", stream-class-id=%" PRId64,
			notit, notit->meta.stream_class,
			bt_stream_class_get_name(notit->meta.stream_class),
			bt_stream_class_get_id(notit->meta.stream_class));
		notit->state = STATE_AFTER_STREAM_PACKET_CONTEXT;
		goto end;
	}

	/*
	 * Create free packet context field from stream class. This
	 * field is going to be moved to the packet once we create it.
	 * We cannot create the packet now because a packet is created
	 * from a stream, and this API must be able to return the packet
	 * header and context fields without creating a stream
	 * (bt_notif_iter_get_packet_header_context_fields()).
	 */
	BT_ASSERT(!notit->packet_context_field);
	notit->packet_context_field =
		bt_stream_class_create_packet_context_field(
			notit->meta.stream_class);
	if (!notit->packet_context_field) {
		BT_LOGE_STR("Cannot create packet context field wrapper from stream class.");
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	notit->dscopes.stream_packet_context =
		bt_packet_context_field_borrow_field(notit->packet_context_field);
	BT_ASSERT(notit->dscopes.stream_packet_context);
	BT_LOGV("Decoding packet context field: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-class-id=%" PRId64 ", "
		"ft-addr=%p",
		notit, notit->meta.stream_class,
		bt_stream_class_get_name(notit->meta.stream_class),
		bt_stream_class_get_id(notit->meta.stream_class),
		packet_context_type);
	status = read_dscope_begin_state(notit, packet_context_type,
		STATE_AFTER_STREAM_PACKET_CONTEXT,
		STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE,
		notit->dscopes.stream_packet_context);
	if (status < 0) {
		BT_LOGW("Cannot decode packet context field: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", ft-addr=%p",
			notit, notit->meta.stream_class,
			bt_stream_class_get_name(notit->meta.stream_class),
			bt_stream_class_get_id(notit->meta.stream_class),
			packet_context_type);
	}

end:
	return status;
}

static
enum bt_notif_iter_status read_packet_context_continue_state(
		struct bt_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
			STATE_AFTER_STREAM_PACKET_CONTEXT);
}

static inline
uint64_t get_field_raw_clock_value(struct bt_field *base_field,
		const char *field_name, struct bt_clock_class **user_cc)
{
	struct bt_field *field;
	struct bt_field_type *ft;
	struct bt_clock_class *clock_class = NULL;
	uint64_t val = UINT64_C(-1);
	int ret;

	field = bt_field_structure_borrow_field_by_name(base_field, field_name);
	if (!field) {
		goto end;
	}

	ft = bt_field_borrow_type(field);
	BT_ASSERT(ft);

	if (!bt_field_type_is_integer(ft)) {
		goto end;
	}

	clock_class = bt_field_type_integer_borrow_mapped_clock_class(ft);
	if (!clock_class) {
		goto end;
	}

	ret = bt_field_integer_unsigned_get_value(field, &val);
	BT_ASSERT(ret == 0);

end:
	*user_cc = clock_class;
	return val;
}

static
enum bt_notif_iter_status set_current_packet_content_sizes(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field *packet_size_field = NULL;
	struct bt_field *content_size_field = NULL;
	uint64_t content_size = -1ULL, packet_size = -1ULL;

	if (!notit->dscopes.stream_packet_context) {
		goto end;
	}

	packet_size_field = bt_field_structure_borrow_field_by_name(
		notit->dscopes.stream_packet_context, "packet_size");
	content_size_field = bt_field_structure_borrow_field_by_name(
		notit->dscopes.stream_packet_context, "content_size");
	if (packet_size_field) {
		int ret = bt_field_integer_unsigned_get_value(
			packet_size_field, &packet_size);

		BT_ASSERT(ret == 0);
		if (packet_size == 0) {
			BT_LOGW("Invalid packet size: packet context field indicates packet size is zero: "
				"notit-addr=%p, packet-context-field-addr=%p",
				notit, notit->dscopes.stream_packet_context);
			status = BT_NOTIF_ITER_STATUS_ERROR;
			goto end;
		} else if ((packet_size % 8) != 0) {
			BT_LOGW("Invalid packet size: packet context field indicates packet size is not a multiple of 8: "
				"notit-addr=%p, packet-context-field-addr=%p, "
				"packet-size=%" PRIu64,
				notit, notit->dscopes.stream_packet_context,
				packet_size);
			status = BT_NOTIF_ITER_STATUS_ERROR;
			goto end;
		}
	}

	if (content_size_field) {
		int ret = bt_field_integer_unsigned_get_value(
			content_size_field, &content_size);

		BT_ASSERT(ret == 0);
	} else {
		content_size = packet_size;
	}

	if (content_size > packet_size) {
		BT_LOGW("Invalid packet or content size: packet context field indicates content size is greater than packet size: "
			"notit-addr=%p, packet-context-field-addr=%p, "
			"packet-size=%" PRIu64 ", content-size=%" PRIu64,
			notit, notit->dscopes.stream_packet_context,
			packet_size, content_size);
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	if (packet_size != -1ULL) {
		notit->cur_packet_size = packet_size;
	} else {
		/*
		 * Use the content size as packet size indicator if the
		 * packet size field is missing. This means there is no
		 * padding in this stream.
		 */
		notit->cur_packet_size = content_size;
	}
	notit->cur_content_size = content_size;
	BT_LOGV("Set current packet and content sizes: "
		"notit-addr=%p, packet-size=%" PRIu64 ", content-size=%" PRIu64,
		notit, packet_size, content_size);
end:
	return status;
}

static
enum bt_notif_iter_status after_packet_context_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status;

	status = set_current_packet_content_sizes(notit);
	if (status != BT_NOTIF_ITER_STATUS_OK) {
		goto end;
	}

	if (notit->stream_begin_emitted) {
		notit->state = STATE_EMIT_NOTIF_NEW_PACKET;
	} else {
		notit->state = STATE_EMIT_NOTIF_NEW_STREAM;
	}

end:
	return status;
}

static
enum bt_notif_iter_status read_event_header_begin_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field_type *event_header_type = NULL;

	/* Reset the position of the last event header */
	notit->buf.last_eh_at = notit->buf.at;

	/* Check if we have some content left */
	if (notit->cur_content_size >= 0) {
		if (packet_at(notit) == notit->cur_content_size) {
			/* No more events! */
			BT_LOGV("Reached end of packet: notit-addr=%p, "
				"cur=%zu", notit, packet_at(notit));
			notit->state = STATE_EMIT_NOTIF_END_OF_PACKET;
			goto end;
		} else if (packet_at(notit) > notit->cur_content_size) {
			/* That's not supposed to happen */
			BT_LOGV("Before decoding event header field: cursor is passed the packet's content: "
				"notit-addr=%p, content-size=%" PRId64 ", "
				"cur=%zu", notit, notit->cur_content_size,
				packet_at(notit));
			status = BT_NOTIF_ITER_STATUS_ERROR;
			goto end;
		}
	}

	release_event_dscopes(notit);
	BT_ASSERT(notit->meta.stream_class);
	event_header_type = bt_stream_class_borrow_event_header_field_type(
		notit->meta.stream_class);
	if (!event_header_type) {
		notit->state = STATE_AFTER_STREAM_EVENT_HEADER;
		goto end;
	}

	BT_ASSERT(!notit->event_header_field);
	notit->event_header_field = bt_stream_class_create_event_header_field(
		notit->meta.stream_class);
	if (!notit->event_header_field) {
		BT_LOGE_STR("Cannot create event header field wrapper from trace.");
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	notit->dscopes.stream_event_header =
		bt_event_header_field_borrow_field(notit->event_header_field);
	BT_ASSERT(notit->dscopes.stream_event_header);
	BT_LOGV("Decoding event header field: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-class-id=%" PRId64 ", "
		"ft-addr=%p",
		notit, notit->meta.stream_class,
		bt_stream_class_get_name(notit->meta.stream_class),
		bt_stream_class_get_id(notit->meta.stream_class),
		event_header_type);
	status = read_dscope_begin_state(notit, event_header_type,
		STATE_AFTER_STREAM_EVENT_HEADER,
		STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE,
		notit->dscopes.stream_event_header);
	if (status < 0) {
		BT_LOGW("Cannot decode event header field: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", ft-addr=%p",
			notit, notit->meta.stream_class,
			bt_stream_class_get_name(notit->meta.stream_class),
			bt_stream_class_get_id(notit->meta.stream_class),
			event_header_type);
	}

end:
	return status;
}

static
enum bt_notif_iter_status read_event_header_continue_state(
		struct bt_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_AFTER_STREAM_EVENT_HEADER);
}

static inline
enum bt_notif_iter_status set_current_event_class(struct bt_notif_iter *notit)
{
	/*
	 * The assert() calls in this function are okay because it is
	 * assumed here that all the metadata objects have been
	 * validated for CTF correctness before decoding actual streams.
	 */

	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field_type *event_header_type;
	struct bt_field_type *id_field_type = NULL;
	struct bt_field_type *v_field_type = NULL;
	uint64_t event_id = -1ULL;
	int ret;

	event_header_type = bt_stream_class_borrow_event_header_field_type(
		notit->meta.stream_class);
	if (!event_header_type) {
		/*
		 * No event header, therefore no event class ID field,
		 * therefore only one event class.
		 */
		goto single_event_class;
	}

	/* Is there any "id"/"v" field in the event header? */
	BT_ASSERT(bt_field_type_is_structure(event_header_type));
	id_field_type = bt_field_type_structure_borrow_field_type_by_name(
		event_header_type, "id");
	v_field_type = bt_field_type_structure_borrow_field_type_by_name(
		event_header_type, "v");
	BT_ASSERT(notit->dscopes.stream_event_header);
	if (v_field_type) {
		/*
		 *  _   _____ _____
		 * | | |_   _|_   _| __   __ _
		 * | |   | |   | || '_ \ / _` |
		 * | |___| |   | || | | | (_| |  S P E C I A L
		 * |_____|_|   |_||_| |_|\__, |  C A S E ™
		 *                       |___/
		 */
		struct bt_field *v_field = NULL;
		struct bt_field *v_struct_field = NULL;
		struct bt_field *v_struct_id_field = NULL;

		// TODO: optimalize!
		v_field = bt_field_structure_borrow_field_by_name(
			notit->dscopes.stream_event_header, "v");
		BT_ASSERT(v_field);

		v_struct_field =
			bt_field_variant_borrow_current_field(v_field);
		if (!v_struct_field) {
			goto end_v_field_type;
		}

		// TODO: optimalize!
		v_struct_id_field = bt_field_structure_borrow_field_by_name(
			v_struct_field, "id");
		if (!v_struct_id_field) {
			goto end_v_field_type;
		}

		if (bt_field_is_integer(v_struct_id_field)) {
			ret = bt_field_integer_unsigned_get_value(
				v_struct_id_field, &event_id);
			if (ret) {
				BT_LOGV("Cannot get value of unsigned integer field (`id`): continuing: "
					"notit=%p, field-addr=%p",
					notit, v_struct_id_field);
				event_id = -1ULL;
			}
		}
	}

end_v_field_type:
	if (id_field_type && event_id == -1ULL) {
		/* Check "id" field */
		struct bt_field *id_field = NULL;
		int ret_get_value = 0;

		// TODO: optimalize!
		id_field = bt_field_structure_borrow_field_by_name(
			notit->dscopes.stream_event_header, "id");
		if (!id_field) {
			goto check_event_id;
		}

		ret_get_value = bt_field_integer_unsigned_get_value(
			id_field, &event_id);
		BT_ASSERT(ret_get_value == 0);
	}

check_event_id:
	if (event_id == -1ULL) {
single_event_class:
		/* Event ID not found: single event? */
		BT_ASSERT(bt_stream_class_get_event_class_count(
			notit->meta.stream_class) == 1);
		event_id = 0;
	}

	BT_LOGV("Found event class ID to use: notit-addr=%p, "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", "
		"event-class-id=%" PRIu64,
		notit, notit->meta.stream_class,
		bt_stream_class_get_name(notit->meta.stream_class),
		bt_stream_class_get_id(notit->meta.stream_class),
		event_id);
	notit->meta.event_class = bt_stream_class_borrow_event_class_by_id(
		notit->meta.stream_class, event_id);
	if (!notit->meta.event_class) {
		BT_LOGW("No event class with ID of event class ID to use in stream class: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", "
			"event-class-id=%" PRIu64,
			notit, notit->meta.stream_class,
			bt_stream_class_get_name(notit->meta.stream_class),
			bt_stream_class_get_id(notit->meta.stream_class),
			event_id);
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	BT_LOGV("Set current event class: "
		"notit-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		notit, notit->meta.event_class,
		bt_event_class_get_name(notit->meta.event_class),
		bt_event_class_get_id(notit->meta.event_class));

end:
	return status;
}

static inline
enum bt_notif_iter_status set_current_event_notification(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_notification *notif = NULL;

	BT_ASSERT(notit->meta.event_class);
	BT_ASSERT(notit->packet);
	BT_LOGV("Creating event notification from event class and packet: "
		"notit-addr=%p, ec-addr=%p, ec-name=\"%s\", packet-addr=%p",
		notit, notit->meta.event_class,
		bt_event_class_get_name(notit->meta.event_class),
		notit->packet);
	BT_ASSERT(notit->notif_iter);
	notif = bt_notification_event_create(notit->notif_iter,
		notit->meta.event_class, notit->packet);
	if (!notif) {
		BT_LOGE("Cannot create event notification: "
			"notit-addr=%p, ec-addr=%p, ec-name=\"%s\", "
			"packet-addr=%p",
			notit, notit->meta.event_class,
			bt_event_class_get_name(notit->meta.event_class),
			notit->packet);
		goto error;
	}

	goto end;

error:
	BT_PUT(notif);
	status = BT_NOTIF_ITER_STATUS_ERROR;

end:
	BT_MOVE(notit->event_notif, notif);
	return status;
}

static
enum bt_notif_iter_status after_event_header_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status;

	status = set_current_event_class(notit);
	if (status != BT_NOTIF_ITER_STATUS_OK) {
		goto end;
	}

	status = set_current_event_notification(notit);
	if (status != BT_NOTIF_ITER_STATUS_OK) {
		goto end;
	}

	notit->event = bt_notification_event_borrow_event(notit->event_notif);
	BT_ASSERT(notit->event);

	if (notit->event_header_field) {
		int ret;

		BT_ASSERT(notit->event);
		ret = bt_event_move_header(notit->event,
			notit->event_header_field);

		if (ret) {
			status = BT_NOTIF_ITER_STATUS_ERROR;
			goto end;
		}

		notit->event_header_field = NULL;

		/*
		 * At this point notit->dscopes.stream_event_header has
		 * the same value as the event header field within
		 * notit->event.
		 */
		BT_ASSERT(bt_event_borrow_header(notit->event) ==
			notit->dscopes.stream_event_header);
	}

	notit->state = STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN;

end:
	return status;
}

static
enum bt_notif_iter_status read_stream_event_context_begin_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field_type *stream_event_context_type;

	stream_event_context_type =
		bt_stream_class_borrow_event_context_field_type(
			notit->meta.stream_class);
	if (!stream_event_context_type) {
		notit->state = STATE_DSCOPE_EVENT_CONTEXT_BEGIN;
		goto end;
	}

	BT_ASSERT(!notit->dscopes.stream_event_context);
	notit->dscopes.stream_event_context =
		bt_event_borrow_stream_event_context(notit->event);
	BT_ASSERT(notit->dscopes.stream_event_context);
	BT_LOGV("Decoding stream event context field: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-class-id=%" PRId64 ", "
		"ft-addr=%p",
		notit, notit->meta.stream_class,
		bt_stream_class_get_name(notit->meta.stream_class),
		bt_stream_class_get_id(notit->meta.stream_class),
		stream_event_context_type);
	status = read_dscope_begin_state(notit, stream_event_context_type,
		STATE_DSCOPE_EVENT_CONTEXT_BEGIN,
		STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE,
		notit->dscopes.stream_event_context);
	if (status < 0) {
		BT_LOGW("Cannot decode stream event context field: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-name=\"%s\", "
			"stream-class-id=%" PRId64 ", ft-addr=%p",
			notit, notit->meta.stream_class,
			bt_stream_class_get_name(notit->meta.stream_class),
			bt_stream_class_get_id(notit->meta.stream_class),
			stream_event_context_type);
	}

end:
	return status;
}

static
enum bt_notif_iter_status read_stream_event_context_continue_state(
		struct bt_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_DSCOPE_EVENT_CONTEXT_BEGIN);
}

static
enum bt_notif_iter_status read_event_context_begin_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field_type *event_context_type;

	event_context_type = bt_event_class_borrow_context_field_type(
		notit->meta.event_class);
	if (!event_context_type) {
		notit->state = STATE_DSCOPE_EVENT_PAYLOAD_BEGIN;
		goto end;
	}

	BT_ASSERT(!notit->dscopes.event_context);
	notit->dscopes.event_context = bt_event_borrow_context(notit->event);
	BT_ASSERT(notit->dscopes.event_context);
	BT_LOGV("Decoding event context field: "
		"notit-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"ft-addr=%p",
		notit, notit->meta.event_class,
		bt_event_class_get_name(notit->meta.event_class),
		bt_event_class_get_id(notit->meta.event_class),
		event_context_type);
	status = read_dscope_begin_state(notit, event_context_type,
		STATE_DSCOPE_EVENT_PAYLOAD_BEGIN,
		STATE_DSCOPE_EVENT_CONTEXT_CONTINUE,
		notit->dscopes.event_context);
	if (status < 0) {
		BT_LOGW("Cannot decode event context field: "
			"notit-addr=%p, event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", ft-addr=%p",
			notit, notit->meta.event_class,
			bt_event_class_get_name(notit->meta.event_class),
			bt_event_class_get_id(notit->meta.event_class),
			event_context_type);
	}

end:
	return status;
}

static
enum bt_notif_iter_status read_event_context_continue_state(
		struct bt_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_DSCOPE_EVENT_PAYLOAD_BEGIN);
}

static
enum bt_notif_iter_status read_event_payload_begin_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	struct bt_field_type *event_payload_type;

	event_payload_type = bt_event_class_borrow_payload_field_type(
		notit->meta.event_class);
	if (!event_payload_type) {
		notit->state = STATE_EMIT_NOTIF_EVENT;
		goto end;
	}

	BT_ASSERT(!notit->dscopes.event_payload);
	notit->dscopes.event_payload = bt_event_borrow_payload(notit->event);
	BT_ASSERT(notit->dscopes.event_payload);
	BT_LOGV("Decoding event payload field: "
		"notit-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"ft-addr=%p",
		notit, notit->meta.event_class,
		bt_event_class_get_name(notit->meta.event_class),
		bt_event_class_get_id(notit->meta.event_class),
		event_payload_type);
	status = read_dscope_begin_state(notit, event_payload_type,
		STATE_EMIT_NOTIF_EVENT,
		STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE,
		notit->dscopes.event_payload);
	if (status < 0) {
		BT_LOGW("Cannot decode event payload field: "
			"notit-addr=%p, event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", ft-addr=%p",
			notit, notit->meta.event_class,
			bt_event_class_get_name(notit->meta.event_class),
			bt_event_class_get_id(notit->meta.event_class),
			event_payload_type);
	}

end:
	return status;
}

static
enum bt_notif_iter_status read_event_payload_continue_state(
		struct bt_notif_iter *notit)
{
	return read_dscope_continue_state(notit, STATE_EMIT_NOTIF_EVENT);
}

static
enum bt_notif_iter_status skip_packet_padding_state(
		struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	size_t bits_to_skip;

	BT_ASSERT(notit->cur_packet_size > 0);
	bits_to_skip = notit->cur_packet_size - packet_at(notit);
	if (bits_to_skip == 0) {
		notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
		goto end;
	} else {
		size_t bits_to_consume;

		BT_LOGV("Trying to skip %zu bits of padding: notit-addr=%p, size=%zu",
			bits_to_skip, notit, bits_to_skip);
		status = buf_ensure_available_bits(notit);
		if (status != BT_NOTIF_ITER_STATUS_OK) {
			goto end;
		}

		bits_to_consume = MIN(buf_available_bits(notit), bits_to_skip);
		BT_LOGV("Skipping %zu bits of padding: notit-addr=%p, size=%zu",
			bits_to_consume, notit, bits_to_consume);
		buf_consume_bits(notit, bits_to_consume);
		bits_to_skip = notit->cur_packet_size - packet_at(notit);
		if (bits_to_skip == 0) {
			notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
			goto end;
		}
	}

end:
	return status;
}

static inline
enum bt_notif_iter_status handle_state(struct bt_notif_iter *notit)
{
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;
	const enum state state = notit->state;

	BT_LOGV("Handling state: notit-addr=%p, state=%s",
		notit, state_string(state));

	// TODO: optimalize!
	switch (state) {
	case STATE_INIT:
		notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
		break;
	case STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN:
		status = read_packet_header_begin_state(notit);
		break;
	case STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE:
		status = read_packet_header_continue_state(notit);
		break;
	case STATE_AFTER_TRACE_PACKET_HEADER:
		status = after_packet_header_state(notit);
		break;
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN:
		status = read_packet_context_begin_state(notit);
		break;
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE:
		status = read_packet_context_continue_state(notit);
		break;
	case STATE_AFTER_STREAM_PACKET_CONTEXT:
		status = after_packet_context_state(notit);
		break;
	case STATE_EMIT_NOTIF_NEW_STREAM:
		notit->state = STATE_EMIT_NOTIF_NEW_PACKET;
		break;
	case STATE_EMIT_NOTIF_NEW_PACKET:
		notit->state = STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN;
		break;
	case STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN:
		status = read_event_header_begin_state(notit);
		break;
	case STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE:
		status = read_event_header_continue_state(notit);
		break;
	case STATE_AFTER_STREAM_EVENT_HEADER:
		status = after_event_header_state(notit);
		break;
	case STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN:
		status = read_stream_event_context_begin_state(notit);
		break;
	case STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE:
		status = read_stream_event_context_continue_state(notit);
		break;
	case STATE_DSCOPE_EVENT_CONTEXT_BEGIN:
		status = read_event_context_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_CONTEXT_CONTINUE:
		status = read_event_context_continue_state(notit);
		break;
	case STATE_DSCOPE_EVENT_PAYLOAD_BEGIN:
		status = read_event_payload_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE:
		status = read_event_payload_continue_state(notit);
		break;
	case STATE_EMIT_NOTIF_EVENT:
		notit->state = STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN;
		break;
	case STATE_SKIP_PACKET_PADDING:
		status = skip_packet_padding_state(notit);
		break;
	case STATE_EMIT_NOTIF_END_OF_PACKET:
		notit->state = STATE_SKIP_PACKET_PADDING;
		break;
	default:
		BT_LOGD("Unknown CTF plugin notification iterator state: "
			"notit-addr=%p, state=%d", notit, notit->state);
		abort();
	}

	BT_LOGV("Handled state: notit-addr=%p, status=%s, "
		"prev-state=%s, cur-state=%s",
		notit, bt_notif_iter_status_string(status),
		state_string(state), state_string(notit->state));
	return status;
}

/**
 * Resets the internal state of a CTF notification iterator.
 */
BT_HIDDEN
void bt_notif_iter_reset(struct bt_notif_iter *notit)
{
	BT_ASSERT(notit);
	BT_LOGD("Resetting notification iterator: addr=%p", notit);
	stack_clear(notit->stack);
	notit->meta.stream_class = NULL;
	notit->meta.event_class = NULL;
	BT_PUT(notit->packet);
	notit->prev_packet_avail = BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_NONE;
	BT_PUT(notit->prev_packet);
	BT_PUT(notit->stream);
	BT_PUT(notit->event_notif);
	release_all_dscopes(notit);
	notit->cur_dscope_field = NULL;

	if (notit->packet_header_field) {
		bt_packet_header_field_release(notit->packet_header_field);
		notit->packet_header_field = NULL;
	}

	if (notit->packet_context_field) {
		bt_packet_context_field_release(notit->packet_context_field);
		notit->packet_context_field = NULL;
	}

	if (notit->event_header_field) {
		bt_event_header_field_release(notit->event_header_field);
		notit->event_header_field = NULL;
	}

	notit->buf.addr = NULL;
	notit->buf.sz = 0;
	notit->buf.at = 0;
	notit->buf.last_eh_at = SIZE_MAX;
	notit->buf.packet_offset = 0;
	notit->state = STATE_INIT;
	notit->cur_content_size = -1;
	notit->cur_packet_size = -1;
	notit->cur_packet_offset = -1;
	notit->stream_begin_emitted = false;
	notit->cur_timestamp_end = NULL;
}

static
int bt_notif_iter_switch_packet(struct bt_notif_iter *notit)
{
	int ret = 0;

	/*
	 * We don't put the stream class here because we need to make
	 * sure that all the packets processed by the same notification
	 * iterator refer to the same stream class (the first one).
	 */
	BT_ASSERT(notit);

	if (notit->cur_packet_size != -1) {
		notit->cur_packet_offset += notit->cur_packet_size;
	}

	BT_LOGV("Switching packet: notit-addr=%p, cur=%zu, "
		"packet-offset=%" PRId64, notit, notit->buf.at,
		notit->cur_packet_offset);
	stack_clear(notit->stack);
	notit->meta.event_class = NULL;
	BT_PUT(notit->packet);
	BT_PUT(notit->event_notif);
	notit->cur_timestamp_end = NULL;
	release_all_dscopes(notit);
	notit->cur_dscope_field = NULL;

	/*
	 * Adjust current buffer so that addr points to the beginning of the new
	 * packet.
	 */
	if (notit->buf.addr) {
		size_t consumed_bytes = (size_t) (notit->buf.at / CHAR_BIT);

		/* Packets are assumed to start on a byte frontier. */
		if (notit->buf.at % CHAR_BIT) {
			BT_LOGW("Cannot switch packet: current position is not a multiple of 8: "
				"notit-addr=%p, cur=%zu", notit, notit->buf.at);
			ret = -1;
			goto end;
		}

		notit->buf.addr += consumed_bytes;
		notit->buf.sz -= consumed_bytes;
		notit->buf.at = 0;
		notit->buf.packet_offset = 0;
		BT_LOGV("Adjusted buffer: addr=%p, size=%zu",
			notit->buf.addr, notit->buf.sz);
	}

	notit->cur_content_size = -1;
	notit->cur_packet_size = -1;
	notit->cur_sc_field_path_cache = NULL;

end:
	return ret;
}

static
struct bt_field *borrow_next_field(struct bt_notif_iter *notit)
{
	struct bt_field *next_field = NULL;
	struct bt_field *base_field;
	struct bt_field_type *base_type;
	size_t index;

	BT_ASSERT(!stack_empty(notit->stack));
	index = stack_top(notit->stack)->index;
	base_field = stack_top(notit->stack)->base;
	BT_ASSERT(base_field);
	base_type = bt_field_borrow_type(base_field);
	BT_ASSERT(base_type);

	switch (bt_field_type_get_type_id(base_type)) {
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		next_field = bt_field_structure_borrow_field_by_index(
			base_field, index);
		break;
	}
	case BT_FIELD_TYPE_ID_ARRAY:
		next_field = bt_field_array_borrow_field(base_field, index);
		break;
	case BT_FIELD_TYPE_ID_SEQUENCE:
		next_field = bt_field_sequence_borrow_field(base_field, index);
		break;
	case BT_FIELD_TYPE_ID_VARIANT:
		next_field = bt_field_variant_borrow_current_field(base_field);
		break;
	default:
		BT_LOGF("Unknown base field type ID: "
			"notit-addr=%p, ft-addr=%p, ft-id=%s",
			notit, base_type,
			bt_common_field_type_id_string(
				bt_field_type_get_type_id(base_type)));
		abort();
	}

	return next_field;
}

static
void update_clock_state(uint64_t *state, struct bt_field *value_field,
		struct bt_field_type *value_type)
{
	uint64_t requested_new_value;
	uint64_t requested_new_value_mask;
	uint64_t cur_value_masked;
	int requested_new_value_size;
	int ret;

	BT_ASSERT(value_type);
	BT_ASSERT(bt_field_type_is_integer(value_type));
	requested_new_value_size =
			bt_field_type_integer_get_size(value_type);
	BT_ASSERT(requested_new_value_size > 0);
	ret = bt_field_integer_unsigned_get_value(value_field,
			&requested_new_value);
	BT_ASSERT(!ret);

	/*
	 * Special case for a 64-bit new value, which is the limit
	 * of a clock value as of this version: overwrite the
	 * current value directly.
	 */
	if (requested_new_value_size == 64) {
		*state = requested_new_value;
		goto end;
	}

	requested_new_value_mask = (1ULL << requested_new_value_size) - 1;
	cur_value_masked = *state & requested_new_value_mask;

	if (requested_new_value < cur_value_masked) {
		/*
		 * It looks like a wrap happened on the number of bits
		 * of the requested new value. Assume that the clock
		 * value wrapped only one time.
		 */
		*state += requested_new_value_mask + 1;
	}

	/* Clear the low bits of the current clock value. */
	*state &= ~requested_new_value_mask;

	/* Set the low bits of the current clock value. */
	*state |= requested_new_value;

end:
	BT_LOGV("Updated clock's value from integer field's value: "
		"value=%" PRIu64, *state);
}

static
enum bt_btr_status update_clock(struct bt_notif_iter *notit,
		struct bt_field *int_field)
{
	gboolean clock_class_found;
	uint64_t *clock_state = NULL;
	struct bt_field_type *int_field_type = NULL;
	enum bt_btr_status ret = BT_BTR_STATUS_OK;
	struct bt_clock_class *clock_class = NULL;

	int_field_type = bt_field_borrow_type(int_field);
	BT_ASSERT(int_field_type);

	if (bt_field_type_is_enumeration(int_field_type)) {
		int_field_type =
			bt_field_type_enumeration_borrow_container_field_type(
				int_field_type);
	}

	clock_class = bt_field_type_integer_borrow_mapped_clock_class(
		int_field_type);
	if (likely(!clock_class)) {
		goto end;
	}

	clock_class_found = g_hash_table_lookup_extended(notit->clock_states,
		clock_class, NULL, (gpointer) &clock_state);
	if (!clock_class_found) {
		clock_state = g_new0(uint64_t, 1);
		if (!clock_state) {
			BT_LOGE_STR("Failed to allocate a uint64_t.");
			ret = BT_BTR_STATUS_ENOMEM;
			goto end;
		}

		g_hash_table_insert(notit->clock_states, clock_class,
			clock_state);
	}

	/* Update the clock's state. */
	BT_LOGV("Updating notification iterator's clock's value from integer field: "
		"notit-addr=%p, clock-class-addr=%p, "
		"clock-class-name=\"%s\", value=%" PRIu64,
		notit, clock_class,
		bt_clock_class_get_name(clock_class), *clock_state);
	update_clock_state(clock_state, int_field, int_field_type);

end:
	return ret;
}

static
enum bt_btr_status btr_unsigned_int_common(uint64_t value,
		struct bt_field_type *type, void *data,
		struct bt_field **out_field)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;
	struct bt_field *field = NULL;
	struct bt_notif_iter *notit = data;
	int ret;

	BT_LOGV("Common unsigned integer function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s, value=%" PRIu64,
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)),
		value);
	field = borrow_next_field(notit);
	if (!field) {
		BT_LOGW("Cannot borrow next field: notit-addr=%p", notit);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	BT_ASSERT(bt_field_is_integer(field) || bt_field_is_enumeration(field));
	ret = bt_field_integer_unsigned_set_value(field, value);
	BT_ASSERT(ret == 0);
	stack_top(notit->stack)->index++;
	*out_field = field;

end:
	return status;
}

static
enum bt_btr_status btr_timestamp_end_cb(void *value,
		struct bt_field_type *type, void *data)
{
	enum bt_btr_status status;
	struct bt_field *field = NULL;
	struct bt_notif_iter *notit = data;
	uint64_t uvalue = *((uint64_t *) value);

	BT_LOGV("`timestamp_end` unsigned integer function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s",
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)));

	status = btr_unsigned_int_common(uvalue, type, data, &field);

	/* Set as the current packet's end timestamp field */
	notit->cur_timestamp_end = field;
	return status;
}

static
enum bt_btr_status btr_unsigned_int_cb(uint64_t value,
		struct bt_field_type *type, void *data)
{
	struct bt_notif_iter *notit = data;
	enum bt_btr_status status = BT_BTR_STATUS_OK;
	struct bt_field *field = NULL;
	struct field_cb_override *override;

	BT_LOGV("Unsigned integer function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s, value=%" PRIu64,
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)),
		value);
	override = g_hash_table_lookup(notit->field_overrides, type);
	if (unlikely(override)) {
		/* Override function logs errors */
		status = override->func(&value, type, override->data);
		goto end;
	}

	status = btr_unsigned_int_common(value, type, data, &field);
	if (status != BT_BTR_STATUS_OK) {
		/* btr_unsigned_int_common() logs errors */
		goto end;
	}

	status = update_clock(notit, field);

end:
	return status;
}

static
enum bt_btr_status btr_signed_int_cb(int64_t value,
		struct bt_field_type *type, void *data)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;
	struct bt_field *field = NULL;
	struct bt_notif_iter *notit = data;
	int ret;

	BT_LOGV("Signed integer function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s, value=%" PRId64,
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)),
		value);
	field = borrow_next_field(notit);
	if (!field) {
		BT_LOGW("Cannot borrow next field: notit-addr=%p", notit);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	BT_ASSERT(bt_field_is_integer(field) || bt_field_is_enumeration(field));
	ret = bt_field_integer_signed_set_value(field, value);
	BT_ASSERT(ret == 0);
	stack_top(notit->stack)->index++;

end:
	return status;
}

static
enum bt_btr_status btr_floating_point_cb(double value,
		struct bt_field_type *type, void *data)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;
	struct bt_field *field = NULL;
	struct bt_notif_iter *notit = data;
	int ret;

	BT_LOGV("Floating point number function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s, value=%f",
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)),
		value);
	field = borrow_next_field(notit);
	if (!field) {
		BT_LOGW("Cannot borrow next field: notit-addr=%p", notit);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	ret = bt_field_floating_point_set_value(field, value);
	BT_ASSERT(!ret);
	stack_top(notit->stack)->index++;

end:
	return status;
}

static
enum bt_btr_status btr_string_begin_cb(
		struct bt_field_type *type, void *data)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;
	struct bt_field *field = NULL;
	struct bt_notif_iter *notit = data;
	int ret;

	BT_LOGV("String (beginning) function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s",
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)));
	field = borrow_next_field(notit);
	if (!field) {
		BT_LOGW("Cannot get next field: notit-addr=%p", notit);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

	ret = bt_field_string_clear(field);
	BT_ASSERT(ret == 0);

	/*
	 * Push on stack. Not a compound type per se, but we know that only
	 * btr_string_cb() may be called between this call and a subsequent
	 * call to btr_string_end_cb().
	 */
	ret = stack_push(notit->stack, field);
	if (ret) {
		BT_LOGE("Cannot push string field on stack: "
			"notit-addr=%p, field-addr=%p", notit, field);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static
enum bt_btr_status btr_string_cb(const char *value,
		size_t len, struct bt_field_type *type, void *data)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;
	struct bt_field *field = NULL;
	struct bt_notif_iter *notit = data;
	int ret;

	BT_LOGV("String (substring) function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s, string-length=%zu",
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)),
		len);
	field = stack_top(notit->stack)->base;
	BT_ASSERT(field);

	/* Append current substring */
	ret = bt_field_string_append_len(field, value, len);
	if (ret) {
		BT_LOGE("Cannot append substring to string field's value: "
			"notit-addr=%p, field-addr=%p, string-length=%zu, "
			"ret=%d", notit, field, len, ret);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static
enum bt_btr_status btr_string_end_cb(
		struct bt_field_type *type, void *data)
{
	struct bt_notif_iter *notit = data;

	BT_LOGV("String (end) function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s",
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)));

	/* Pop string field */
	stack_pop(notit->stack);

	/* Go to next field */
	stack_top(notit->stack)->index++;
	return BT_BTR_STATUS_OK;
}

enum bt_btr_status btr_compound_begin_cb(
		struct bt_field_type *type, void *data)
{
	enum bt_btr_status status = BT_BTR_STATUS_OK;
	struct bt_notif_iter *notit = data;
	struct bt_field *field;
	int ret;

	BT_LOGV("Compound (beginning) function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s",
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)));

	/* Borrow field */
	if (stack_empty(notit->stack)) {
		/* Root: already set by read_dscope_begin_state() */
		field = notit->cur_dscope_field;
	} else {
		field = borrow_next_field(notit);
		if (!field) {
			BT_LOGW("Cannot borrow next field: notit-addr=%p", notit);
			status = BT_BTR_STATUS_ERROR;
			goto end;
		}
	}

	/* Push field */
	BT_ASSERT(field);
	ret = stack_push(notit->stack, field);
	if (ret) {
		BT_LOGE("Cannot push compound field onto the stack: "
			"notit-addr=%p, ft-addr=%p, ft-id=%s, ret=%d",
			notit, type,
			bt_common_field_type_id_string(
				bt_field_type_get_type_id(type)),
			ret);
		status = BT_BTR_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

enum bt_btr_status btr_compound_end_cb(
		struct bt_field_type *type, void *data)
{
	struct bt_notif_iter *notit = data;

	BT_LOGV("Compound (end) function called from BTR: "
		"notit-addr=%p, btr-addr=%p, ft-addr=%p, "
		"ft-id=%s",
		notit, notit->btr, type,
		bt_common_field_type_id_string(
			bt_field_type_get_type_id(type)));
	BT_ASSERT(!stack_empty(notit->stack));

	/* Pop stack */
	stack_pop(notit->stack);

	/* If the stack is not empty, increment the base's index */
	if (!stack_empty(notit->stack)) {
		stack_top(notit->stack)->index++;
	}

	return BT_BTR_STATUS_OK;
}

static
struct bt_field *resolve_field(struct bt_notif_iter *notit,
		struct bt_field_path *path)
{
	struct bt_field *field = NULL;
	unsigned int i;

	if (BT_LOG_ON_VERBOSE) {
		GString *gstr = bt_field_path_string(path);

		BT_LOGV("Resolving field path: notit-addr=%p, field-path=\"%s\"",
			notit, gstr ? gstr->str : NULL);

		if (gstr) {
			g_string_free(gstr, TRUE);
		}
	}

	switch (bt_field_path_get_root_scope(path)) {
	case BT_SCOPE_TRACE_PACKET_HEADER:
		field = notit->dscopes.trace_packet_header;
		break;
	case BT_SCOPE_STREAM_PACKET_CONTEXT:
		field = notit->dscopes.stream_packet_context;
		break;
	case BT_SCOPE_STREAM_EVENT_HEADER:
		field = notit->dscopes.stream_event_header;
		break;
	case BT_SCOPE_STREAM_EVENT_CONTEXT:
		field = notit->dscopes.stream_event_context;
		break;
	case BT_SCOPE_EVENT_CONTEXT:
		field = notit->dscopes.event_context;
		break;
	case BT_SCOPE_EVENT_FIELDS:
		field = notit->dscopes.event_payload;
		break;
	default:
		BT_LOGF("Cannot resolve field path: unknown scope: "
			"notit-addr=%p, root-scope=%s",
			notit, bt_common_scope_string(
				bt_field_path_get_root_scope(path)));
		abort();
	}

	if (!field) {
		BT_LOGW("Cannot resolve field path: root field not found: "
			"notit-addr=%p, root-scope=%s",
			notit, bt_common_scope_string(
				bt_field_path_get_root_scope(path)));
		goto end;
	}

	for (i = 0; i < bt_field_path_get_index_count(path); ++i) {
		struct bt_field *next_field = NULL;
		struct bt_field_type *field_type;
		int index = bt_field_path_get_index(path, i);

		field_type = bt_field_borrow_type(field);
		BT_ASSERT(field_type);

		if (bt_field_type_is_structure(field_type)) {
			next_field = bt_field_structure_borrow_field_by_index(
				field, index);
		} else if (bt_field_type_is_variant(field_type)) {
			next_field =
				bt_field_variant_borrow_current_field(field);
		}

		field = NULL;

		if (!next_field) {
			BT_LOGW("Cannot find next field: "
				"notit-addr=%p, ft-addr=%p, ft-id=%s, index=%d",
				notit, field_type,
				bt_common_field_type_id_string(
					bt_field_type_get_type_id(field_type)),
				index);
			goto end;
		}

		/* Move next field -> field */
		field = next_field;
	}

end:
	return field;
}

static
int64_t btr_get_sequence_length_cb(struct bt_field_type *type, void *data)
{
	int64_t ret = -1;
	int iret;
	struct bt_field *seq_field;
	struct bt_field_path *field_path;
	struct bt_notif_iter *notit = data;
	struct bt_field *length_field = NULL;
	uint64_t length;

	field_path = bt_field_type_sequence_borrow_length_field_path(type);
	BT_ASSERT(field_path);
	length_field = resolve_field(notit, field_path);
	if (!length_field) {
		BT_LOGW("Cannot resolve sequence field type's length field path: "
			"notit-addr=%p, ft-addr=%p",
			notit, type);
		goto end;
	}

	iret = bt_field_integer_unsigned_get_value(length_field, &length);
	if (iret) {
		BT_LOGE("Cannot get value of sequence length field: "
			"notit-addr=%p, field-addr=%p",
			notit, length_field);
		goto end;
	}

	seq_field = stack_top(notit->stack)->base;
	iret = bt_field_sequence_set_length(seq_field, length);
	if (iret) {
		BT_LOGE("Cannot set sequence field's length field: "
			"notit-addr=%p, seq-field-addr=%p, "
			"length=%" PRIu64,
			notit, seq_field, length);
		goto end;
	}

	ret = (int64_t) length;

end:
	return ret;
}

static
struct bt_field_type *btr_borrow_variant_field_type_cb(
		struct bt_field_type *type, void *data)
{
	int ret;
	struct bt_field_path *path;
	struct bt_notif_iter *notit = data;
	struct bt_field *var_field;
	struct bt_field *tag_field = NULL;
	struct bt_field_type *tag_ft = NULL;
	struct bt_field_type *tag_int_ft = NULL;
	struct bt_field *selected_field = NULL;
	struct bt_field_type *selected_field_type = NULL;

	path = bt_field_type_variant_borrow_tag_field_path(type);
	BT_ASSERT(path);
	tag_field = resolve_field(notit, path);
	if (!tag_field) {
		BT_LOGW("Cannot resolve variant field type's tag field path: "
			"notit-addr=%p, ft-addr=%p",
			notit, type);
		goto end;
	}

	/*
	 * We found the enumeration tag field instance which should be
	 * able to select a current field for this variant. This
	 * callback function we're in is called _after_
	 * compound_begin(), so the current stack top's base field is
	 * the variant field in question. We set the variant field's tag
	 * here and then get the current (selected) field thanks to this
	 * tag field's value. This current field will also provide us
	 * with its type. Then, this current field will remain the
	 * current selected one until the next callback function call
	 * which is used to fill it.
	 */
	var_field = stack_top(notit->stack)->base;
	tag_ft = bt_field_borrow_type(tag_field);
	tag_int_ft = bt_field_type_enumeration_borrow_container_field_type(
		tag_ft);

	if (bt_field_type_integer_is_signed(tag_int_ft)) {
		int64_t tag_value;

		ret = bt_field_integer_signed_get_value(tag_field, &tag_value);
		BT_ASSERT(ret == 0);
		ret = bt_field_variant_set_tag_signed(var_field, tag_value);
		BT_ASSERT(ret == 0);
	} else {
		uint64_t tag_value;

		ret = bt_field_integer_unsigned_get_value(tag_field,
			&tag_value);
		BT_ASSERT(ret == 0);
		ret = bt_field_variant_set_tag_unsigned(var_field, tag_value);
		BT_ASSERT(ret == 0);
	}

	selected_field = bt_field_variant_borrow_current_field(var_field);
	if (!selected_field) {
		BT_LOGW("Cannot borrow variant field's current field: "
			"notit-addr=%p, var-field-addr=%p",
			notit, var_field);
		goto end;
	}

	selected_field_type = bt_field_borrow_type(selected_field);

end:
	return selected_field_type;
}

static
int set_event_clocks(struct bt_notif_iter *notit)
{
	int ret;
	GHashTableIter iter;
	struct bt_clock_class *clock_class;
	uint64_t *clock_state;

	g_hash_table_iter_init(&iter, notit->clock_states);

	while (g_hash_table_iter_next(&iter, (gpointer) &clock_class,
			(gpointer) &clock_state)) {
		ret = bt_event_set_clock_value(notit->event, clock_class,
			*clock_state, BT_TRUE);
		if (ret) {
			BT_LOGE("Cannot set event's default clock value: "
				"notit-addr=%p, clock-class-addr=%p, "
				"clock-class-name=\"%s\"",
				notit, clock_class,
				bt_clock_class_get_name(clock_class));
			ret = -1;
			goto end;
		}
	}

	ret = 0;

end:
	return ret;
}

static
void notify_new_stream(struct bt_notif_iter *notit,
		struct bt_notification **notification)
{
	enum bt_notif_iter_status status;
	struct bt_notification *ret = NULL;

	status = set_current_stream(notit);
	if (status != BT_NOTIF_ITER_STATUS_OK) {
		BT_PUT(ret);
		goto end;
	}

	BT_ASSERT(notit->stream);
	BT_ASSERT(notit->notif_iter);
	ret = bt_notification_stream_begin_create(notit->notif_iter,
		notit->stream);
	if (!ret) {
		BT_LOGE("Cannot create stream beginning notification: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}

end:
	*notification = ret;
}

static
void notify_end_of_stream(struct bt_notif_iter *notit,
		struct bt_notification **notification)
{
	struct bt_notification *ret;

	if (!notit->stream) {
		BT_LOGE("Cannot create stream for stream notification: "
			"notit-addr=%p", notit);
		return;
	}

	BT_ASSERT(notit->notif_iter);
	ret = bt_notification_stream_end_create(notit->notif_iter,
		notit->stream);
	if (!ret) {
		BT_LOGE("Cannot create stream beginning notification: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}
	*notification = ret;
}

static
void notify_new_packet(struct bt_notif_iter *notit,
		struct bt_notification **notification)
{
	int ret;
	enum bt_notif_iter_status status;
	struct bt_notification *notif = NULL;

	status = set_current_packet(notit);
	if (status != BT_NOTIF_ITER_STATUS_OK) {
		goto end;
	}

	BT_ASSERT(notit->packet);

	if (notit->packet_header_field) {
		ret = bt_packet_move_header(notit->packet,
			notit->packet_header_field);
		if (ret) {
			goto end;
		}

		notit->packet_header_field = NULL;

		/*
		 * At this point notit->dscopes.trace_packet_header has
		 * the same value as the packet header field within
		 * notit->packet.
		 */
		BT_ASSERT(bt_packet_borrow_header(notit->packet) ==
			notit->dscopes.trace_packet_header);
	}

	if (notit->packet_context_field) {
		ret = bt_packet_move_context(notit->packet,
			notit->packet_context_field);
		if (ret) {
			goto end;
		}

		notit->packet_context_field = NULL;

		/*
		 * At this point notit->dscopes.trace_packet_header has
		 * the same value as the packet header field within
		 * notit->packet.
		 */
		BT_ASSERT(bt_packet_borrow_context(notit->packet) ==
			notit->dscopes.stream_packet_context);
	}

	BT_ASSERT(notit->notif_iter);
	notif = bt_notification_packet_begin_create(notit->notif_iter,
		notit->packet);
	if (!notif) {
		BT_LOGE("Cannot create packet beginning notification: "
			"notit-addr=%p, packet-addr=%p",
			notit, notit->packet);
		goto end;
	}

	*notification = notif;

end:
	return;
}

static
void notify_end_of_packet(struct bt_notif_iter *notit,
		struct bt_notification **notification)
{
	struct bt_notification *notif;

	if (!notit->packet) {
		return;
	}

	BT_ASSERT(notit->notif_iter);
	notif = bt_notification_packet_end_create(notit->notif_iter,
		notit->packet);
	if (!notif) {
		BT_LOGE("Cannot create packet end notification: "
			"notit-addr=%p, packet-addr=%p",
			notit, notit->packet);
		return;

	}

	BT_MOVE(notit->prev_packet, notit->packet);
	notit->prev_packet_avail =
		BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_AVAILABLE;
	*notification = notif;
}

static
void init_trace_field_path_cache(struct bt_trace *trace,
		struct trace_field_path_cache *trace_field_path_cache)
{
	int stream_id = -1;
	int stream_instance_id = -1;
	int i, count;
	struct bt_field_type *packet_header = NULL;

	packet_header = bt_trace_borrow_packet_header_field_type(trace);
	if (!packet_header) {
		goto end;
	}

	if (!bt_field_type_is_structure(packet_header)) {
		goto end;
	}

	count = bt_field_type_structure_get_field_count(packet_header);
	BT_ASSERT(count >= 0);

	for (i = 0; (i < count && (stream_id == -1 || stream_instance_id == -1)); i++) {
		int ret;
		const char *field_name;

		ret = bt_field_type_structure_borrow_field_by_index(
			packet_header, &field_name, NULL, i);
		if (ret) {
			BT_LOGE("Cannot get structure field's field: "
				"field-addr=%p, index=%d",
				packet_header, i);
			goto end;
		}

		if (stream_id == -1 && !strcmp(field_name, "stream_id")) {
			stream_id = i;
		} else if (stream_instance_id == -1 &&
				!strcmp(field_name, "stream_instance_id")) {
			stream_instance_id = i;
		}
	}

end:
	trace_field_path_cache->stream_id = stream_id;
	trace_field_path_cache->stream_instance_id = stream_instance_id;
}

BT_HIDDEN
struct bt_notif_iter *bt_notif_iter_create(struct bt_trace *trace,
		size_t max_request_sz,
		struct bt_notif_iter_medium_ops medops, void *data)
{
	struct bt_notif_iter *notit = NULL;
	struct bt_btr_cbs cbs = {
		.types = {
			.signed_int = btr_signed_int_cb,
			.unsigned_int = btr_unsigned_int_cb,
			.floating_point = btr_floating_point_cb,
			.string_begin = btr_string_begin_cb,
			.string = btr_string_cb,
			.string_end = btr_string_end_cb,
			.compound_begin = btr_compound_begin_cb,
			.compound_end = btr_compound_end_cb,
		},
		.query = {
			.get_sequence_length = btr_get_sequence_length_cb,
			.borrow_variant_field_type = btr_borrow_variant_field_type_cb,
		},
	};

	BT_ASSERT(trace);
	BT_ASSERT(medops.request_bytes);
	BT_ASSERT(medops.borrow_stream);
	BT_LOGD("Creating CTF plugin notification iterator: "
		"trace-addr=%p, trace-name=\"%s\", max-request-size=%zu, "
		"data=%p",
		trace, bt_trace_get_name(trace), max_request_sz, data);
	notit = g_new0(struct bt_notif_iter, 1);
	if (!notit) {
		BT_LOGE_STR("Failed to allocate one CTF plugin notification iterator.");
		goto end;
	}
	notit->clock_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
	if (!notit->clock_states) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		goto error;
	}
	notit->meta.trace = trace;
	notit->medium.medops = medops;
	notit->medium.max_request_sz = max_request_sz;
	notit->medium.data = data;
	notit->stack = stack_new(notit);
	if (!notit->stack) {
		BT_LOGE_STR("Failed to create field stack.");
		goto error;
	}

	notit->btr = bt_btr_create(cbs, notit);
	if (!notit->btr) {
		BT_LOGE_STR("Failed to create binary type reader (BTR).");
		goto error;
	}

	bt_notif_iter_reset(notit);
	init_trace_field_path_cache(trace, &notit->trace_field_path_cache);
	notit->sc_field_path_caches = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
	if (!notit->sc_field_path_caches) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		goto error;
	}

	notit->field_overrides = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
	if (!notit->field_overrides) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		goto error;
	}

	BT_LOGD("Created CTF plugin notification iterator: "
		"trace-addr=%p, trace-name=\"%s\", max-request-size=%zu, "
		"data=%p, notit-addr=%p",
		trace, bt_trace_get_name(trace), max_request_sz, data,
		notit);
	notit->cur_packet_offset = 0;

end:
	return notit;

error:
	bt_notif_iter_destroy(notit);
	notit = NULL;
	goto end;
}

void bt_notif_iter_destroy(struct bt_notif_iter *notit)
{
	BT_PUT(notit->packet);
	BT_PUT(notit->prev_packet);
	BT_PUT(notit->stream);
	release_all_dscopes(notit);

	BT_LOGD("Destroying CTF plugin notification iterator: addr=%p", notit);

	if (notit->stack) {
		BT_LOGD_STR("Destroying field stack.");
		stack_destroy(notit->stack);
	}

	if (notit->btr) {
		BT_LOGD("Destroying BTR: btr-addr=%p", notit->btr);
		bt_btr_destroy(notit->btr);
	}

	if (notit->clock_states) {
		g_hash_table_destroy(notit->clock_states);
	}

	if (notit->sc_field_path_caches) {
		g_hash_table_destroy(notit->sc_field_path_caches);
	}

	if (notit->field_overrides) {
		g_hash_table_destroy(notit->field_overrides);
	}

	g_free(notit);
}

enum bt_notif_iter_status bt_notif_iter_get_next_notification(
		struct bt_notif_iter *notit,
		struct bt_private_connection_private_notification_iterator *notif_iter,
		struct bt_notification **notification)
{
	int ret;
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;

	BT_ASSERT(notit);
	BT_ASSERT(notification);

	if (notit->state == STATE_DONE) {
		status = BT_NOTIF_ITER_STATUS_EOF;
		goto end;
	}

	notit->notif_iter = notif_iter;

	BT_LOGV("Getting next notification: notit-addr=%p", notit);

	while (true) {
		status = handle_state(notit);
		if (status == BT_NOTIF_ITER_STATUS_AGAIN) {
			BT_LOGV_STR("Medium returned BT_NOTIF_ITER_STATUS_AGAIN.");
			goto end;
		}

		if (status != BT_NOTIF_ITER_STATUS_OK) {
			if (status == BT_NOTIF_ITER_STATUS_EOF) {
				enum state next_state = notit->state;

				BT_LOGV_STR("Medium returned BT_NOTIF_ITER_STATUS_EOF.");

				if (notit->packet) {
					notify_end_of_packet(notit,
						notification);
				} else {
					notify_end_of_stream(notit,
						notification);
					next_state = STATE_DONE;
				}

				if (!*notification) {
					status = BT_NOTIF_ITER_STATUS_ERROR;
					goto end;
				}

				status = BT_NOTIF_ITER_STATUS_OK;
				notit->state = next_state;
			} else {
				BT_LOGW("Cannot handle state: "
					"notit-addr=%p, state=%s",
					notit, state_string(notit->state));
			}

			goto end;
		}

		switch (notit->state) {
		case STATE_EMIT_NOTIF_NEW_STREAM:
			/* notify_new_stream() logs errors */
			notify_new_stream(notit, notification);
			if (!*notification) {
				status = BT_NOTIF_ITER_STATUS_ERROR;
			}
			notit->stream_begin_emitted = true;
			goto end;
		case STATE_EMIT_NOTIF_NEW_PACKET:
			/* notify_new_packet() logs errors */
			notify_new_packet(notit, notification);
			if (!*notification) {
				status = BT_NOTIF_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_NOTIF_EVENT:
			BT_ASSERT(notit->event_notif);
			ret = set_event_clocks(notit);
			if (ret) {
				status = BT_NOTIF_ITER_STATUS_ERROR;
				goto end;
			}

			*notification = notit->event_notif;
			notit->event_notif = NULL;
			goto end;
		case STATE_EMIT_NOTIF_END_OF_PACKET:
			/* Update clock with timestamp_end field. */
			if (notit->cur_timestamp_end) {
				enum bt_btr_status btr_status;

				btr_status = update_clock(notit,
					notit->cur_timestamp_end);
				if (btr_status != BT_BTR_STATUS_OK) {
					BT_LOGW("Cannot update stream's clock value: "
						"notit-addr=%p", notit);
					status = BT_NOTIF_ITER_STATUS_ERROR;
					goto end;
				}
			}

			/* notify_end_of_packet() logs errors */
			notify_end_of_packet(notit, notification);
			if (!*notification) {
				status = BT_NOTIF_ITER_STATUS_ERROR;
			}

			goto end;
		default:
			/* Non-emitting state: continue */
			break;
		}
	}

end:
	return status;
}

BT_HIDDEN
enum bt_notif_iter_status bt_notif_iter_borrow_packet_header_context_fields(
		struct bt_notif_iter *notit,
		struct bt_field **packet_header_field,
		struct bt_field **packet_context_field)
{
	int ret;
	enum bt_notif_iter_status status = BT_NOTIF_ITER_STATUS_OK;

	BT_ASSERT(notit);

	if (notit->state == STATE_EMIT_NOTIF_NEW_PACKET) {
		/* We're already there */
		goto set_fields;
	}

	while (true) {
		status = handle_state(notit);
		if (status == BT_NOTIF_ITER_STATUS_AGAIN) {
			BT_LOGV_STR("Medium returned BT_NOTIF_ITER_STATUS_AGAIN.");
			goto end;
		}
		if (status != BT_NOTIF_ITER_STATUS_OK) {
			if (status == BT_NOTIF_ITER_STATUS_EOF) {
				BT_LOGV_STR("Medium returned BT_NOTIF_ITER_STATUS_EOF.");
			} else {
				BT_LOGW("Cannot handle state: "
					"notit-addr=%p, state=%s",
					notit, state_string(notit->state));
			}
			goto end;
		}

		switch (notit->state) {
		case STATE_EMIT_NOTIF_NEW_PACKET:
			/*
			 * Packet header and context fields are
			 * potentially decoded (or they don't exist).
			 */
			goto set_fields;
		case STATE_INIT:
		case STATE_EMIT_NOTIF_NEW_STREAM:
		case STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN:
		case STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE:
		case STATE_AFTER_TRACE_PACKET_HEADER:
		case STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN:
		case STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE:
		case STATE_AFTER_STREAM_PACKET_CONTEXT:
			/* Non-emitting state: continue */
			break;
		default:
			/*
			 * We should never get past the
			 * STATE_EMIT_NOTIF_NEW_PACKET state.
			 */
			BT_LOGF("Unexpected state: notit-addr=%p, state=%s",
				notit, state_string(notit->state));
			abort();
		}
	}

set_fields:
	ret = set_current_packet_content_sizes(notit);
	if (ret) {
		status = BT_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	if (packet_header_field) {
		*packet_header_field = notit->dscopes.trace_packet_header;
	}

	if (packet_context_field) {
		*packet_context_field = notit->dscopes.stream_packet_context;
	}

end:
	return status;
}

BT_HIDDEN
void bt_notif_iter_set_medops_data(struct bt_notif_iter *notit,
		void *medops_data)
{
	BT_ASSERT(notit);
	notit->medium.data = medops_data;
}

BT_HIDDEN
enum bt_notif_iter_status bt_notif_iter_seek(
		struct bt_notif_iter *notit, off_t offset)
{
	enum bt_notif_iter_status ret = BT_NOTIF_ITER_STATUS_OK;
	enum bt_notif_iter_medium_status medium_status;

	BT_ASSERT(notit);
	if (offset < 0) {
		BT_LOGE("Cannot seek to negative offset: offset=%jd", offset);
		ret = BT_NOTIF_ITER_STATUS_INVAL;
		goto end;
	}

	if (!notit->medium.medops.seek) {
		ret = BT_NOTIF_ITER_STATUS_UNSUPPORTED;
		BT_LOGD("Aborting seek as the iterator's underlying media does not implement seek support.");
		goto end;
	}

	medium_status = notit->medium.medops.seek(
			BT_NOTIF_ITER_SEEK_WHENCE_SET, offset,
			notit->medium.data);
	if (medium_status != BT_NOTIF_ITER_MEDIUM_STATUS_OK) {
		if (medium_status == BT_NOTIF_ITER_MEDIUM_STATUS_EOF) {
			ret = BT_NOTIF_ITER_STATUS_EOF;
		} else {
			ret = BT_NOTIF_ITER_STATUS_ERROR;
			goto end;
		}
	}

	bt_notif_iter_reset(notit);
	notit->cur_packet_offset = offset;
end:
	return ret;
}

BT_HIDDEN
off_t bt_notif_iter_get_current_packet_offset(
		struct bt_notif_iter *notit)
{
	BT_ASSERT(notit);
	return notit->cur_packet_offset;
}

BT_HIDDEN
off_t bt_notif_iter_get_current_packet_size(
		struct bt_notif_iter *notit)
{
	BT_ASSERT(notit);
	return notit->cur_packet_size;
}
