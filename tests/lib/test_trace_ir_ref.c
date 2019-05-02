/*
 * test_trace_ir_ref.c
 *
 * Trace IR Reference Count test
 *
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "tap/tap.h"
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-writer/trace.h>
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/event.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/trace-ir/fields.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/trace.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compat/stdlib-internal.h>
#include <babeltrace/assert-internal.h>
#include "common.h"

#define NR_TESTS 37

struct user {
	struct bt_trace *tc;
	struct bt_stream_class *sc;
	struct bt_event_class *ec;
	struct bt_stream *stream;
	struct bt_event *event;
};

struct writer_user {
	struct bt_ctf_writer *writer;
	struct bt_ctf_trace *tc;
	struct bt_ctf_stream_class *sc;
	struct bt_ctf_event_class *ec;
	struct bt_ctf_stream *stream;
	struct bt_ctf_event *event;
};

const char *writer_user_names[] = {
	"writer",
	"trace",
	"stream class",
	"event class",
	"stream",
	"event",
};

static const size_t WRITER_USER_NR_ELEMENTS =
	sizeof(struct writer_user) / sizeof(void *);

/**
 * Returns a structure containing the following fields:
 *     - uint8_t payload_8;
 *     - uint16_t payload_16;
 *     - uint32_t payload_32;
 */
static struct bt_field_class *create_integer_struct(void)
{
	int ret;
	struct bt_field_class *structure = NULL;
	struct bt_field_class *ui8 = NULL, *ui16 = NULL, *ui32 = NULL;

	structure = bt_field_class_structure_create();
	BT_ASSERT(structure);
	ui8 = bt_field_class_unsigned_integer_create();
	BT_ASSERT(ui8);
	ret = bt_field_class_integer_set_field_value_range(ui8, 8);
	BT_ASSERT(ret == 0);
	ret = bt_field_class_structure_append_member(structure,
		"payload_8", ui8);
	BT_ASSERT(ret == 0);
	ui16 = bt_field_class_unsigned_integer_create();
	BT_ASSERT(ui16);
	ret = bt_field_class_integer_set_field_value_range(ui16, 16);
	BT_ASSERT(ret == 0);
	ret = bt_field_class_structure_append_member(structure,
		"payload_16", ui16);
	BT_ASSERT(ret == 0);
	ui32 = bt_field_class_unsigned_integer_create();
	BT_ASSERT(ui32);
	ret = bt_field_class_integer_set_field_value_range(ui32, 32);
	BT_ASSERT(ret == 0);
	ret = bt_field_class_structure_append_member(structure,
		"payload_32", ui32);
	BT_ASSERT(ret == 0);
	BT_PUT(ui8);
	BT_PUT(ui16);
	BT_PUT(ui32);
	return structure;
}

static struct bt_ctf_field_type *create_writer_integer_struct(void)
{
	int ret;
	struct bt_ctf_field_type *structure = NULL;
	struct bt_ctf_field_type *ui8 = NULL, *ui16 = NULL, *ui32 = NULL;

	structure = bt_ctf_field_type_structure_create();
	BT_ASSERT(structure);
	ui8 = bt_ctf_field_type_integer_create(8);
	BT_ASSERT(ui8);
	ret = bt_ctf_field_type_structure_add_field(structure, ui8,
		        "payload_8");
	BT_ASSERT(ret == 0);
	ui16 = bt_ctf_field_type_integer_create(16);
	BT_ASSERT(ui16);
	ret = bt_ctf_field_type_structure_add_field(structure, ui16,
		        "payload_16");
	BT_ASSERT(ret == 0);
	ui32 = bt_ctf_field_type_integer_create(32);
	BT_ASSERT(ui32);
	ret = bt_ctf_field_type_structure_add_field(structure, ui32,
		        "payload_32");
	BT_ASSERT(ret == 0);
	BT_PUT(ui8);
	BT_PUT(ui16);
	BT_PUT(ui32);
	return structure;
}

/**
 * A simple event has the following payload:
 *     - uint8_t payload_8;
 *     - uint16_t payload_16;
 *     - uint32_t payload_32;
 */
