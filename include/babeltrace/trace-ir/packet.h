#ifndef BABELTRACE_TRACE_IR_PACKET_H
#define BABELTRACE_TRACE_IR_PACKET_H

/*
 * BabelTrace - Trace IR: Stream packet
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdint.h>

/* For enum bt_property_availability */
#include <babeltrace/property.h>

/* For enum bt_clock_value_status */
#include <babeltrace/trace-ir/clock-value.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_packet;
struct bt_packet_header_field;
struct bt_packet_context_field;
struct bt_stream;
struct bt_clock_value;

extern struct bt_packet *bt_packet_create(struct bt_stream *stream);

extern struct bt_stream *bt_packet_borrow_stream(struct bt_packet *packet);

extern
struct bt_field *bt_packet_borrow_header_field(struct bt_packet *packet);

extern
int bt_packet_move_header_field(struct bt_packet *packet,
		struct bt_packet_header_field *header);

extern
struct bt_field *bt_packet_borrow_context_field(struct bt_packet *packet);

extern
int bt_packet_move_context_field(struct bt_packet *packet,
		struct bt_packet_context_field *context);

extern
enum bt_clock_value_status bt_packet_borrow_default_beginning_clock_value(
		struct bt_packet *packet, struct bt_clock_value **clock_value);

extern
int bt_packet_set_default_beginning_clock_value(struct bt_packet *packet,
		uint64_t value_cycles);

extern
enum bt_clock_value_status bt_packet_borrow_default_end_clock_valeu(
		struct bt_packet *packet, struct bt_clock_value **clock_value);

extern
int bt_packet_set_default_end_clock_value(struct bt_packet *packet,
		uint64_t value_cycles);

extern
enum bt_property_availability bt_packet_get_discarded_event_counter_snapshot(
		struct bt_packet *packet, uint64_t *value);

extern
int bt_packet_set_discarded_event_counter_snapshot(struct bt_packet *packet,
		uint64_t value);

extern
enum bt_property_availability bt_packet_get_packet_counter_snapshot(
		struct bt_packet *packet, uint64_t *value);

extern
int bt_packet_set_packet_counter_snapshot(struct bt_packet *packet,
		uint64_t value);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_PACKET_H */