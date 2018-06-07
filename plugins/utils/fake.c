#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>
#include <glib.h>

struct fake_component;

struct fake_notif_iter {
	struct fake_component *fake_comp;
	struct bt_private_connection_private_notification_iterator *pc_notif_iter;
	struct bt_packet *packet;
	enum bt_packet_previous_packet_availability prev_packet_avail;
	struct bt_packet *prev_packet;
	size_t at;
	size_t event_count;

	enum {
		STATE_EMIT_STREAM_BEGINNING = 0,
		STATE_EMIT_PACKET_BEGINNING,
		STATE_EMIT_EVENT,
		STATE_EMIT_PACKET_END,
		STATE_EMIT_STREAM_END,
		STATE_DONE,
	} state;
};

struct fake_component {
	size_t event_count;
	struct bt_trace *trace;
	struct bt_stream_class *stream_class;
	struct bt_event_class *event_class;
	struct bt_stream *stream;
};

static
struct bt_field_type *create_packet_header_ft(void)
{
	struct bt_field_type *root_ft = NULL;
	struct bt_field_type *ft = NULL;
	int ret;

	root_ft = bt_field_type_structure_create();
	BT_ASSERT(root_ft);
	ft = bt_field_type_integer_create(32);
	BT_ASSERT(ft);
	ret = bt_field_type_structure_add_field(root_ft, ft, "magic");
	BT_ASSERT(ret == 0);
	bt_put(ft);
	return root_ft;
}

static
struct bt_field_type *create_packet_context_ft(void)
{
	struct bt_field_type *root_ft = NULL;
	struct bt_field_type *ft = NULL;
	int ret;

	root_ft = bt_field_type_structure_create();
	BT_ASSERT(root_ft);
	ft = bt_field_type_integer_create(32);
	BT_ASSERT(ft);
	ret = bt_field_type_structure_add_field(root_ft, ft, "packet_size");
	BT_ASSERT(ret == 0);
	bt_put(ft);
	ft = bt_field_type_integer_create(32);
	BT_ASSERT(ft);
	ret = bt_field_type_structure_add_field(root_ft, ft, "content_size");
	BT_ASSERT(ret == 0);
	bt_put(ft);
	return root_ft;
}

static
struct bt_field_type *create_event_payload_ft(void)
{
	struct bt_field_type *root_ft = NULL;
	struct bt_field_type *ft = NULL;
	int ret;

	root_ft = bt_field_type_structure_create();
	BT_ASSERT(root_ft);
	ft = bt_field_type_integer_create(64);
	BT_ASSERT(ft);
	ret = bt_field_type_structure_add_field(root_ft, ft, "customer");
	BT_ASSERT(ret == 0);
	bt_put(ft);
	ft = bt_field_type_integer_create(64);
	BT_ASSERT(ft);
	ret = bt_field_type_structure_add_field(root_ft, ft, "limited");
	BT_ASSERT(ret == 0);
	bt_put(ft);
	ft = bt_field_type_string_create();
	BT_ASSERT(ft);
	ret = bt_field_type_structure_add_field(root_ft, ft, "knit");
	BT_ASSERT(ret == 0);
	bt_put(ft);
	ft = bt_field_type_integer_create(32);
	BT_ASSERT(ft);
	ret = bt_field_type_structure_add_field(root_ft, ft, "twin");
	BT_ASSERT(ret == 0);
	bt_put(ft);
	return root_ft;
}

static
void create_meta(struct fake_component *fake_comp)
{
	struct bt_field_type *ft = NULL;
	int ret;

	fake_comp->trace = bt_trace_create();
	BT_ASSERT(fake_comp->trace);
	ft = create_packet_header_ft();
	BT_ASSERT(ft);
	ret = bt_trace_set_packet_header_field_type(fake_comp->trace, ft);
	BT_ASSERT(ret == 0);
	bt_put(ft);
	ret = bt_trace_set_name(fake_comp->trace, "fake!");
	BT_ASSERT(ret == 0);
	fake_comp->stream_class = bt_stream_class_create(NULL);
	BT_ASSERT(fake_comp->stream_class);
	ft = create_packet_context_ft();
	BT_ASSERT(ft);
	ret = bt_stream_class_set_packet_context_field_type(
		fake_comp->stream_class, ft);
	BT_ASSERT(ret == 0);
	bt_put(ft);
	fake_comp->event_class = bt_event_class_create("the_event");
	BT_ASSERT(fake_comp->event_class);
	ft = create_event_payload_ft();
	BT_ASSERT(ft);
	ret = bt_event_class_set_payload_field_type(fake_comp->event_class, ft);
	BT_ASSERT(ret == 0);
	bt_put(ft);
	ret = bt_stream_class_add_event_class(fake_comp->stream_class,
		fake_comp->event_class);
	BT_ASSERT(ret == 0);
	ret = bt_trace_add_stream_class(fake_comp->trace,
		fake_comp->stream_class);
	BT_ASSERT(ret == 0);
}