static struct bt_event_class *create_simple_event(struct bt_stream_class *sc,
		const char *name)
{
	int ret;
	struct bt_event_class *event = NULL;
	struct bt_field_class *payload = NULL;

	BT_ASSERT(name);
	event = bt_event_class_create(sc);
	BT_ASSERT(event);
	ret = bt_event_class_set_name(event, name);
	BT_ASSERT(ret == 0);
	payload = create_integer_struct();
	BT_ASSERT(payload);
	ret = bt_event_class_set_payload_field_class(event, payload);
	BT_ASSERT(ret == 0);
	BT_PUT(payload);
	return event;
}

/**
 * A complex event has the following payload:
 *     - uint8_t payload_8;
 *     - uint16_t payload_16;
 *     - uint32_t payload_32;
 *     - struct payload_struct:
 *           - uint8_t payload_8;
 *           - uint16_t payload_16;
 *           - uint32_t payload_32;
 */
static struct bt_event_class *create_complex_event(struct bt_stream_class *sc,
		const char *name)
{
	int ret;
	struct bt_event_class *event = NULL;
	struct bt_field_class *inner = NULL, *outer = NULL;

	BT_ASSERT(name);
	event = bt_event_class_create(sc);
	BT_ASSERT(event);
	ret = bt_event_class_set_name(event, name);
	BT_ASSERT(ret == 0);
	outer = create_integer_struct();
	BT_ASSERT(outer);
	inner = create_integer_struct();
	BT_ASSERT(inner);
	ret = bt_field_class_structure_append_member(outer,
		"payload_struct", inner);
	BT_ASSERT(ret == 0);
	ret = bt_event_class_set_payload_field_class(event, outer);
	BT_ASSERT(ret == 0);
	BT_PUT(inner);
	BT_PUT(outer);
	return event;
}

static void set_stream_class_field_classes(
		struct bt_stream_class *stream_class)
{
	struct bt_field_class *packet_context_type;
	struct bt_field_class *event_header_type;
	struct bt_field_class *fc;
	int ret;

	packet_context_type = bt_field_class_structure_create();
	BT_ASSERT(packet_context_type);
	fc = bt_field_class_unsigned_integer_create();
	BT_ASSERT(fc);
	ret = bt_field_class_integer_set_field_value_range(fc, 32);
	BT_ASSERT(ret == 0);
	ret = bt_field_class_structure_append_member(packet_context_type,
		"packet_size", fc);
	BT_ASSERT(ret == 0);
	bt_put(fc);
	fc = bt_field_class_unsigned_integer_create();
	BT_ASSERT(fc);
	ret = bt_field_class_integer_set_field_value_range(fc, 32);
	BT_ASSERT(ret == 0);
	ret = bt_field_class_structure_append_member(packet_context_type,
		"content_size", fc);
	BT_ASSERT(ret == 0);
	bt_put(fc);
	event_header_type = bt_field_class_structure_create();
	BT_ASSERT(event_header_type);
	fc = bt_field_class_unsigned_integer_create();
	BT_ASSERT(fc);
	ret = bt_field_class_integer_set_field_value_range(fc, 32);
	BT_ASSERT(ret == 0);
	ret = bt_field_class_structure_append_member(event_header_type,
		"id", fc);
	BT_ASSERT(ret == 0);
	bt_put(fc);
	ret = bt_stream_class_set_packet_context_field_class(stream_class,
		packet_context_type);
	BT_ASSERT(ret == 0);
	ret = bt_stream_class_set_event_header_field_class(stream_class,
		event_header_type);
	BT_ASSERT(ret == 0);
	bt_put(packet_context_type);
	bt_put(event_header_type);
}

static void create_sc1(struct bt_trace *trace)
{
	int ret;
	struct bt_event_class *ec1 = NULL, *ec2 = NULL;
	struct bt_stream_class *sc1 = NULL, *ret_stream = NULL;

	sc1 = bt_stream_class_create(trace);
	BT_ASSERT(sc1);
	ret = bt_stream_class_set_name(sc1, "sc1");
	BT_ASSERT(ret == 0);
	set_stream_class_field_classes(sc1);
	ec1 = create_complex_event(sc1, "ec1");
	BT_ASSERT(ec1);
	ec2 = create_simple_event(sc1, "ec2");
	BT_ASSERT(ec2);
	ret_stream = bt_event_class_borrow_stream_class(ec1);
	ok(ret_stream == sc1, "Borrow parent stream SC1 from EC1");
	ret_stream = bt_event_class_borrow_stream_class(ec2);
	ok(ret_stream == sc1, "Borrow parent stream SC1 from EC2");
	BT_PUT(ec1);
	BT_PUT(ec2);
	BT_PUT(sc1);
}

