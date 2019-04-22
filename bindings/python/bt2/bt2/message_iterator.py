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
from bt2 import native_bt, utils
import bt2.message
import bt2.component
import bt2


class _MessageIterator(collections.abc.Iterator):
    def _handle_status(self, status, gen_error_msg):
        if status == native_bt.MESSAGE_ITERATOR_STATUS_AGAIN:
            raise bt2.TryAgain
        elif status == native_bt.MESSAGE_ITERATOR_STATUS_END:
            raise bt2.Stop
        elif status < 0:
            raise bt2.Error(gen_error_msg)

    def __next__(self):
        raise NotImplementedError


class _GenericMessageIterator(bt2.object._SharedObject, _MessageIterator):
    def __init__(self, ptr):
            self._current_notifs = []
            self._at = 0
            super().__init__(ptr)

    def __next__(self):
        if len(self._current_notifs) == self._at:
            status, notifs = self._GET_NOFICATION_RANGE(self._ptr)
            self._handle_status(status,
                                'unexpected error: cannot advance the message iterator')
            self._current_notifs = notifs
            self._at = 0

        notif_ptr = self._current_notifs[self._at]
        self._at += 1

        return bt2.message._create_from_ptr(notif_ptr)


class _SelfPortInputMessageIterator(_GenericMessageIterator):
    _GET_NOFICATION_RANGE = native_bt.py3_self_component_port_input_get_msg_range
    _GET_REF_FUNC = native_bt.self_component_port_input_message_iterator_get_ref
    _PUT_REF_FUNC = native_bt.self_component_port_input_message_iterator_put_ref

    @property
    def component(self):
        comp_ptr = native_bt.self_component_port_input_message_iterator_borrow_component(self._ptr)
        assert(comp_ptr)
        return bt2.component._create_known_component_from_ptr_and_get_ref(comp_ptr, None)


class _OutputPortMessageIterator(_GenericMessageIterator):
    _GET_NOFICATION_RANGE = native_bt.py3_port_output_get_msg_range
    _GET_REF_FUNC = native_bt.port_output_message_iterator_get_ref
    _PUT_REF_FUNC = native_bt.port_output_message_iterator_put_ref


class _UserMessageIterator(_MessageIterator):
    def __new__(cls, ptr):
        # User iterator objects are always created by the native side,
        # that is, never instantiated directly by Python code.
        #
        # The native code calls this, then manually calls
        # self.__init__() without the `ptr` argument. The user has
        # access to self.component during this call, thanks to this
        # self._ptr argument being set.
        #
        # self._ptr is NOT owned by this object here, so there's nothing
        # to do in __del__().
        self = super().__new__(cls)
        self._ptr = ptr
        return self

    def __init__(self):
        pass

    @property
    def _component(self):
        return native_bt.py3_get_user_component_from_user_msg_iter(self._ptr)

    @property
    def addr(self):
        return int(self._ptr)

    def _finalize(self):
        pass

    def __next__(self):
        raise bt2.Stop

    def _next_from_native(self):
        # this can raise anything: it's catched by the native part
        try:
            notif = next(self)
        except StopIteration:
            raise bt2.Stop
        except:
            raise

        utils._check_type(notif, bt2.message._Message)

        # take a new reference for the native part
        notif._get()
        return int(notif._ptr)

    def _create_event_message(self, event_class, packet, default_clock_snapshot=None):
        return bt2.message._EventMessage(self, event_class, packet, default_clock_snapshot)

    def _create_inactivity_message(self, clock_class, clock_snapshot):
        return bt2.message._InactivityMessage(self, clock_class, clock_snapshot)

    def _create_stream_beginning_message(self, stream):
        return bt2.message._StreamBeginningMessage(self, stream)

    def _create_stream_activity_beginning_message(self, stream, default_clock_snapshot=None):
        return bt2.message._StreamActivityBeginningMessage(self, stream, default_clock_snapshot)

    def _create_stream_activity_end_message(self, stream, default_clock_snapshot=None):
        return bt2.message._StreamActivityEndMessage(self, stream, default_clock_snapshot)

    def _create_stream_end_message(self, stream):
        return bt2.message._StreamEndMessage(self, stream)

    def _create_packet_beginning_message(self, packet, default_clock_snapshot=None):
        return bt2.message._PacketBeginningMessage(self, packet, default_clock_snapshot)

    def _create_packet_end_message(self, packet, default_clock_snapshot=None):
        return bt2.message._PacketEndMessage(self, packet, default_clock_snapshot)
