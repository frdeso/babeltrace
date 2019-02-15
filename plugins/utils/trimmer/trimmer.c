/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-UTILS-TRIMMER-FLT"
#include "logging.h"

#include <babeltrace/compat/utc-internal.h>
#include <babeltrace/compat/time-internal.h>
#include <babeltrace/babeltrace.h>
#include <plugins-common.h>
#include <babeltrace/assert-internal.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>

#include "trimmer.h"

#define NS_PER_S	INT64_C(1000000000)

static const char * const in_port_name = "in";

struct trimmer_time {
	unsigned int hour, minute, second, ns;
};

struct trimmer_bound {
	int64_t ns_from_origin;
	bool is_ns_from_origin_set;
	struct trimmer_time time;
};

struct trimmer_comp {
	struct trimmer_bound begin, end;
	bool is_gmt;
};

enum trimmer_iterator_state {
	TRIMMER_ITERATOR_STATE_SET_BOUNDS_NS_FROM_ORIGIN,
	TRIMMER_ITERATOR_STATE_INITIAL_SEEK,
	TRIMMER_ITERATOR_STATE_TRIM,
	TRIMMER_ITERATOR_STATE_ENDED,
};

struct trimmer_iterator {
	struct trimmer_comp *trimmer_comp;
	enum trimmer_iterator_state state;
	bt_self_component_port_input_message_iterator *upstream_iter;
	struct trimmer_bound begin, end;
	GQueue *output_messages;
	GHashTable *stream_states;
};

struct trimmer_iterator_stream_state {

};

static
void destroy_trimmer_comp(struct trimmer_comp *trimmer_comp)
{
	g_free(trimmer_comp);
}

static
struct trimmer_comp *create_trimmer_comp(void)
{
	return g_new0(struct trimmer_comp, 1);
}

BT_HIDDEN
void trimmer_finalize(bt_self_component_filter *self_comp)
{
	struct trimmer_comp *trimmer_comp =
		bt_self_component_get_data(
			bt_self_component_filter_as_self_component(self_comp));

	if (trimmer_comp) {
		destroy_trimmer_comp(trimmer_comp);
	}
}

static
int set_date_and_time(struct trimmer_bound *bound,
		unsigned int year, unsigned int month, unsigned int day,
		unsigned int hour, unsigned int minute, unsigned int second,
		unsigned int ns, bool is_gmt)
{
	int ret = 0;
	time_t result;
	struct tm tm = {
		.tm_sec = second,
		.tm_min = minute,
		.tm_hour = hour,
		.tm_mday = day,
		.tm_mon = month - 1,
		.tm_year = year - 1900,
		.tm_isdst = -1,
	};

	if (is_gmt) {
		result = bt_timegm(&tm);
	} else {
		result = mktime(&tm);
	}

	if (result < 0) {
		ret = -1;
		goto end;
	}

	bound->ns_from_origin = (int64_t) result;
	bound->ns_from_origin *= NS_PER_S;
	bound->ns_from_origin += ns;
	bound->is_ns_from_origin_set = true;

end:
	return ret;
}

/*
 * Parses a timestamp, figuring out its format.
 *
 * Returns a negative value if anything goes wrong.
 *
 * Expected formats:
 *
 *     YYYY-MM-DD hh:mm[:ss[.ns]]
 *     [hh:mm:]ss[.ns]
 *     [-]s[.ns]
 */