static void create_sc2(struct bt_trace *trace)
{
	int ret;
	struct bt_event_class *ec3 = NULL;
	struct bt_stream_class *sc2 = NULL, *ret_stream = NULL;

	sc2 = bt_stream_class_create(trace);
	BT_ASSERT(sc2);
	ret = bt_stream_class_set_name(sc2, "sc2");
	BT_ASSERT(ret == 0);
	set_stream_class_field_classes(sc2);
	ec3 = create_simple_event(sc2, "ec3");
	ret_stream = bt_event_class_borrow_stream_class(ec3);
	ok(ret_stream == sc2, "Borrow parent stream SC2 from EC3");
	BT_PUT(ec3);
	BT_PUT(sc2);
}

static void set_trace_packet_header(struct bt_trace *trace)
{
	struct bt_field_class *packet_header_type;
	struct bt_field_class *fc;
	int ret;

	packet_header_type = bt_field_class_structure_create();
	BT_ASSERT(packet_header_type);
	fc = bt_field_class_unsigned_integer_create();
	BT_ASSERT(fc);
	ret = bt_field_class_integer_set_field_value_range(fc, 32);
	BT_ASSERT(ret == 0);
	ret = bt_field_class_structure_append_member(packet_header_type,
		"stream_id", fc);
	BT_ASSERT(ret == 0);
	bt_put(fc);
	ret = bt_trace_set_packet_header_field_class(trace,
		packet_header_type);
	BT_ASSERT(ret == 0);

	bt_put(packet_header_type);
}

static struct bt_trace *create_tc1(void)
{
	struct bt_trace *tc1 = NULL;

	tc1 = bt_trace_create();
	BT_ASSERT(tc1);
	set_trace_packet_header(tc1);
	create_sc1(tc1);
	create_sc2(tc1);
	return tc1;
}

static void init_weak_refs(struct bt_trace *tc,
		struct bt_trace **tc1,
		struct bt_stream_class **sc1,
		struct bt_stream_class **sc2,
		struct bt_event_class **ec1,
		struct bt_event_class **ec2,
		struct bt_event_class **ec3)
{
	*tc1 = tc;
	*sc1 = bt_trace_borrow_stream_class_by_index(tc, 0);
	*sc2 = bt_trace_borrow_stream_class_by_index(tc, 1);
	*ec1 = bt_stream_class_borrow_event_class_by_index(*sc1, 0);
	*ec2 = bt_stream_class_borrow_event_class_by_index(*sc1, 1);
	*ec3 = bt_stream_class_borrow_event_class_by_index(*sc2, 0);
}

