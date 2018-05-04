# The MIT License (MIT)
#
# Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>
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

from . import domain
from bt2 import native_bt, utils
from bt2 import internal
import bt2.clock_class
import bt2.packet
import bt2.stream
import bt2.fields
import bt2.clock_value
import collections
import numbers
import copy
import bt2

class _EventClockValuesIterator(collections.abc.Iterator):
    def __init__(self, event_clock_values):
        self._event_clock_values = event_clock_values
        self._clock_classes = event_clock_values._event._clock_classes
        self._at = 0

    def __next__(self):
        if self._at == len(self._clock_classes):
            raise StopIteration

        self._at += 1
        return self._clock_classes[at]


class _EventClockValues(collections.abc.Mapping):
    def __init__(self, event):
        self._event = event

    def __getitem__(self, clock_class):
        utils._check_type(clock_class, bt2.ClockClass)
        clock_value_ptr = native_bt.event_get_clock_value(self._event._ptr,
                                                          clock_class._ptr)

        if clock_value_ptr is None:
            return

        clock_value = bt2.clock_value._create_clock_value_from_ptr(clock_value_ptr)
        return clock_value

    def add(self, clock_value):
        utils._check_type(clock_value, bt2.clock_value._ClockValue)
        ret = native_bt.event_set_clock_value(self._event._ptr,
                                              clock_value._ptr)
        utils._handle_ret(ret, "cannot set event object's clock value")

    def __len__(self):
        count = len(self._event._clock_classes)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _EventClockValuesIterator(self)


class _Event(internal._Event, domain._DomainProvider):
    @property
    def packet(self):
        packet_ptr = native_bt.event_get_packet(self._ptr)

        if packet_ptr is None:
            return packet_ptr

        return bt2.packet._Packet._create_from_ptr(packet_ptr)

    @packet.setter
    def packet(self, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ret = native_bt.event_set_packet(self._ptr, packet._ptr)
        utils._handle_ret(ret, "cannot set event object's packet object")

    def __getitem__(self, key):
        utils._check_str(key)
        payload_field = self.payload_field

        if payload_field is not None and key in payload_field:
            return payload_field[key]

        context_field = self.context_field

        if context_field is not None and key in context_field:
            return context_field[key]

        sec_field = self.stream_event_context_field

        if sec_field is not None and key in sec_field:
            return sec_field[key]

        header_field = self.header_field

        if header_field is not None and key in header_field:
            return header_field[key]

        packet = self.packet

        if packet is None:
            raise KeyError(key)

        pkt_context_field = packet.context_field

        if pkt_context_field is not None and key in pkt_context_field:
            return pkt_context_field[key]

        pkt_header_field = packet.header_field

        if pkt_header_field is not None and key in pkt_header_field:
            return pkt_header_field[key]

        raise KeyError(key)

    @property
    def clock_values(self):
        return _EventClockValues(self)


domain._Domain.Event = _Event
