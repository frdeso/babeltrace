# The MIT License (MIT)
#
# Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

__all__ = ['_Event']

import abc
import collections
import copy
import numbers
import bt2
from bt2 import utils
from . import object, fields


class _Event(object._Object):
    @property
    def event_class(self):
        return self._event_class

    @property
    def name(self):
        return self._event_class.name

    @property
    def id(self):
        return self._event_class.id

    @property
    def stream(self):
        stream_ptr = self._Domain.event_get_stream(self._ptr)

        if stream_ptr is None:
            return stream_ptr

        return self._Domain.Stream._create_from_ptr(stream_ptr)

    @property
    def header_field(self):
        field_ptr = self._Domain.event_get_header(self._ptr)

        if field_ptr is None:
            return

        return self._Domain.create_field_from_ptr(field_ptr)

    @header_field.setter
    def header_field(self, header_field):
        header_field_ptr = None

        if header_field is not None:
            utils._check_type(header_field, fields._Field)
            header_field_ptr = header_field._ptr

        ret = self._Domain.event_set_header(self._ptr, header_field_ptr)
        utils._handle_ret(ret, "cannot set event object's header field")

    @property
    def stream_event_context_field(self):
        field_ptr = self._Domain.event_get_stream_event_context(self._ptr)

        if field_ptr is None:
            return

        return self._Domain.create_field_from_ptr(field_ptr)

    @stream_event_context_field.setter
    def stream_event_context_field(self, stream_event_context):
        stream_event_context_ptr = None

        if stream_event_context is not None:
            utils._check_type(stream_event_context, fields._Field)
            stream_event_context_ptr = stream_event_context._ptr

        ret = self._Domain.event_set_stream_event_context(self._ptr,
                                                       stream_event_context_ptr)
        utils._handle_ret(ret, "cannot set event object's stream event context field")

    @property
    def context_field(self):
        field_ptr = self._Domain.event_get_context(self._ptr)

        if field_ptr is None:
            return

        return self._Domain.create_field_from_ptr(field_ptr)

    @context_field.setter
    def context_field(self, context):
        context_ptr = None

        if context is not None:
            utils._check_type(context, fields._Field)
            context_ptr = context._ptr

        ret = self._Domain.event_set_context(self._ptr, context_ptr)
        utils._handle_ret(ret, "cannot set event object's context field")

    @property
    def payload_field(self):
        field_ptr = self._Domain.event_get_payload(self._ptr)

        if field_ptr is None:
            return

        return self._Domain.create_field_from_ptr(field_ptr)

    @payload_field.setter
    def payload_field(self, payload):
        payload_ptr = None

        if payload is not None:
            utils._check_type(payload, fields._Field)
            payload_ptr = payload._ptr

        ret = self._Domain.event_set_payload(self._ptr, payload_ptr)
        utils._handle_ret(ret, "cannot set event object's payload field")

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_props = (
            self.header_field,
            self.stream_event_context_field,
            self.context_field,
            self.payload_field,
        )
        other_props = (
            other.header_field,
            other.stream_event_context_field,
            other.context_field,
            other.payload_field,
        )
        if self_props != other_props:
            return False

        return self._eq(other)

    def _internal_copy(self, copy_func):
        cpy = self._event_class()

        # copy fields
        cpy.header_field = copy_func(self.header_field)
        cpy.stream_event_context_field = copy_func(self.stream_event_context_field)
        cpy.context_field = copy_func(self.context_field)
        cpy.payload_field = copy_func(self.payload_field)

        cpy._copy(self)

        return cpy

    def __copy__(self):
        return self._internal_copy(copy.copy)

    def __deepcopy__(self, memo):
        cpy = self._internal_copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