static
int set_bound_from_str(const char *str, struct trimmer_bound *bound,
		bool is_gmt)
{
	int ret = 0;
	int s_ret;
	unsigned int year, month, day, hour, minute, second, ns;
	char dummy;

	/* Try `YYYY-MM-DD hh:mm:ss.ns` format */
	s_ret = sscanf(str, "%u-%u-%u %u:%u:%u.%u%c", &year, &month, &day, &hour,
		&minute, &second, &ns, &dummy);
	if (s_ret == 7) {
		ret = set_date_and_time(bound, year, month, day,
			hour, minute, second, ns, is_gmt);
		goto end;
	}

	/* Try `YYYY-MM-DD hh:mm:ss` format */
	s_ret = sscanf(str, "%u-%u-%u %u:%u:%u%c", &year, &month, &day, &hour,
		&minute, &second, &dummy);
	if (s_ret == 6) {
		ret = set_date_and_time(bound, year, month, day,
			hour, minute, second, 0, is_gmt);
		goto end;
	}

	/* Try `YYYY-MM-DD hh:mm` format */
	s_ret = sscanf(str, "%u-%u-%u %u:%u%c", &year, &month, &day, &hour,
		&minute, &dummy);
	if (s_ret == 5) {
		ret = set_date_and_time(bound, year, month, day,
			hour, minute, 0, 0, is_gmt);
		goto end;
	}

	/* Try `YYYY-MM-DD` format */
	s_ret = sscanf(str, "%u-%u-%u%c", &year, &month, &day, &dummy);
	if (s_ret == 3) {
		ret = set_date_and_time(bound, year, month, day,
			0, 0, 0, 0, is_gmt);
		goto end;
	}

	/* Try `hh:mm:ss.ns` format */
	s_ret = sscanf(str, "%u:%u:%u.%u%c", &hour, &minute, &second, &ns,
		&dummy);
	if (s_ret == 4) {
		bound->time.hour = hour;
		bound->time.minute = minute;
		bound->time.second = second;
		bound->time.ns = ns;
		goto end;
	}

	/* Try `hh:mm:ss` format */
	s_ret = sscanf(str, "%u:%u:%u%c", &hour, &minute, &second, &dummy);
	if (s_ret == 3) {
		bound->time.hour = hour;
		bound->time.minute = minute;
		bound->time.second = second;
		bound->time.ns = 0;
		goto end;
	}

	/* Try `-s.ns` format */
	s_ret = sscanf(str, "-%u.%u%c", &second, &ns, &dummy);
	if (s_ret == 2) {
		bound->ns_from_origin = -((int64_t) second) * NS_PER_S;
		bound->ns_from_origin -= (int64_t) ns;
		bound->is_ns_from_origin_set = true;
		goto end;
	}

	/* Try `s.ns` format */
	s_ret = sscanf(str, "%u.%u%c", &second, &ns, &dummy);
	if (s_ret == 2) {
		bound->ns_from_origin = ((int64_t) second) * NS_PER_S;
		bound->ns_from_origin += (int64_t) ns;
		bound->is_ns_from_origin_set = true;
		goto end;
	}

	/* Try `-s` format */
	s_ret = sscanf(str, "-%u%c", &second, &dummy);
	if (s_ret == 1) {
		bound->ns_from_origin = -((int64_t) second) * NS_PER_S;
		bound->is_ns_from_origin_set = true;
		goto end;
	}

	/* Try `s` format */
	s_ret = sscanf(str, "%u%c", &second, &dummy);
	if (s_ret == 1) {
		bound->ns_from_origin = (int64_t) second * NS_PER_S;
		bound->is_ns_from_origin_set = true;
		goto end;
	}

	BT_LOGE("Invalid date/time format: param=\"%s\"", str);
	ret = -1;

end:
	return ret;
}

static
int set_bound_from_param(const char *param_name, const bt_value *param,
		struct trimmer_bound *bound, bool is_gmt)
{
	int ret;
	const char *arg;
	char tmp_arg[64];

	if (bt_value_is_integer(param)) {
		int64_t value = bt_value_integer_get(param);

		/*
		 * Just convert it to a temporary string to handle
		 * everything the same way.
		 */
		sprintf(tmp_arg, "%" PRId64, value);
		arg = tmp_arg;
	} else if (bt_value_is_string(param)) {
		arg = bt_value_string_get(param);
	} else {
		BT_LOGE("`%s` parameter must be an integer or a string value.",
			param_name);
		ret = -1;
		goto end;
	}

	ret = set_bound_from_str(arg, bound, is_gmt);

end:
	return ret;
}

