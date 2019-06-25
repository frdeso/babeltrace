/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%include <babeltrace2/trace-ir/trace-class-const.h>
%include <babeltrace2/trace-ir/trace-class.h>

/* Helper functions for Python */
%{
static void
trace_class_destroyed_listener(const bt_trace_class *trace_class, void *py_callable)
{
	PyObject *py_trace_class_ptr = NULL;
	PyObject *py_res = NULL;

	py_trace_class_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(trace_class),
		SWIGTYPE_p_bt_trace_class, 0);
	if (!py_trace_class_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(O)", py_trace_class_ptr);
	if (py_res != NULL) {
		BT_ASSERT(py_res == Py_None);
	} else {
		bt2_py_loge_exception();
	}

	Py_DECREF(py_trace_class_ptr);
	Py_XDECREF(py_res);
}

uint64_t bt_py3_trace_class_add_destruction_listener(bt_trace_class *trace_class,
	PyObject *py_callable)
{
	uint64_t id = UINT64_C(-1);
	bt_trace_class_status status;

	BT_ASSERT(trace_class);
	BT_ASSERT(py_callable);

	status = bt_trace_class_add_destruction_listener(
		trace_class, trace_class_destroyed_listener, py_callable, &id);
	if (status != BT_TRACE_CLASS_STATUS_OK) {
		BT_LOGF_STR("Failed to add trace class destruction listener.");
		abort();
	}

	Py_INCREF(py_callable);

	return id;
}
%}

uint64_t bt_py3_trace_class_add_destruction_listener(bt_trace_class *trace_class,
	PyObject *py_callable);