static
void handle_params(struct fake_component *fake_comp, struct bt_value *params)
{
	struct bt_value *count_param = NULL;
	int ret;

	fake_comp->event_count = 5000;
	count_param = bt_value_map_borrow(params, "count");
	if (count_param) {
		int64_t count;

		BT_ASSERT(bt_value_is_integer(count_param));
		ret = bt_value_integer_get(count_param, &count);
		BT_ASSERT(ret == 0);
		BT_ASSERT(count > 0);
		fake_comp->event_count = (size_t) count;
	}
}

static
void fill_packet_fields(struct bt_packet *packet)
{
	struct bt_field *root = NULL;
	struct bt_field *field = NULL;
	int ret;

	root = bt_packet_borrow_header(packet);
	BT_ASSERT(root);
	field = bt_field_structure_borrow_field_by_index(root, 0);
	BT_ASSERT(field);
	ret = bt_field_integer_unsigned_set_value(field, 0xc1fc1fc1);
	BT_ASSERT(ret == 0);
	root = bt_packet_borrow_context(packet);
	BT_ASSERT(root);
	field = bt_field_structure_borrow_field_by_index(root, 0);
	BT_ASSERT(field);
	ret = bt_field_integer_unsigned_set_value(field, 0);
	BT_ASSERT(ret == 0);
	field = bt_field_structure_borrow_field_by_index(root, 1);
	BT_ASSERT(field);
	ret = bt_field_integer_unsigned_set_value(field, 0);
	BT_ASSERT(ret == 0);
}

static
void fill_event_payload_field(struct bt_event *event)
{
	struct bt_field *ep = NULL;
	struct bt_field *field = NULL;
	int ret;

	ep = bt_event_borrow_payload(event);
	BT_ASSERT(ep);
	field = bt_field_structure_borrow_field_by_index(ep, 0);
	BT_ASSERT(field);
	ret = bt_field_integer_unsigned_set_value(field,
		UINT64_C(0xabcd1234cdef5678));
	BT_ASSERT(ret == 0);
	field = bt_field_structure_borrow_field_by_index(ep, 1);
	BT_ASSERT(field);
	ret = bt_field_integer_unsigned_set_value(field,
		UINT64_C(0xbeefbeefbeefbeef));
	BT_ASSERT(ret == 0);
	field = bt_field_structure_borrow_field_by_index(ep, 2);
	BT_ASSERT(field);
	ret = bt_field_string_set_value(field, "hello world!");
	BT_ASSERT(ret == 0);
	field = bt_field_structure_borrow_field_by_index(ep, 3);
	BT_ASSERT(field);
	ret = bt_field_integer_unsigned_set_value(field,
		UINT64_C(0xdeadc0de));
	BT_ASSERT(ret == 0);
}

static
void create_stream(struct fake_component *fake_comp)
{
	int ret;

	fake_comp->stream = bt_stream_create(fake_comp->stream_class,
		NULL, 0);
	BT_ASSERT(fake_comp->stream);
	ret = bt_trace_set_is_static(fake_comp->trace);
	BT_ASSERT(ret == 0);
}

static
void switch_packet(struct fake_notif_iter *fake_notif_iter)
{
	BT_MOVE(fake_notif_iter->prev_packet, fake_notif_iter->packet);

	if (fake_notif_iter->prev_packet) {
		fake_notif_iter->prev_packet_avail =
			BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_AVAILABLE;
	}

	fake_notif_iter->packet = bt_packet_create(
		fake_notif_iter->fake_comp->stream,
		fake_notif_iter->prev_packet_avail,
		fake_notif_iter->prev_packet);
	BT_ASSERT(fake_notif_iter->packet);
	fill_packet_fields(fake_notif_iter->packet);
}

static
void destroy_fake_component(struct fake_component *fake_comp)
{
	if (!fake_comp) {
		return;
	}

	bt_put(fake_comp->trace);
	bt_put(fake_comp->stream_class);
	bt_put(fake_comp->event_class);
	bt_put(fake_comp->stream);
	g_free(fake_comp);
}

static
void create_port(struct bt_private_component *priv_comp)
{
	int ret;

	ret = bt_private_component_source_add_output_private_port(priv_comp,
		"out", NULL, NULL);
	BT_ASSERT(ret == 0);
}

BT_HIDDEN
enum bt_component_status fake_init(struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_method_data)
{
	struct fake_component *fake_comp = g_new0(struct fake_component, 1);