static
int init_trimmer_comp_from_params(struct trimmer_comp *trimmer_comp,
		const bt_value *params)
{
	const bt_value *value;
	int ret = 0;

	BT_ASSERT(params);
        value = bt_value_map_borrow_entry_value_const(params, "gmt");
	if (value) {
		trimmer_comp->is_gmt = (bool) bt_value_bool_get(value);
	}

        value = bt_value_map_borrow_entry_value_const(params, "begin");
	if (value) {
		if (set_bound_from_param("begin", value,
				&trimmer_comp->begin, trimmer_comp->is_gmt)) {
			/* set_bound_from_param() logs errors */
			ret = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

        value = bt_value_map_borrow_entry_value_const(params, "end");
	if (value) {
		if (set_bound_from_param("end", value,
				&trimmer_comp->end, trimmer_comp->is_gmt)) {
			/* set_bound_from_param() logs errors */
			ret = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}

end:
	if (trimmer_comp->begin.is_ns_from_origin_set &&
			trimmer_comp->end.is_ns_from_origin_set) {
		if (trimmer_comp->begin.ns_from_origin >
				trimmer_comp->end.ns_from_origin) {
			BT_LOGE("Trimming time range's beginning time is greater than end time: "
				"begin-ns-from-origin=%" PRId64 ", "
				"end-ns-from-origin=%" PRId64,
				trimmer_comp->begin.ns_from_origin,
				trimmer_comp->end.ns_from_origin);
			ret = BT_SELF_COMPONENT_STATUS_ERROR;
		}
	}

	return ret;
}

bt_self_component_status trimmer_init(bt_self_component_filter *self_comp,
		const bt_value *params, void *init_data)
{
	int ret;
	bt_self_component_status status;
	struct trimmer_comp *trimmer_comp = create_trimmer_comp();

	if (!trimmer_comp) {
		ret = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto error;
	}

	status = bt_self_component_filter_add_input_port(
		self_comp, in_port_name, NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	status = bt_self_component_filter_add_output_port(
		self_comp, "out", NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = init_trimmer_comp_from_params(trimmer_comp, params);
	if (ret) {
		goto error;
	}

	bt_self_component_set_data(
		bt_self_component_filter_as_self_component(self_comp),
		trimmer_comp);
	goto end;

error:
	if (status == BT_SELF_COMPONENT_STATUS_OK) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}

	if (trimmer_comp) {
		destroy_trimmer_comp(trimmer_comp);
	}

end:
	return ret;
}

static
void destroy_trimmer_iterator(struct trimmer_iterator *trimmer_it)
{
	BT_ASSERT(trimmer_it);
	bt_self_component_port_input_message_iterator_put_ref(
		trimmer_it->upstream_iter);

	if (trimmer_it->output_messages) {
		g_queue_free(trimmer_it->output_messages);
	}

	if (trimmer_it->stream_states) {
		g_hash_table_destroy(trimmer_it->stream_states);
	}

	g_free(trimmer_it);
}

static
void destroy_trimmer_iterator_stream_state(
		struct trimmer_iterator_stream_state *sstate)
{

}

BT_HIDDEN
bt_self_message_iterator_status trimmer_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_filter *self_comp,
		bt_self_component_port_output *port)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	struct trimmer_iterator *trimmer_it;

	trimmer_it = g_new0(struct trimmer_iterator, 1);
	if (!trimmer_it) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	trimmer_it->trimmer_comp = bt_self_component_get_data(
		bt_self_component_filter_as_self_component(self_comp));
	BT_ASSERT(trimmer_it->trimmer_comp);

	if (trimmer_it->trimmer_comp->begin.is_ns_from_origin_set &&
			trimmer_it->trimmer_comp->end.is_ns_from_origin_set) {
		trimmer_it->state = TRIMMER_ITERATOR_STATE_INITIAL_SEEK;
	}

	trimmer_it->begin = trimmer_it->trimmer_comp->begin;
	trimmer_it->end = trimmer_it->trimmer_comp->end;
	trimmer_it->upstream_iter =
		bt_self_component_port_input_message_iterator_create(
			bt_self_component_filter_borrow_input_port_by_name(
				self_comp, in_port_name));
	if (!trimmer_it->upstream_iter) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	trimmer_it->output_messages = g_queue_new();
	if (!trimmer_it->output_messages) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	trimmer_it->stream_states = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL,
		(GDestroyNotify) destroy_trimmer_iterator_stream_state);
	if (!trimmer_it->stream_states) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

end:
	if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK && trimmer_it) {
		destroy_trimmer_iterator(trimmer_it);
	}

	return status;
}