static void test_example_scenario(void)
{
	/*
	 * Weak pointers to trace IR objects are to be used very
	 * carefully. This is NOT a good practice and is strongly
	 * discouraged; this is only done to facilitate the validation
	 * of expected reference counts without affecting them by taking
	 * "real" references to the objects.
	 */
	struct bt_trace *tc1 = NULL, *weak_tc1 = NULL;
	struct bt_stream_class *weak_sc1 = NULL, *weak_sc2 = NULL;
	struct bt_event_class *weak_ec1 = NULL, *weak_ec2 = NULL,
			*weak_ec3 = NULL;
	struct user user_a = { 0 }, user_b = { 0 }, user_c = { 0 };

	/* The only reference which exists at this point is on TC1. */
	tc1 = create_tc1();
	ok(tc1, "Initialize trace");
	BT_ASSERT(tc1);
	init_weak_refs(tc1, &weak_tc1, &weak_sc1, &weak_sc2, &weak_ec1,
			&weak_ec2, &weak_ec3);
	ok(bt_object_get_ref_count((void *) weak_sc1) == 0,
			"Initial SC1 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 0,
			"Initial SC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec1) == 0,
			"Initial EC1 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec2) == 0,
			"Initial EC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 0,
			"Initial EC3 reference count is 0");

	/* User A has ownership of the trace. */
	BT_MOVE(user_a.tc, tc1);
	ok(bt_object_get_ref_count((void *) user_a.tc) == 1,
			"TC1 reference count is 1");

	/* User A acquires a reference to SC2 from TC1. */
	user_a.sc = bt_get(bt_trace_borrow_stream_class_by_index(user_a.tc, 1));
	ok(user_a.sc, "User A acquires SC2 from TC1");
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 1,
			"SC2 reference count is 1");

	/* User A acquires a reference to EC3 from SC2. */
	user_a.ec = bt_get(
		bt_stream_class_borrow_event_class_by_index(user_a.sc, 0));
	ok(user_a.ec, "User A acquires EC3 from SC2");
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 2,
			"SC2 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 1,
			"EC3 reference count is 1");

	/* User A releases its reference to SC2. */
	diag("User A releases SC2");
	BT_PUT(user_a.sc);
	/*
	 * We keep the pointer to SC2 around to validate its reference
	 * count.
	 */
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 1,
			"SC2 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 1,
			"EC3 reference count is 1");

	/* User A releases its reference to TC1. */
	diag("User A releases TC1");
	BT_PUT(user_a.tc);
	/*
	 * We keep the pointer to TC1 around to validate its reference
	 * count.
	 */
	ok(bt_object_get_ref_count((void *) weak_tc1) == 1,
			"TC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 1,
			"SC2 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 1,
			"EC3 reference count is 1");

	/* User B acquires a reference to SC1. */
	diag("User B acquires a reference to SC1");
	user_b.sc = bt_get(weak_sc1);
	ok(bt_object_get_ref_count((void *) weak_tc1) == 2,
			"TC1 reference count is 2");
	ok(bt_object_get_ref_count((void *) weak_sc1) == 1,
			"SC1 reference count is 1");

	/* User C acquires a reference to EC1. */
	diag("User C acquires a reference to EC1");
	user_c.ec = bt_get(
		bt_stream_class_borrow_event_class_by_index(user_b.sc, 0));
	ok(bt_object_get_ref_count((void *) weak_ec1) == 1,
			"EC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc1) == 2,
			"SC1 reference count is 2");

	/* User A releases its reference on EC3. */
	diag("User A releases its reference on EC3");
	BT_PUT(user_a.ec);
	ok(bt_object_get_ref_count((void *) weak_ec3) == 0,
			"EC3 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 0,
			"SC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_tc1) == 1,
			"TC1 reference count is 1");

	/* User B releases its reference on SC1. */
	diag("User B releases its reference on SC1");
	BT_PUT(user_b.sc);
	ok(bt_object_get_ref_count((void *) weak_sc1) == 1,
			"SC1 reference count is 1");

	/*
	 * User C is the sole owner of an object and is keeping the whole
	 * trace hierarchy "alive" by holding a reference to EC1.
	 */
	ok(bt_object_get_ref_count((void *) weak_tc1) == 1,
			"TC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc1) == 1,
			"SC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_sc2) == 0,
			"SC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec1) == 1,
			"EC1 reference count is 1");
	ok(bt_object_get_ref_count((void *) weak_ec2) == 0,
			"EC2 reference count is 0");
	ok(bt_object_get_ref_count((void *) weak_ec3) == 0,
			"EC3 reference count is 0");

	/* Reclaim last reference held by User C. */
	BT_PUT(user_c.ec);
}