	BT_ASSERT(fake_comp);
	handle_params(fake_comp, params);
	create_meta(fake_comp);
	create_stream(fake_comp);
	create_port(priv_comp);
	(void) bt_private_component_set_user_data(priv_comp, fake_comp);
	return BT_COMPONENT_STATUS_OK;
}

BT_HIDDEN
void fake_finalize(struct bt_private_component *priv_comp)
{
	void *data = bt_private_component_get_user_data(priv_comp);

	destroy_fake_component(data);
}

static
void destroy_fake_notif_iter(struct fake_notif_iter *fake_notif_iter)
{
	if (!fake_notif_iter) {
		return;
	}

	bt_put(fake_notif_iter->packet);
	g_free(fake_notif_iter);
}

BT_HIDDEN
enum bt_notification_iterator_status fake_notif_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *priv_port)
{
	struct bt_private_component *priv_comp = NULL;
	struct fake_component *fake_comp;
	struct fake_notif_iter *fake_notif_iter =
		g_new0(struct fake_notif_iter, 1);

	BT_ASSERT(fake_notif_iter);
	priv_comp = bt_private_connection_private_notification_iterator_get_private_component(
		priv_notif_iter);
	BT_ASSERT(priv_comp);
	bt_put(priv_comp);
	fake_comp = bt_private_component_get_user_data(priv_comp);
	BT_ASSERT(fake_comp);
	fake_notif_iter->fake_comp = fake_comp;
	fake_notif_iter->event_count = fake_comp->event_count;
	fake_notif_iter->pc_notif_iter = priv_notif_iter;
	fake_notif_iter->prev_packet_avail =
		BT_PACKET_PREVIOUS_PACKET_AVAILABILITY_NONE;
	(void) bt_private_connection_private_notification_iterator_set_user_data(
		priv_notif_iter, fake_notif_iter);
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

BT_HIDDEN
void fake_notif_iter_finalize(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter)
{
	destroy_fake_notif_iter(
		bt_private_connection_private_notification_iterator_get_user_data(
			priv_notif_iter));
}

static inline
void do_next(struct fake_notif_iter *fake_notif_iter,
		struct bt_notification **notif)
{
	BT_ASSERT(fake_notif_iter);

	switch (fake_notif_iter->state) {
	case STATE_EMIT_STREAM_BEGINNING:
		*notif = bt_notification_stream_begin_create(
			fake_notif_iter->pc_notif_iter,
			fake_notif_iter->fake_comp->stream);
		fake_notif_iter->state = STATE_EMIT_PACKET_BEGINNING;
		break;
	case STATE_EMIT_PACKET_BEGINNING:
		switch_packet(fake_notif_iter);
		*notif = bt_notification_packet_begin_create(
			fake_notif_iter->pc_notif_iter, fake_notif_iter->packet);
		fake_notif_iter->state = STATE_EMIT_EVENT;
		break;
	case STATE_EMIT_EVENT:
		*notif = bt_notification_event_create(
			fake_notif_iter->pc_notif_iter,
			fake_notif_iter->fake_comp->event_class,
			fake_notif_iter->packet);
		fill_event_payload_field(
			bt_notification_event_borrow_event(
				*notif));
		fake_notif_iter->at++;

		if (fake_notif_iter->at % 5000 == 0) {
			fake_notif_iter->state = STATE_EMIT_PACKET_END;
		}

		if (fake_notif_iter->at == fake_notif_iter->event_count) {
			fake_notif_iter->state = STATE_EMIT_PACKET_END;
		}

		break;
	case STATE_EMIT_PACKET_END:
		*notif = bt_notification_packet_end_create(
			fake_notif_iter->pc_notif_iter,
			fake_notif_iter->packet);

		if (fake_notif_iter->at == fake_notif_iter->event_count) {
			fake_notif_iter->state = STATE_EMIT_STREAM_END;
		} else {
			fake_notif_iter->state = STATE_EMIT_PACKET_BEGINNING;
		}

		break;
	case STATE_EMIT_STREAM_END:
		*notif = bt_notification_stream_end_create(
			fake_notif_iter->pc_notif_iter,
			fake_notif_iter->fake_comp->stream);
		fake_notif_iter->state = STATE_DONE;
		break;
	default:
		break;
	}
}

BT_HIDDEN
enum bt_notification_iterator_status fake_notif_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	struct fake_notif_iter *fake_notif_iter =
		bt_private_connection_private_notification_iterator_get_user_data(
			priv_notif_iter);
	uint64_t i = 0;

	if (fake_notif_iter->state == STATE_DONE) {
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		goto end;
	}

	while (i < capacity && fake_notif_iter->state != STATE_DONE) {
		do_next(fake_notif_iter, &notifs[i]);
		i++;
	}

	BT_ASSERT(i > 0);
	*count = i;

end:
	return status;
}
