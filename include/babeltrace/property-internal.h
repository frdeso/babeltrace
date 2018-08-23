#ifndef BABELTRACE_PROPERTY_INTERNAL_H
#define BABELTRACE_PROPERTY_INTERNAL_H

/*
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/assert-internal.h>
#include <babeltrace/property.h>
#include <babeltrace/types.h>
#include <glib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

struct bt_property {
	enum bt_property_availability avail;
};

struct bt_property_bool {
	struct bt_property base;
	bt_bool value;
};

struct bt_property_uint {
	struct bt_property base;
	uint64_t value;
};

struct bt_property_int {
	struct bt_property base;
	int64_t value;
};

struct bt_property_float {
	struct bt_property base;
	double value;
};

struct bt_property_string {
	struct bt_property base;
	GString *value;
};

struct bt_property_uuid {
	struct bt_property base;
	uint8_t value[16];
};

static inline
void bt_property_bool_set(struct bt_property_bool *prop, bt_bool value)
{
	BT_ASSERT(prop);
	prop->base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	prop->value = value;
}

static inline
void bt_property_uint_set(struct bt_property_uint *prop, uint64_t value)
{
	BT_ASSERT(prop);
	prop->base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	prop->value = value;
}

static inline
void bt_property_int_set(struct bt_property_int *prop, int64_t value)
{
	BT_ASSERT(prop);
	prop->base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	prop->value = value;
}

static inline
void bt_property_float_set(struct bt_property_float *prop, double value)
{
	BT_ASSERT(prop);
	prop->base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	prop->value = value;
}

static inline
void bt_property_string_set(struct bt_property_string *prop, const char *value)
{
	BT_ASSERT(prop);
	BT_ASSERT(value);
	prop->base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	g_string_assign(prop->value, value);
}

static inline
void bt_property_uuid_set(struct bt_property_uuid *prop, bt_uuid value)
{
	BT_ASSERT(prop);
	BT_ASSERT(value);
	prop->base.avail = BT_PROPERTY_AVAILABILITY_AVAILABLE;
	memcpy(prop->value, value, 16);
}

static inline
void bt_property_bool_init(struct bt_property_bool *prop,
		enum bt_property_availability avail, bool value)
{
	BT_ASSERT(prop);
	prop->base.avail = avail;
	prop->value = value;
}

static inline
void bt_property_uint_init(struct bt_property_uint *prop,
		enum bt_property_availability avail, uint64_t value)
{
	BT_ASSERT(prop);
	prop->base.avail = avail;
	prop->value = value;
}

static inline
void bt_property_int_init(struct bt_property_int *prop,
		enum bt_property_availability avail, int64_t value)
{
	BT_ASSERT(prop);
	prop->base.avail = avail;
	prop->value = value;
}

static inline
void bt_property_float_init(struct bt_property_float *prop,
		enum bt_property_availability avail, double value)
{
	BT_ASSERT(prop);
	prop->base.avail = avail;
	prop->value = value;
}

static inline
int bt_property_string_init(struct bt_property_string *prop,
		enum bt_property_availability avail, const char *value)
{
	int ret = 0;

	BT_ASSERT(prop);
	BT_ASSERT(!prop->value);
	prop->base.avail = avail;
	prop->value = g_string_new(value);
	if (!prop->value) {
#ifdef BT_LOGE_STR
		BT_LOGE_STR("Failed to allocate a GString.");
#endif

		ret = -1;
		goto end;
	}

end:
	return ret;
}

static inline
void bt_property_string_fini(struct bt_property_string *prop)
{
	BT_ASSERT(prop);

	if (prop->value) {
		g_string_free(prop->value, TRUE);
	}
}

static inline
int bt_property_uuid_init(struct bt_property_uuid *prop,
		enum bt_property_availability avail, bt_uuid value)
{
	BT_ASSERT(prop);
	prop->base.avail = avail;

	if (value) {
		memcpy(prop->value, value, 16);
	}
}

#endif /* BABELTRACE_PROPERTY_INTERNAL_H */
