/*
 * clock-class.c
 *
 * Babeltrace CTF Writer - Clock Class
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
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

#define BT_LOG_TAG "CTF-WRITER-CLOCK-CLASS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-writer/clock-class.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <inttypes.h>

BT_HIDDEN
void bt_ctf_clock_class_serialize(struct bt_ctf_clock_class *clock_class,
		struct metadata_context *context)
{
	unsigned char *uuid;

	BT_LOGD("Serializing clock class's metadata: clock-class-addr=%p, "
		"name=\"%s\", metadata-context-addr=%p", clock_class,
		bt_clock_class_get_name(BT_TO_COMMON(clock_class)),
		context);

	if (!clock_class || !context) {
		BT_LOGW("Invalid parameter: clock class or metadata context is NULL: "
			"clock-class-addr=%p, name=\"%s\", metadata-context-addr=%p",
			clock_class,
			bt_clock_class_get_name(BT_TO_COMMON(clock_class)),
			context);
		return;
	}

	uuid = clock_class->common.uuid;
	g_string_append(context->string, "clock {\n");
	g_string_append_printf(context->string, "\tname = %s;\n",
		clock_class->common.name->str);

	if (clock_class->common.uuid_set) {
		g_string_append_printf(context->string,
			"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
			uuid[0], uuid[1], uuid[2], uuid[3],
			uuid[4], uuid[5], uuid[6], uuid[7],
			uuid[8], uuid[9], uuid[10], uuid[11],
			uuid[12], uuid[13], uuid[14], uuid[15]);
	}

	if (clock_class->common.description) {
		g_string_append_printf(context->string, "\tdescription = \"%s\";\n",
			clock_class->common.description->str);
	}

	g_string_append_printf(context->string, "\tfreq = %" PRIu64 ";\n",
		clock_class->common.frequency);
	g_string_append_printf(context->string, "\tprecision = %" PRIu64 ";\n",
		clock_class->common.precision);
	g_string_append_printf(context->string, "\toffset_s = %" PRIu64 ";\n",
		clock_class->common.offset_s);
	g_string_append_printf(context->string, "\toffset = %" PRIu64 ";\n",
		clock_class->common.offset);
	g_string_append_printf(context->string, "\tabsolute = %s;\n",
		clock_class->common.absolute ? "true" : "false");
	g_string_append(context->string, "};\n\n");
}

struct bt_ctf_clock_class *bt_ctf_clock_class_create(const char *name,
                uint64_t freq)
{
	return BT_FROM_COMMON(bt_clock_class_create(name, freq));
}

const char *bt_ctf_clock_class_get_name(
                struct bt_ctf_clock_class *clock_class)
{
	return bt_clock_class_get_name(BT_TO_COMMON(clock_class));
}

int bt_ctf_clock_class_set_name(struct bt_ctf_clock_class *clock_class,
                const char *name)
{
	return bt_clock_class_set_name(BT_TO_COMMON(clock_class), name);
}

const char *bt_ctf_clock_class_get_description(
                struct bt_ctf_clock_class *clock_class)
{
	return bt_clock_class_get_description(BT_TO_COMMON(clock_class));
}

int bt_ctf_clock_class_set_description(
                struct bt_ctf_clock_class *clock_class,
                const char *desc)
{
	return bt_clock_class_set_description(BT_TO_COMMON(clock_class), desc);
}

uint64_t bt_ctf_clock_class_get_frequency(
                struct bt_ctf_clock_class *clock_class)
{
	return bt_clock_class_get_frequency(BT_TO_COMMON(clock_class));
}

int bt_ctf_clock_class_set_frequency(
                struct bt_ctf_clock_class *clock_class, uint64_t freq)
{
	return bt_clock_class_set_frequency(BT_TO_COMMON(clock_class), freq);
}

uint64_t bt_ctf_clock_class_get_precision(
                struct bt_ctf_clock_class *clock_class)
{
	return bt_clock_class_get_precision(BT_TO_COMMON(clock_class));
}

int bt_ctf_clock_class_set_precision(
                struct bt_ctf_clock_class *clock_class, uint64_t precision)
{
	return bt_clock_class_set_precision(BT_TO_COMMON(clock_class),
		precision);
}

int bt_ctf_clock_class_get_offset_s(
                struct bt_ctf_clock_class *clock_class, int64_t *seconds)
{
	return bt_clock_class_get_offset_s(BT_TO_COMMON(clock_class), seconds);
}

int bt_ctf_clock_class_set_offset_s(
                struct bt_ctf_clock_class *clock_class, int64_t seconds)
{
	return bt_clock_class_set_offset_s(BT_TO_COMMON(clock_class), seconds);
}

int bt_ctf_clock_class_get_offset_cycles(
                struct bt_ctf_clock_class *clock_class, int64_t *cycles)
{
	return bt_clock_class_get_offset_cycles(BT_TO_COMMON(clock_class),
		cycles);
}

int bt_ctf_clock_class_set_offset_cycles(
                struct bt_ctf_clock_class *clock_class, int64_t cycles)
{
	return bt_clock_class_set_offset_cycles(BT_TO_COMMON(clock_class),
		cycles);
}

bt_bool bt_ctf_clock_class_is_absolute(
                struct bt_ctf_clock_class *clock_class)
{
	return bt_clock_class_is_absolute(BT_TO_COMMON(clock_class));
}

int bt_ctf_clock_class_set_is_absolute(
                struct bt_ctf_clock_class *clock_class, bt_bool is_absolute)
{
	return bt_clock_class_set_is_absolute(BT_TO_COMMON(clock_class),
		is_absolute);
}

const unsigned char *bt_ctf_clock_class_get_uuid(
                struct bt_ctf_clock_class *clock_class)
{
	return bt_clock_class_get_uuid(BT_TO_COMMON(clock_class));
}

int bt_ctf_clock_class_set_uuid(struct bt_ctf_clock_class *clock_class,
                const unsigned char *uuid)
{
	return bt_clock_class_set_uuid(BT_TO_COMMON(clock_class), uuid);
}
