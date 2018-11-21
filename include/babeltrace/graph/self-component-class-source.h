#ifndef BABELTRACE_GRAPH_SELF_COMPONENT_CLASS_SOURCE_H
#define BABELTRACE_GRAPH_SELF_COMPONENT_CLASS_SOURCE_H

/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_class_source;
struct bt_self_component_class_source;

static inline
struct bt_component_class_source *
bt_self_component_class_source_borrow_component_class_source(
		struct bt_self_component_class_source *self_comp_cls_source)
{
	return (void *) self_comp_cls_source;
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_SELF_COMPONENT_CLASS_SOURCE_H */
