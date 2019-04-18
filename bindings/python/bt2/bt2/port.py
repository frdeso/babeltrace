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

from bt2 import native_bt
import bt2.component
import bt2.connection
import bt2.message_iterator
import bt2.message
import bt2


def _create_from_ptr_and_get_ref(ptr, port_type):
    if port_type == native_bt.PORT_TYPE_INPUT:
        cls = _InputPort
    elif port_type == native_bt.PORT_TYPE_OUTPUT:
        cls = _OutputPort
    else:
        raise bt2.Error('unknown port type: {}'.format(port_type))

    return cls._create_from_ptr_and_get_ref(ptr)


def _create_self_from_ptr_and_get_ref(ptr, port_type):
    if port_type == native_bt.PORT_TYPE_INPUT:
        cls = _SelfInputPort
    elif port_type == native_bt.PORT_TYPE_OUTPUT:
        cls = _SelfOutputPort
    else:
        raise bt2.Error('unknown port type: {}'.format(port_type))

    return cls._create_from_ptr_and_get_ref(ptr)


class _Port(bt2.object._SharedObject):
    @classmethod
    def _GET_REF_FUNC(cls, ptr):
        ptr = cls._AS_PORT(ptr)
        return native_bt.port_get_ref(ptr)

    @classmethod
    def _PUT_REF_FUNC(cls, ptr):
        ptr = cls._AS_PORT(ptr)
        return native_bt.port_put_ref(ptr)

    @property
    def name(self):
        ptr = self._AS_PORT(self._ptr)
        name = native_bt.port_get_name(ptr)
        assert name is not None
        return name

    @property
    def connection(self):
        ptr = self._AS_PORT(self._ptr)
        conn_ptr = native_bt.port_borrow_connection_const(ptr)

        if conn_ptr is None:
            return

        return bt2.connection._Connection._create_from_ptr_and_get_ref(conn_ptr)

    @property
    def is_connected(self):
        ptr = self._AS_PORT(self._ptr)
        return native_bt.port_is_connected(ptr)



class _InputPort(_Port):
    _AS_PORT = native_bt.port_input_as_port_const


class _OutputPort(_Port):
    _AS_PORT = native_bt.port_output_as_port_const

    def create_message_iterator(self, colander_component_name=None):
        if colander_component_name is not None:
            utils._check_str(colander_component_name)

        notif_iter_ptr = native_bt.py3_create_output_port_notif_iter(int(self._ptr),
                                                                     colander_component_name)

        if notif_iter_ptr is None:
            raise bt2.CreationError('cannot create output port message iterator')

        return bt2.message_iterator._OutputPortMessageIterator(notif_iter_ptr)


class _SelfPort(_Port):
    @classmethod
    def _AS_PORT(cls, ptr):
        ptr = cls._AS_SELF_PORT(ptr)
        return native_bt.self_component_port_as_port(ptr)

    @property
    def connection(self):
        ptr = self._AS_PORT(self._ptr)
        conn_ptr = native_bt.port_borrow_connection_const(ptr)

        if conn_ptr is None:
            return

        return bt2.connection._Connection._create_from_ptr_and_get_ref(conn_ptr)


class _SelfInputPort(_SelfPort, _InputPort):
    _AS_SELF_PORT = native_bt.self_component_port_input_as_self_component_port

    def create_message_iterator(self):
        msg_iter_ptr = native_bt.self_component_port_input_message_iterator_create(self._ptr)
        if msg_iter_ptr is None:
            raise bt2.Error('cannot create message iterator object')

        return bt2.message_iterator._SelfPortInputMessageIterator(msg_iter_ptr)


class _SelfOutputPort(_SelfPort, _OutputPort):
    _AS_SELF_PORT = native_bt.self_component_port_output_as_self_component_port
