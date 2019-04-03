# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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

import collections.abc
import copy
from bt2 import native_bt, utils
import bt2

class _EventClassIterator(collections.abc.Iterator):
    def __init__(self, stream_class):
        self._stream_class = stream_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._stream_class):
            raise StopIteration

        ec_ptr = native_bt.stream_class_borrow_event_class_by_index(self._stream_class._ptr,
                                                                                  self._at)
        assert(ec_ptr)
        ec_id = native_bt.event_class_get_id(ec_ptr)
        utils._handle_ret(ec_id, "cannot get event class object's ID")
        self._at += 1
        return ec_id

class _StreamClass(bt2.object._SharedObject, collections.abc.Mapping):
    _GET_REF_FUNC = native_bt.stream_class_get_ref
    _PUT_REF_FUNC = native_bt.stream_class_put_ref

    def __getitem__(self, key):
        utils._check_int64(key)

        for idx, event_class_id in enumerate(self):
            if event_class_id == key:
                ec_ptr = native_bt.stream_class_borrow_event_class_by_index(self._ptr, idx)
                native_bt.get(ec_ptr)
                return bt2.event_class._EventClass._create_from_ptr(ec_ptr)

        raise KeyError(key)

    def __len__(self):
        count = native_bt.stream_class_get_event_class_count(self._ptr)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _EventClassIterator(self)

    def create_event_class(self, id=None):
        if self.assigns_automatic_event_class_id:
            ec_ptr = native_bt.event_class_create(self._ptr)
        else:
            utils._check_uint64(id)
            ec_ptr = native_bt.event_class_create_with_id(self._ptr, id)
                
        return bt2.event_class._EventClass._create_from_ptr(ec_ptr)

    @property
    def trace(self):
        tc_ptr = native_bt.stream_class_borrow_trace(self._ptr)
        native_bt.get(tc_ptr)

        if tc_ptr is not None:
            return bt2.Trace._create_from_ptr(tc_ptr)

    @property
    def name(self):
        return native_bt.stream_class_get_name(self._ptr)

    @name.setter
    def name(self, name):
        utils._check_str(name)
        ret = native_bt.stream_class_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set stream class object's name")

    @property
    def assigns_automatic_event_class_id(self):
        return native_bt.stream_class_assigns_automatic_event_class_id(self._ptr)

    def _assigns_automatic_event_class_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.stream_class_set_assigns_automatic_event_class_id(self._ptr, auto_id)

    _assigns_automatic_event_class_id = property(fset=_assigns_automatic_event_class_id)

    @property
    def assigns_automatic_stream_id(self):
        return native_bt.stream_class_assigns_automatic_stream_id(self._ptr)

    def _assigns_automatic_stream_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.stream_class_set_assigns_automatic_stream_id(self._ptr, auto_id)

    _assigns_automatic_stream_id = property(fset=_assigns_automatic_stream_id)

    @property
    def id(self):
        return native_bt.stream_class_get_id(self._ptr)

    @property
    def event_header_field_class(self):
        ft_ptr = native_bt.stream_class_borrow_event_header_field_class(self._ptr)

        if ft_ptr is None:
            return

        native_bt.get(ft_ptr)

        return bt2.field_class._create_field_class_from_ptr(ft_ptr)

    @event_header_field_class.setter
    def event_header_field_class(self, event_header_field_class):
        event_header_field_class_ptr = None

        if event_header_field_class is not None:
            utils._check_type(event_header_field_class, bt2.field_class._FieldClass)
            event_header_field_class_ptr = event_header_field_class._ptr

            ret = native_bt.stream_class_set_event_header_field_class(self._ptr,
                                                               event_header_field_class_ptr)
            utils._handle_ret(ret, "cannot set stream class object's event header field type")

    @property
    def packet_context_field_class(self):
        ft_ptr = native_bt.stream_class_borrow_packet_context_field_class_const(self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_class._create_field_class_from_ptr_and_get_ref(ft_ptr)

    def _packet_context_field_class(self, packet_context_field_class):
        if packet_context_field_class is None:
            return

        utils._check_type(packet_context_field_class,
                          bt2.field_class._FieldClass)
        packet_context_field_class_ptr = packet_context_field_class._ptr

        ret = native_bt.stream_class_set_packet_context_field_class(self._ptr,
                                                                    packet_context_field_class_ptr)
        utils._handle_ret(
            ret, "cannot set stream class object's packet context field type")

    _packet_context_field_class = property(fset=_packet_context_field_class)

    @property
    def event_common_context_field_class(self):
        ft_ptr = native_bt.stream_class_borrow_event_common_context_field_class_const(
            self._ptr)

        if ft_ptr is None:
            return

        return bt2.field_class._create_field_class_from_ptr_and_get_ref(ft_ptr)

    def _event_common_context_field_class(self, event_common_context_field_class):
        event_common_context_field_class_ptr = None

        if event_common_context_field_class is not None:
            utils._check_type(event_common_context_field_class,
                              bt2.field_class._FieldClass)
            event_common_context_field_class_ptr = event_common_context_field_class._ptr

            ret = native_bt.stream_class_set_event_common_context_field_class(self._ptr,
                                                                              event_common_context_field_class_ptr)
            utils._handle_ret(
                ret, "cannot set stream class object's event context field type")

    _event_common_context_field_class = property(fset=_event_common_context_field_class)

    def __call__(self, id=None):
        if self.assigns_automatic_stream_id:
            stream_ptr = native_bt.stream_create(self._ptr)
        else:
            utils._check_uint64(id)
            stream_ptr = native_bt.stream_create_with_id(self._ptr, id)

        if stream_ptr is None:
            raise bt2.CreationError('cannot create stream object')

        return bt2.stream._Stream._create_from_ptr(stream_ptr)

    @property
    def default_clock_class(self):
        cc_ptr = native_bt.stream_class_borrow_default_clock_class(self._ptr)
        if cc_ptr is None:
            return

        return bt2.clock_class.ClockClass._create_from_ptr_and_get_ref(cc_ptr)

    def _default_clock_class(self, clock_class):
        utils._check_type(clock_class, bt2.clock_class.ClockClass)
        native_bt.stream_class_set_default_clock_class(
            self._ptr, clock_class._ptr)

    _default_clock_class = property(fset=_default_clock_class)

    @property
    def default_clock_always_known(self):
        return native_bt.stream_class_default_clock_is_always_known(self._ptr)

    @property
    def packets_have_discarded_event_counter_snapshot(self):
        return native_bt.stream_class_packets_have_discarded_event_counter_snapshot(self._ptr)

    @packets_have_discarded_event_counter_snapshot.setter
    def packets_have_discarded_event_counter_snapshot(self, have_discarded_event_counter):
        utils._check_bool(have_discarded_event_counter)
        native_bt.stream_class_set_packets_have_discarded_event_counter_snapshot(self._ptr, have_discarded_event_counter)

    @property
    def packets_have_packet_counter_snapshot(self):
        return native_bt.stream_class_packets_have_packet_counter_snapshot(self._ptr)

    @packets_have_packet_counter_snapshot.setter
    def packets_have_packet_counter_snapshot(self, have_packet_counter):
        utils._check_bool(have_packet_counter)
        native_bt.stream_class_set_packets_have_packet_counter_snapshot(self._ptr, have_packet_counter)

    @property
    def packets_have_default_beginning_clock_snapshot(self):
        return native_bt.stream_class_packets_have_default_beginning_clock_snapshot(self._ptr)

    @packets_have_default_beginning_clock_snapshot.setter
    def packets_have_default_beginning_clock_snapshot(self, have_default_beginning_clock_snapshot):
        utils._check_bool(have_default_beginning_clock_snapshot)
        native_bt.stream_class_set_packets_have_default_beginning_clock_snapshot(self._ptr, have_default_beginning_clock_snapshot)

    @property
    def packets_have_default_end_clock_snapshot(self):
        return native_bt.stream_class_packets_have_default_end_clock_snapshot(self._ptr)

    @packets_have_default_end_clock_snapshot.setter
    def packets_have_default_end_clock_snapshot(self, have_default_end_clock_snapshot):
        utils._check_bool(have_default_end_clock_snapshot)
        native_bt.stream_class_set_packets_have_default_end_clock_snapshot(self._ptr, have_default_end_clock_snapshot)
