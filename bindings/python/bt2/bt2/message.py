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

import copy
import collections
from bt2 import native_bt, utils
from bt2 import object
import bt2.clock_snapshot
import bt2.event
import bt2.packet
import bt2.stream
import bt2


def _create_from_ptr(ptr):
    msg_type = native_bt.message_get_type(ptr)

    if msg_type not in _MESSAGE_TYPE_TO_CLS:
        raise bt2.Error('unknown message type: {}'.format(msg_type))

    return _MESSAGE_TYPE_TO_CLS[msg_type]._create_from_ptr(ptr)


class _Message(object._SharedObject):
    _GET_REF_FUNC = native_bt.message_get_ref
    _PUT_REF_FUNC = native_bt.message_put_ref


class _EventMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_EVENT

    def __init__(self, self_msg_iter, event_class, packet, default_clock_snapshot):
        utils._check_type(event_class, bt2.event_class._EventClass)

        has_default_clock_class = packet.stream.stream_class.default_clock_class is not None
        if has_default_clock_class and default_clock_snapshot is None:
            raise bt2.Error(
                '_EventMessage: stream class has a default clock class, default_clock_snapshot should not be None')

        if not has_default_clock_class and default_clock_snapshot is not None:
            raise bt2.Error(
                '_EventMessage: stream class has no default clock class, default_clock_snapshot should be None')

        if has_default_clock_class:
            utils._check_uint64(default_clock_snapshot)
            ptr = native_bt.message_event_create_with_default_clock_snapshot(self_msg_iter._ptr,
                                                                             event_class._ptr, packet._ptr, default_clock_snapshot)
        else:
            ptr = native_bt.message_event_create(self_msg_iter._ptr,
                                                 event_class._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create event message object')

        super().__init__(ptr)

    @property
    def event(self):
        event_ptr = native_bt.message_event_borrow_event(self._ptr)
        assert(event_ptr)
        return bt2.event._create_from_ptr(event_ptr, self._ptr,
                                          self._GET_REF_FUNC,
                                          self._PUT_REF_FUNC)

    @property
    def default_clock_snapshot(self):
        if self.event.event_class.stream_class.default_clock_class is None:
            return None

        status, snapshot_ptr = native_bt.message_event_borrow_default_clock_snapshot_const(self._ptr)

        if status == native_bt.CLOCK_SNAPSHOT_STATE_UNKNOWN:
            return

        if snapshot_ptr is None:
            return

        assert snapshot_ptr is not None

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(snapshot_ptr, self._ptr,
                                                                  self._GET_REF_FUNC,
                                                                  self._PUT_REF_FUNC)


class _PacketMessage(_Message):
    def __init__(self, self_msg_iter, packet, default_clock_snapshot):
        utils._check_type(packet, bt2.packet._Packet)
        has_default_clock_class = packet.stream.stream_class.default_clock_class is not None

        if has_default_clock_class and default_clock_snapshot is None:
            raise bt2.Error(
                '{}: stream class has a default clock class, default_clock_snapshot should not be None'.format(self._NAME))

        if not has_default_clock_class and default_clock_snapshot is not None:
            raise bt2.Error(
                '{}: stream class has no default clock class, default_clock_snapshot should be None'.format(self._NAME))

        if has_default_clock_class:
            utils._check_uint64(default_clock_snapshot)
            ptr = self._CREATE_WITH_SNAPSHOT(self_msg_iter._ptr, packet._ptr, default_clock_snapshot)
        else:
            ptr = self._CREATE(self_msg_iter._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet beginning message object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = self._BORROW_PACKET(self._ptr)
        assert packet_ptr
        return bt2.packet._Packet._create_from_ptr_and_get_ref(packet_ptr)

    @property
    def default_clock_snapshot(self):
        if self.packet.stream.stream_class.default_clock_class is None:
            return None

        status, snapshot_ptr = self._BORROW_DEFAULT_CLOCK_SNAPSHOT(self._ptr)

        if status == native_bt.MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN:
            return

        assert snapshot_ptr is not None

        # TODO: Is it really possible that the above returns KNOWN, but a None snapshot_ptr?s
        if snapshot_ptr is None:
            return

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(snapshot_ptr, self._ptr,
                                                                  self._GET_REF_FUNC,
                                                                  self._PUT_REF_FUNC)


class _PacketBeginningMessage(_PacketMessage):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_BEGINNING
    _NAME = "_PacketBeginningMessage"
    _CREATE = native_bt.message_packet_beginning_create
    _CREATE_WITH_SNAPSHOT = native_bt.message_packet_beginning_create_with_default_clock_snapshot
    _BORROW_PACKET= native_bt.message_packet_beginning_borrow_packet
    _BORROW_DEFAULT_CLOCK_SNAPSHOT = native_bt.message_packet_beginning_borrow_default_clock_snapshot_const


class _PacketEndMessage(_PacketMessage):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_END
    _NAME = "_PacketEndMessage"
    _CREATE = native_bt.message_packet_end_create
    _CREATE_WITH_SNAPSHOT = native_bt.message_packet_end_create_with_default_clock_snapshot
    _BORROW_PACKET= native_bt.message_packet_end_borrow_packet
    _BORROW_DEFAULT_CLOCK_SNAPSHOT = native_bt.message_packet_end_borrow_default_clock_snapshot_const


class _StreamMessage(_Message):
    def __init__(self, self_msg_iter, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = self._CREATE(self_msg_iter._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create stream beginning message object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = self._BORROW_STREAM(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)


class _StreamBeginningMessage(_StreamMessage):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_BEGINNING
    _CREATE = native_bt.message_stream_beginning_create
    _BORROW_STREAM = native_bt.message_stream_beginning_borrow_stream


class _StreamEndMessage(_StreamMessage):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_END
    _CREATE = native_bt.message_stream_end_create
    _BORROW_STREAM = native_bt.message_stream_end_borrow_stream


class _StreamActivityMessage(_Message):
    def __init__(self, self_msg_iter, stream, default_clock_snapshot):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = self._CREATE(self_msg_iter._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError(
                'cannot create stream activity beginning message object')

        super().__init__(ptr)

        if default_clock_snapshot is not None:
            self._default_clock_snapshot = default_clock_snapshot

    @property
    def default_clock_snapshot(self):
        if self.stream.stream_class.default_clock_class is None:
            return None

        status, snapshot_ptr = self._BORROW_DEFAULT_CLOCK_SNAPSHOT(self._ptr)

        if status == native_bt.MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN:
            return

        assert snapshot_ptr is not None

        # TODO: Is it really possible that the above returns KNOWN, but a None snapshot_ptr?s
        if snapshot_ptr is None:
            return

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(snapshot_ptr, self._ptr,
                                                                  self._GET_REF_FUNC,
                                                                  self._PUT_REF_FUNC)

    def _default_clock_snapshot(self, value):
        self._SET_DEFAULT_CLOCK_SNAPSHOT(self._ptr, value)

    _default_clock_snapshot = property(fset=_default_clock_snapshot)

    @property
    def stream(self):
        stream_ptr = self._BORROW_STREAM(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)


class _StreamActivityBeginningMessage(_StreamActivityMessage):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING
    _CREATE = native_bt.message_stream_activity_beginning_create
    _BORROW_DEFAULT_CLOCK_SNAPSHOT = native_bt.message_stream_activity_beginning_borrow_default_clock_snapshot_const
    _SET_DEFAULT_CLOCK_SNAPSHOT = native_bt.message_stream_activity_beginning_set_default_clock_snapshot
    _BORROW_STREAM = native_bt.message_stream_activity_beginning_borrow_stream


class _StreamActivityEndMessage(_StreamActivityMessage):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_END
    _CREATE = native_bt.message_stream_activity_end_create
    _BORROW_DEFAULT_CLOCK_SNAPSHOT = native_bt.message_stream_activity_end_borrow_default_clock_snapshot_const
    _SET_DEFAULT_CLOCK_SNAPSHOT = native_bt.message_stream_activity_end_set_default_clock_snapshot
    _BORROW_STREAM = native_bt.message_stream_activity_end_borrow_stream


class _MessageIteratorInactivityMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY

    def __init__(self, self_msg_iter, clock_class, clock_snapshot):
        ptr = native_bt.message_message_iterator_inactivity_create(
            self_msg_iter._ptr, clock_class._ptr, clock_snapshot)

        if ptr is None:
            raise bt2.CreationError('cannot create inactivity message object')

        super().__init__(ptr)

    @property
    def default_clock_snapshot(self):
        status, snapshot_ptr = native_bt.message_message_iterator_inactivity_borrow_default_clock_snapshot_const(self._ptr)

        assert status == native_bt.CLOCK_SNAPSHOT_STATE_KNOWN
        assert snapshot_ptr is not None

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(snapshot_ptr, self._ptr,
                                                                  self._GET_REF_FUNC,
                                                                  self._PUT_REF_FUNC)


class _DiscardedMessage(_Message):
    def __init__(self, self_msg_iter, stream, count, beg_snapshot_value=None, end_snapshot_value=None):

        utils._check_type(stream, bt2.stream._Stream)

        if beg_snapshot_value is None and end_snapshot_value is not None:
            raise bt2.CreationError('cannot create inactivity message object')

        if beg_snapshot_value is not None and end_snapshot_value is None:
            raise bt2.CreationError('cannot create inactivity message object')

        if beg_snapshot_value is None:
            ptr = self._CREATE(self_msg_iter._ptr, stream._ptr)
        else:
            utils._check_uint64(beg_snapshot_value)
            utils._check_uint64(end_snapshot_value)
            ptr = self._CREATE_WITH_SNAPSHOTS(self_msg_iter._ptr, stream._ptr,
                                                    beg_snapshot_value, end_snapshot_value)

        if ptr is None:
            raise bt2.CreationError('cannot create stream end message object')

        self._SET_COUNT(ptr, count)

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = self._BORROW_STREAM(self._ptr);
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def default_clock_class(self):
        cc_ptr = self._BORROW_CLOCK_CLASS_CONST(self._ptr);
        if cc_ptr is not None:
            return bt2.ClockClass._create_from_ptr_and_get_ref(cc_ptr)

    @property
    def count(self):
        avail, count = self._GET_COUNT(self._ptr);
        if avail is native_bt.PROPERTY_AVAILABILITY_AVAILABLE:
            return count

    @property
    def beginning_default_clock_snapshot(self):
        if self.stream.stream_class.default_clock_class is None:
            return None

        snapshot_state, snapshot_ptr = self._BORROW_BEGINNING_CLOCK_SNAPSHOT(self._ptr)
        if snapshot_state == native_bt.CLOCK_SNAPSHOT_STATE_KNOWN:
            if snapshot_ptr is None:
                return

            return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(snapshot_ptr, self._ptr,
                                                                  self._GET_REF_FUNC,
                                                                  self._PUT_REF_FUNC)

    @property
    def end_default_clock_snapshot(self):
        if self.stream.stream_class.default_clock_class is None:
            return None

        snapshot_state, snapshot_ptr = self._BORROW_END_CLOCK_SNAPSHOT(self._ptr)
        if snapshot_state == native_bt.CLOCK_SNAPSHOT_STATE_KNOWN:
            if snapshot_ptr is None:
                return None

            return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(snapshot_ptr, self._ptr,
                                                                  self._GET_REF_FUNC,
                                                                  self._PUT_REF_FUNC)


class _DiscardedEventsMessage(_DiscardedMessage):
    _TYPE = native_bt.MESSAGE_TYPE_DISCARDED_EVENTS
    _CREATE = native_bt.message_discarded_events_create
    _CREATE_WITH_SNAPSHOTS = native_bt.message_discarded_events_create_with_default_clock_snapshots
    _BORROW_STREAM = native_bt.message_discarded_events_borrow_stream_const
    _BORROW_CLOCK_CLASS_CONST = native_bt.message_discarded_events_borrow_stream_class_default_clock_class_const
    _GET_COUNT = native_bt.message_discarded_events_get_count
    _SET_COUNT = native_bt.message_discarded_events_set_count
    _BORROW_BEGINNING_CLOCK_SNAPSHOT = native_bt.message_discarded_events_borrow_default_beginning_clock_snapshot_const
    _BORROW_END_CLOCK_SNAPSHOT = native_bt.message_discarded_events_borrow_default_end_clock_snapshot_const


class _DiscardedPacketsMessage(_DiscardedMessage):
    _TYPE = native_bt.MESSAGE_TYPE_DISCARDED_PACKETS
    _CREATE = native_bt.message_discarded_packets_create
    _CREATE_WITH_SNAPSHOTS = native_bt.message_discarded_packets_create_with_default_clock_snapshots
    _BORROW_STREAM = native_bt.message_discarded_packets_borrow_stream_const
    _BORROW_CLOCK_CLASS_CONST = native_bt.message_discarded_packets_borrow_stream_class_default_clock_class_const
    _GET_COUNT = native_bt.message_discarded_packets_get_count
    _SET_COUNT = native_bt.message_discarded_packets_set_count
    _BORROW_BEGINNING_CLOCK_SNAPSHOT = native_bt.message_discarded_packets_borrow_default_beginning_clock_snapshot_const
    _BORROW_END_CLOCK_SNAPSHOT = native_bt.message_discarded_packets_borrow_default_end_clock_snapshot_const


_MESSAGE_TYPE_TO_CLS = {
    native_bt.MESSAGE_TYPE_EVENT: _EventMessage,
    native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY: _MessageIteratorInactivityMessage,
    native_bt.MESSAGE_TYPE_STREAM_BEGINNING: _StreamBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_END: _StreamEndMessage,
    native_bt.MESSAGE_TYPE_PACKET_BEGINNING: _PacketBeginningMessage,
    native_bt.MESSAGE_TYPE_PACKET_END: _PacketEndMessage,
    native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING: _StreamActivityBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_END: _StreamActivityEndMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_EVENTS: _DiscardedEventsMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_PACKETS: _DiscardedPacketsMessage,
}
