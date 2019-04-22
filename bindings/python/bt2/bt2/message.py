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


class _PacketBeginningMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_BEGINNING

    def __init__(self, priv_conn_priv_iter, packet, default_clock_snapshot):
        utils._check_type(packet, bt2.packet._Packet)
        has_default_clock_class = packet.stream.stream_class.default_clock_class is not None

        if has_default_clock_class and default_clock_snapshot is None:
            raise bt2.Error(
                '_PacketBeginningMessage: stream class has a default clock class, default_clock_snapshot should not be None')

        if not has_default_clock_class and default_clock_snapshot is not None:
            raise bt2.Error(
                '_PacketBeginningMessage: stream class has no default clock class, default_clock_snapshot should be None')

        if has_default_clock_class:
            utils._check_uint64(default_clock_snapshot)
            ptr = native_bt.message_packet_beginning_create_with_default_clock_snapshot(
                priv_conn_priv_iter._ptr, packet._ptr, default_clock_snapshot)
        else:
            ptr = native_bt.message_packet_beginning_create(
                priv_conn_priv_iter._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet beginning message object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.message_packet_beginning_borrow_packet(self._ptr)
        assert packet_ptr
        return bt2.packet._Packet._create_from_ptr_and_get_ref(packet_ptr)


class _PacketEndMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_END

    def __init__(self, priv_conn_priv_iter, packet, default_clock_snapshot):
        utils._check_type(packet, bt2.packet._Packet)
        has_default_clock_class = packet.stream.stream_class.default_clock_class is not None

        if has_default_clock_class and default_clock_snapshot is None:
            raise bt2.Error(
                '_PacketEndMessage: stream class has a default clock class, default_clock_snapshot should not be None')

        if not has_default_clock_class and default_clock_snapshot is not None:
            raise bt2.Error(
                '_PacketEndMessage: stream class has no default clock class, default_clock_snapshot should be None')

        if has_default_clock_class:
            utils._check_uint64(default_clock_snapshot)
            ptr = native_bt.message_packet_end_create_with_default_clock_snapshot(
                priv_conn_priv_iter._ptr, packet._ptr, default_clock_snapshot)
        else:
            ptr = native_bt.message_packet_end_create(
                priv_conn_priv_iter._ptr, packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet end message object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.message_packet_end_borrow_packet(self._ptr)
        assert packet_ptr
        return bt2.packet._Packet._create_from_ptr_and_get_ref(packet_ptr)


class _StreamBeginningMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_BEGINNING

    def __init__(self, priv_conn_priv_iter, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_beginning_create(
            priv_conn_priv_iter._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create stream beginning message object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_beginning_borrow_stream(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def default_clock_snapshot(self):
        value_ptr = native_bt.message_stream_begin_borrow_default_clock_snapshot(self._ptr)

        if value_ptr is None:
            return

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(value_ptr, self._ptr)

    @default_clock_snapshot.setter
    def default_clock_snapshot(self, value):
        ret = native_bt.message_stream_begin_set_default_clock_snapshot(self._ptr, value)
        utils._handle_ret(ret, "cannot set stream begin message clock value")


class _StreamActivityBeginningMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING

    def __init__(self, self_msg_iter, stream, default_clock_snapshot):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_activity_beginning_create(
            self_msg_iter._ptr, stream._ptr)

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

        status, snapshot_ptr = native_bt.message_stream_activity_beginning_borrow_default_clock_snapshot_const(
            self._ptr)

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
        native_bt.message_stream_activity_beginning_set_default_clock_snapshot(
            self._ptr, value)

    _default_clock_snapshot = property(fset=_default_clock_snapshot)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_activity_beginning_borrow_stream(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)


class _StreamActivityEndMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_END

    def __init__(self, self_msg_iter, stream, default_clock_snapshot):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_activity_end_create(
            self_msg_iter._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError(
                'cannot create stream activity end message object')

        super().__init__(ptr)

        if default_clock_snapshot is not None:
            self._default_clock_snapshot = default_clock_snapshot

    @property
    def default_clock_snapshot(self):
        if self.stream.stream_class.default_clock_class is None:
            return None

        status, snapshot_ptr = native_bt.message_stream_activity_end_borrow_default_clock_snapshot_const(
            self._ptr)

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
        native_bt.message_stream_activity_end_set_default_clock_snapshot(
            self._ptr, value)

    _default_clock_snapshot = property(fset=_default_clock_snapshot)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_activity_end_borrow_stream(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)


class _StreamEndMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_END

    def __init__(self, priv_conn_priv_iter, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_end_create(priv_conn_priv_iter._ptr, stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create stream end message object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_end_borrow_stream(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def default_clock_snapshot(self):
        value_ptr = native_bt.message_stream_end_borrow_default_clock_snapshot(self._ptr)

        if value_ptr is None:
            return

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr(value_ptr, self._ptr)

    @default_clock_snapshot.setter
    def default_clock_snapshot(self, value):
        ret = native_bt.message_stream_end_set_default_clock_snapshot(self._ptr, value)
        utils._handle_ret(ret, "cannot set stream end message clock value")


class _InactivityMessage(_Message):
    _TYPE = native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY

    def __init__(self, priv_conn_priv_iter, clock_class, clock_snapshot):
        ptr = native_bt.message_message_iterator_inactivity_create(
            priv_conn_priv_iter._ptr, clock_class._ptr, clock_snapshot)

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

    @default_clock_snapshot.setter
    def default_clock_snapshot(self, value):
        ret = native_bt.message_inactivity_set_default_clock_snapshot(self._ptr, value)
        utils._handle_ret(ret, "cannot set stream end message clock value")

class _DiscardedEventsMessage(_Message):
    pass


class _DiscardedPacketsMessage(_Message):
    pass


_MESSAGE_TYPE_TO_CLS = {
    native_bt.MESSAGE_TYPE_EVENT: _EventMessage,
    native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY: _InactivityMessage,
    native_bt.MESSAGE_TYPE_STREAM_BEGINNING: _StreamBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_END: _StreamEndMessage,
    native_bt.MESSAGE_TYPE_PACKET_BEGINNING: _PacketBeginningMessage,
    native_bt.MESSAGE_TYPE_PACKET_END: _PacketEndMessage,
    native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING: _StreamActivityBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_END: _StreamActivityEndMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_EVENTS: _DiscardedEventsMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_PACKETS: _DiscardedPacketsMessage,
}