static inline
int get_msg_ns_from_origin(const bt_message *msg, int64_t *ns_from_origin,
		bool *skip)
{
	const bt_clock_class *clock_class = NULL;
	const bt_clock_snapshot *clock_snapshot = NULL;
	bt_clock_snapshot_state cs_state = BT_CLOCK_SNAPSHOT_STATE_KNOWN;
	bt_message_stream_activity_clock_snapshot_state sa_cs_state;
	int ret = 0;

	BT_ASSERT(msg);
	BT_ASSERT(ns_from_origin);
	BT_ASSERT(skip);

	switch (bt_message_get_type(msg)) {
	case BT_MESSAGE_TYPE_EVENT:
		clock_class =
			bt_message_event_borrow_stream_class_default_clock_class_const(
				msg);
		if (!clock_class) {
			goto no_clock_snapshot;
		}

		cs_state = bt_message_event_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
			msg);
		if (!clock_class) {
			goto no_clock_snapshot;
		}

		cs_state = bt_message_packet_beginning_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		bt_message_packet_end_borrow_stream_class_default_clock_class_const(
			msg);
		if (!clock_class) {
			goto no_clock_snapshot;
		}

		cs_state = bt_message_packet_end_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
			msg);
		if (!clock_class) {
			goto no_clock_snapshot;
		}

		cs_state = bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
			msg);
		if (!clock_class) {
			goto no_clock_snapshot;
		}

		cs_state = bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
			msg, &clock_snapshot);
		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
		bt_message_stream_activity_beginning_borrow_stream_class_default_clock_class_const(
			msg);
		if (!clock_class) {
			goto no_clock_snapshot;
		}

		sa_cs_state = bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		if (sa_cs_state != BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN) {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
		bt_message_stream_activity_end_borrow_stream_class_default_clock_class_const(
			msg);
		if (!clock_class) {
			goto no_clock_snapshot;
		}

		sa_cs_state = bt_message_stream_activity_end_borrow_default_clock_snapshot_const(
			msg, &clock_snapshot);
		if (sa_cs_state != BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN) {
			goto no_clock_snapshot;
		}

		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		cs_state =
			bt_message_message_iterator_inactivity_borrow_default_clock_snapshot_const(
				msg, &clock_snapshot);
		break;
	default:
		goto no_clock_snapshot;
	}

	if (cs_state != BT_CLOCK_SNAPSHOT_STATE_KNOWN) {
		BT_LOGE_STR("Unsupported unknown clock snapshot.");
		ret = -1;
		goto end;
	}

	ret = bt_clock_snapshot_get_ns_from_origin(clock_snapshot,
		ns_from_origin);
	if (ret) {
		goto error;
	}

	goto end;

no_clock_snapshot:
	*skip = true;
	goto end;

error:
	ret = -1;

end:
	return ret;
}

static inline
void put_messages(bt_message_array_const msgs, uint64_t count)
{
	uint64_t i;

	for (i = 0; i < count; i++) {
		BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);
	}
}

