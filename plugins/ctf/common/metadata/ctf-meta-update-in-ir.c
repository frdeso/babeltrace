/*
 * Copyright 2018 - Philippe Proulx <pproulx@efficios.com>
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
 */

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-UPDATE-IN-IR"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-update-in-ir.h"

static
int update_field_type_in_ir(struct ctf_field_type *ft)
{
	int ret = 0;
	uint64_t i;

	if (!ft) {
		goto end;
	}

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_INT:
	case CTF_FIELD_TYPE_ID_ENUM:
	{
		struct ctf_field_type_int *int_ft = (void *) ft;

		if (int_ft->meaning != CTF_FIELD_TYPE_MEANING_NONE) {
			ft->in_ir = false;
			goto end;
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_field_type_struct *struct_ft = (void *) ft;
		bool in_ir = false;

		for (i = 0; i < struct_ft->members->len; i++) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_struct_borrow_member_by_index(
					struct_ft, i);

			ret = update_field_type_in_ir(named_ft->ft);
			if (ret) {
				goto end;
			}

			if (named_ft->ft->in_ir) {
				in_ir = true;
			}
		}

		ft->in_ir = in_ir;
		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_named_field_type *named_ft;
		struct ctf_field_type_variant *var_ft = (void *) ft;
		bool in_ir = false;

		for (i = 0; i < var_ft->options->len; i++) {
			named_ft =
				ctf_field_type_variant_borrow_option_by_index(
					var_ft, i);

			ret = update_field_type_in_ir(named_ft->ft);
			if (ret) {
				goto end;
			}

			if (named_ft->ft->in_ir) {
				in_ir = true;
			}
		}

		ft->in_ir = in_ir;

		if (in_ir) {
			/*
			 * At least one option will make it to IR. In
			 * this case, make all options part of IR
			 * because the variant's tag could still select
			 * a removed option. This can mean having an
			 * empty structure as an option, but at least
			 * the option exists.
			 */
			for (i = 0; i < var_ft->options->len; i++) {
				ctf_field_type_variant_borrow_option_by_index(
					var_ft, i)->ft->in_ir = true;
			}
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_ARRAY:
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_array_base *array_ft = (void *) ft;

		ret = update_field_type_in_ir(array_ft->elem_ft);
		if (ret) {
			goto end;
		}

		ft->in_ir = array_ft->elem_ft->in_ir;
		if (ft->id == CTF_FIELD_TYPE_ID_ARRAY) {
			struct ctf_field_type_array *arr_ft = (void *) ft;

			if (arr_ft->meaning != CTF_FIELD_TYPE_MEANING_NONE) {
				ft->in_ir = false;
			}
		}

		break;
	}
	default:
		break;
	}

end:
	return ret;
}

static
int update_event_class_in_ir(struct ctf_event_class *ec)
{
	int ret = 0;

	if (ec->is_translated) {
		goto end;
	}

	ret = update_field_type_in_ir(ec->spec_context_ft);
	if (ret) {
		goto end;
	}

	ret = update_field_type_in_ir(ec->payload_ft);
	if (ret) {
		goto end;
	}

end:
	return ret;
}

static
int update_stream_class_in_ir(struct ctf_stream_class *sc)
{
	int ret = 0;
	uint64_t i;

	if (!sc->is_translated) {
		ret = update_field_type_in_ir(sc->packet_context_ft);
		if (ret) {
			goto end;
		}

		ret = update_field_type_in_ir(sc->event_header_ft);
		if (ret) {
			goto end;
		}

		ret = update_field_type_in_ir(sc->event_common_context_ft);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < sc->event_classes->len; i++) {
		struct ctf_event_class *ec = sc->event_classes->pdata[i];

		ret = update_event_class_in_ir(ec);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_update_in_ir(struct ctf_trace_class *ctf_tc)
{
	int ret = 0;
	uint64_t i;

	if (!ctf_tc->is_translated) {
		ret = update_field_type_in_ir(ctf_tc->packet_header_ft);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = ctf_tc->stream_classes->pdata[i];

		ret = update_stream_class_in_ir(sc);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}