static void create_writer_user_full(struct writer_user *user)
{
	gchar *trace_path;
	struct bt_ctf_field_type *ft;
	struct bt_ctf_field *field;
	struct bt_ctf_clock *clock;
	int ret;

	trace_path = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);
	if (!bt_mkdtemp(trace_path)) {
		perror("# perror");
	}

	user->writer = bt_ctf_writer_create(trace_path);
	BT_ASSERT(user->writer);
	ret = bt_ctf_writer_set_byte_order(user->writer,
		BT_CTF_BYTE_ORDER_LITTLE_ENDIAN);
	BT_ASSERT(ret == 0);
	user->tc = bt_ctf_writer_get_trace(user->writer);
	BT_ASSERT(user->tc);
	user->sc = bt_ctf_stream_class_create("sc");
	BT_ASSERT(user->sc);
	clock = bt_ctf_clock_create("the_clock");
	BT_ASSERT(clock);
	ret = bt_ctf_writer_add_clock(user->writer, clock);
	BT_ASSERT(!ret);
	ret = bt_ctf_stream_class_set_clock(user->sc, clock);
	BT_ASSERT(!ret);
	BT_PUT(clock);
	user->stream = bt_ctf_writer_create_stream(user->writer, user->sc);
	BT_ASSERT(user->stream);
	user->ec = bt_ctf_event_class_create("ec");
	BT_ASSERT(user->ec);
	ft = create_writer_integer_struct();
	BT_ASSERT(ft);
	ret = bt_ctf_event_class_set_payload_field_type(user->ec, ft);
	BT_PUT(ft);
	BT_ASSERT(!ret);
	ret = bt_ctf_stream_class_add_event_class(user->sc, user->ec);
	BT_ASSERT(!ret);
	user->event = bt_ctf_event_create(user->ec);
	BT_ASSERT(user->event);
	field = bt_ctf_event_get_payload(user->event, "payload_8");
	BT_ASSERT(field);
	ret = bt_ctf_field_integer_unsigned_set_value(field, 10);
	BT_ASSERT(!ret);
	BT_PUT(field);
	field = bt_ctf_event_get_payload(user->event, "payload_16");
	BT_ASSERT(field);
	ret = bt_ctf_field_integer_unsigned_set_value(field, 20);
	BT_ASSERT(!ret);
	BT_PUT(field);
	field = bt_ctf_event_get_payload(user->event, "payload_32");
	BT_ASSERT(field);
	ret = bt_ctf_field_integer_unsigned_set_value(field, 30);
	BT_ASSERT(!ret);
	BT_PUT(field);
	ret = bt_ctf_stream_append_event(user->stream, user->event);
	BT_ASSERT(!ret);
	recursive_rmdir(trace_path);
	g_free(trace_path);
}

static void test_put_order_swap(size_t *array, size_t a, size_t b)
{
	size_t temp = array[a];

	array[a] = array[b];
	array[b] = temp;
}

static void test_put_order_put_objects(size_t *array, size_t size)
{
	size_t i;
	struct writer_user user = { 0 };
	void **objects = (void *) &user;

	create_writer_user_full(&user);
	printf("# ");

	for (i = 0; i < size; ++i) {
		void *obj = objects[array[i]];

		printf("%s", writer_user_names[array[i]]);
		BT_PUT(obj);

		if (i < size - 1) {
			printf(" -> ");
		}
	}

	puts("");
}

static void test_put_order_permute(size_t *array, int k, size_t size)
{
	if (k == 0) {
		test_put_order_put_objects(array, size);
	} else {
		int i;

		for (i = k - 1; i >= 0; i--) {
			size_t next_k = k - 1;

			test_put_order_swap(array, i, next_k);
			test_put_order_permute(array, next_k, size);
			test_put_order_swap(array, i, next_k);
		}
	}
}

static void test_put_order(void)
{
	size_t i;
	size_t array[WRITER_USER_NR_ELEMENTS];

	/* Initialize array of indexes */
	for (i = 0; i < WRITER_USER_NR_ELEMENTS; ++i) {
		array[i] = i;
	}

	test_put_order_permute(array, WRITER_USER_NR_ELEMENTS,
		WRITER_USER_NR_ELEMENTS);
}

/**
 * The objective of this test is to implement and expand upon the scenario
 * described in the reference counting documentation and ensure that any node of
 * the Trace, Stream Class, Event Class, Stream and Event hiearchy keeps all
 * other "alive" and reachable.
 *
 * External tools (e.g. valgrind) should be used to confirm that this
 * known-good test does not leak memory.
 */
int main(int argc, char **argv)
{
	/* Initialize tap harness before any tests */
	plan_tests(NR_TESTS);

	test_example_scenario();
	test_put_order();

	return exit_status();
}