static inline
int set_trimmer_iterator_bound(struct trimmer_bound *bound,
		int64_t ns_from_origin, bool is_gmt)
{
	struct tm tm;
	time_t time_s = (time_t) (ns_from_origin / NS_PER_S);
	int ret = 0;

	BT_ASSERT(!bound->is_ns_from_origin_set);
	errno = 0;

	if (is_gmt) {
		bt_gmtime_r(&time_s, &tm);
	} else {
		bt_localtime_r(&time_s, &tm);
	}

	if (errno) {
		BT_LOGE_ERRNO("Cannot convert timestamp to date and time",
			"ts=%" PRId64, (int64_t) time_s);
		ret = -1;
		goto end;
	}

	ret = set_date_and_time(bound, tm.tm_year + 1900, tm.tm_mon + 1,
		tm.tm_mday, bound->time.hour, bound->time.minute,
		bound->time.second, bound->time.ns, is_gmt);

end:
	return ret;
}

static
bt_self_message_iterator_status set_trimmer_iterator_bounds(
		struct trimmer_iterator *trimmer_it)
{
	bt_message_iterator_status upstream_iter_status =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	struct trimmer_comp *trimmer_comp = trimmer_it->trimmer_comp;
	bt_message_array_const msgs;
	uint64_t count = 0;
	int64_t ns_from_origin;
	uint64_t i;
	int ret;

	while (true) {
		upstream_iter_status =
			bt_self_component_port_input_message_iterator_next(
				trimmer_it->upstream_iter, &msgs, &count);
		if (upstream_iter_status != BT_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}

		for (i = 0; i < count; i++) {
			const bt_message *msg = msgs[i];
			bool skip = false;
			int ret;

			ret = get_msg_ns_from_origin(msg, &ns_from_origin,
				&skip);
			if (ret) {
				goto error;
			}

			if (skip) {
				continue;
			}

			put_messages(msgs, count);
			goto found;
		}

		put_messages(msgs, count);
	}

found:
	if (!trimmer_it->begin.is_ns_from_origin_set) {
		ret = set_trimmer_iterator_bound(&trimmer_it->begin,
			ns_from_origin, trimmer_comp->is_gmt);
		if (ret) {
			goto error;
		}
	}

	if (!trimmer_it->end.is_ns_from_origin_set) {
		ret = set_trimmer_iterator_bound(&trimmer_it->end,
			ns_from_origin, trimmer_comp->is_gmt);
		if (ret) {
			goto error;
		}
	}

	goto end;

error:
	put_messages(msgs, count);
	upstream_iter_status = BT_MESSAGE_ITERATOR_STATUS_ERROR;

end:
	return (int) upstream_iter_status;
}

static
bt_self_message_iterator_status seek_initially(
		struct trimmer_iterator *trimmer_it)
{
	return BT_MESSAGE_ITERATOR_STATUS_OK;
}

static inline
bt_self_message_iterator_status trim(struct trimmer_iterator *trimmer_it,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	return BT_MESSAGE_ITERATOR_STATUS_OK;
}

BT_HIDDEN
bt_self_message_iterator_status trimmer_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	struct trimmer_iterator *trimmer_it =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(trimmer_it);

	switch (trimmer_it->state) {
	case TRIMMER_ITERATOR_STATE_SET_BOUNDS_NS_FROM_ORIGIN:
		status = set_trimmer_iterator_bounds(trimmer_it);
		if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}

		/* Fall through */
	case TRIMMER_ITERATOR_STATE_INITIAL_SEEK:
		status = seek_initially(trimmer_it);
		if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}

		/* Fall through */
	case TRIMMER_ITERATOR_STATE_TRIM:
		status = trim(trimmer_it, msgs, capacity, count);
		if (status != BT_SELF_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}

		break;
	case TRIMMER_ITERATOR_STATE_ENDED:
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
		break;
	default:
		abort();
	}

end:
	return status;
}

BT_HIDDEN
void trimmer_msg_iter_finalize(bt_self_message_iterator *self_msg_iter)
{
	struct trimmer_iterator *trimmer_it =
		bt_self_message_iterator_get_data(self_msg_iter);

	BT_ASSERT(trimmer_it);
	destroy_trimmer_iterator(trimmer_it);
}
