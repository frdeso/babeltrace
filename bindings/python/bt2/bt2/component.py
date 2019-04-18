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
import os
import sys
import traceback
from bt2 import native_bt, utils, object
import bt2.message_iterator
import bt2.value
import bt2.port
import bt2


_env_var = os.environ.get('BABELTRACE_PYTHON_BT2_NO_TRACEBACK')
_NO_PRINT_TRACEBACK = _env_var == '1'


# This class wraps a component class pointer. This component class could
# have been created by Python code, but since we only have the pointer,
# we can only wrap it in a generic way and lose the original Python
# class.
class _GenericComponentClass(object._SharedObject):

    @property
    def name(self):
        ptr = self._AS_COMPONENT_CLASS(self._ptr)
        name = native_bt.component_class_get_name(ptr)
        assert name is not None
        return name

    @property
    def description(self):
        ptr = self._AS_COMPONENT_CLASS(self._ptr)
        return native_bt.component_class_get_description(ptr)

    @property
    def help(self):
        ptr = self._AS_COMPONENT_CLASS(self._ptr)
        return native_bt.component_class_get_help(ptr)


class _GenericBaseComponentClass(_GenericComponentClass):
    _GET_REF_FUNC = native_bt.component_class_get_ref
    _PUT_REF_FUNC = native_bt.component_class_put_ref
    _AS_COMPONENT_CLASS = lambda self, ptr:ptr


class _GenericSourceComponentClass(_GenericComponentClass):
    _GET_REF_FUNC = native_bt.component_class_source_get_ref
    _PUT_REF_FUNC = native_bt.component_class_source_put_ref
    _AS_COMPONENT_CLASS = native_bt.component_class_source_as_component_class


class _GenericFilterComponentClass(_GenericComponentClass):
    _GET_REF_FUNC = native_bt.component_class_filter_get_ref
    _PUT_REF_FUNC = native_bt.component_class_filter_put_ref
    _AS_COMPONENT_CLASS = native_bt.component_class_filter_as_component_class


class _GenericSinkComponentClass(_GenericComponentClass):
    _GET_REF_FUNC = native_bt.component_class_sink_get_ref
    _PUT_REF_FUNC = native_bt.component_class_sink_put_ref
    _AS_COMPONENT_CLASS = native_bt.component_class_sink_as_component_class


def _handle_component_status(status, gen_error_msg):
    if status == native_bt.SELF_COMPONENT_STATUS_END:
        raise bt2.Stop
    elif status == native_bt.SELF_COMPONENT_STATUS_AGAIN:
        raise bt2.TryAgain
    elif status == native_bt.SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION:
        raise bt2.PortConnectionRefused
    elif status < 0:
        raise bt2.Error(gen_error_msg)


class _PortIterator(collections.abc.Iterator):
    def __init__(self, comp_ports):
        self._comp_ports = comp_ports
        self._at = 0

    def __next__(self):
        if self._at == len(self._comp_ports):
            raise StopIteration

        comp_ports = self._comp_ports
        comp_ptr = comp_ports._component_ptr

        port_ptr = comp_ports._borrow_port_at_index_fn(comp_ptr, self._at)
        assert port_ptr is not None

        name = native_bt.port_get_name(comp_ports._port_pycls._AS_PORT(port_ptr))
        assert name is not None

        self._at += 1
        return name


class _ComponentPorts(collections.abc.Mapping):
    def __init__(self, component_ptr,
                 borrow_port_by_name_fn,
                 borrow_port_at_index_fn,
                 get_port_count_fn,
                 port_pycls):
        '''component_ptr is a bt_component_source *, bt_component_filter * or
        bt_component_sink *.  Its type my match the type expected by the
        function pointers passed as arguments.'''

        self._component_ptr = component_ptr
        self._borrow_port_by_name_fn = borrow_port_by_name_fn
        self._borrow_port_at_index_fn = borrow_port_at_index_fn
        self._get_port_count_fn = get_port_count_fn
        self._port_pycls = port_pycls

    def __getitem__(self, key):
        utils._check_str(key)
        port_ptr = self._borrow_port_by_name_fn(self._component_ptr, key)

        if port_ptr is None:
            raise KeyError(key)

        return self._port_pycls._create_from_ptr_and_get_ref(port_ptr)

    def __len__(self):
        count = self._get_port_count_fn(self._component_ptr)
        assert count >= 0
        return count

    def __iter__(self):
        return _PortIterator(self)


# This class holds the methods which are common to both generic
# component objects and Python user component objects. They use the
# internal native _ptr, however it was set, to call native API
# functions.
class _Component:
    @property
    def name(self):
        ptr = self._AS_COMPONENT(self._ptr)
        name = native_bt.component_get_name(ptr)
        assert name is not None
        return name

    @property
    def component_class(self):
        cc_ptr = self._BORROW_COMPONENT_CLASS(self._ptr)
        assert cc_ptr is not None
        return _create_known_component_class_from_ptr_and_get_ref(cc_ptr, self._COMP_CLS_TYPE)


class _BaseComponent(_Component):
    _AS_COMPONENT = lambda self, ptr:ptr
    _AS_COMPONENT_CLASS = lambda self, ptr:ptr
    _BORROW_COMPONENT_CLASS = native_bt.component_borrow_class_const
    _COMP_CLS_TYPE = None


class _SourceComponent(_Component):
    _AS_COMPONENT = native_bt.component_source_as_component_const
    _AS_COMPONENT_CLASS = native_bt.component_class_source_as_component_class
    _BORROW_COMPONENT_CLASS = native_bt.component_source_borrow_component_class_const
    _COMP_CLS_TYPE = native_bt.COMPONENT_CLASS_TYPE_SOURCE


class _FilterComponent(_Component):
    _AS_COMPONENT = native_bt.component_filter_as_component_const
    _AS_COMPONENT_CLASS = native_bt.component_class_filter_as_component_class
    _BORROW_COMPONENT_CLASS = native_bt.component_filter_borrow_component_class_const
    _COMP_CLS_TYPE = native_bt.COMPONENT_CLASS_TYPE_FILTER


class _SinkComponent(_Component):
    _AS_COMPONENT = native_bt.component_sink_as_component_const
    _AS_COMPONENT_CLASS = native_bt.component_class_sink_as_component_class
    _BORROW_COMPONENT_CLASS = native_bt.component_sink_borrow_component_class_const
    _COMP_CLS_TYPE = native_bt.COMPONENT_CLASS_TYPE_SINK


class _GenericBaseComponent(object._SharedObject, _BaseComponent):
    _GET_REF_FUNC = native_bt.component_get_ref
    _PUT_REF_FUNC = native_bt.component_put_ref


class _GenericSourceComponent(object._SharedObject, _SourceComponent):
    _GET_REF_FUNC = native_bt.component_source_get_ref
    _PUT_REF_FUNC = native_bt.component_source_put_ref

    @property
    def output_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.component_source_borrow_output_port_by_name_const,
                               native_bt.component_source_borrow_output_port_by_index_const,
                               native_bt.component_source_get_output_port_count,
                               bt2.port._OutputPort)


# This is analogous to _GenericFilterComponentClass, but for filter
# component objects.
class _GenericFilterComponent(object._SharedObject, _FilterComponent):
    _GET_REF_FUNC = native_bt.component_filter_get_ref
    _PUT_REF_FUNC = native_bt.component_filter_put_ref

    @property
    def output_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.component_filter_borrow_output_port_by_name_const,
                               native_bt.component_filter_borrow_output_port_by_index_const,
                               native_bt.component_filter_get_output_port_count,
                               bt2.port._OutputPort)

    @property
    def input_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.component_filter_borrow_input_port_by_name_const,
                               native_bt.component_filter_borrow_input_port_by_index_const,
                               native_bt.component_filter_get_input_port_count,
                               bt2.port._InputPort)


# This is analogous to _GenericSinkComponentClass, but for sink
# component objects.
class _GenericSinkComponent(object._SharedObject, _SinkComponent):
    _GET_REF_FUNC = native_bt.component_sink_get_ref
    _PUT_REF_FUNC = native_bt.component_sink_put_ref

    @property
    def input_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.component_sink_borrow_input_port_by_name_const,
                               native_bt.component_sink_borrow_input_port_by_index_const,
                               native_bt.component_sink_get_input_port_count,
                               bt2.port._InputPort)


_COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _GenericSourceComponent,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _GenericFilterComponent,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _GenericSinkComponent,
}


_COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS = {
    native_bt.COMPONENT_CLASS_TYPE_SOURCE: _GenericSourceComponentClass,
    native_bt.COMPONENT_CLASS_TYPE_FILTER: _GenericFilterComponentClass,
    native_bt.COMPONENT_CLASS_TYPE_SINK: _GenericSinkComponentClass,
}


def _create_known_component_from_ptr(ptr, comp_cls_type):
    """Create a component Python object (of type _GenericSourceComponent,
    _GenericFilterComponent or _GenericSinkComponent, depending on
    comp_cls_type).

    The Python object steals the reference to ptr."""
    if comp_cls_type is not None:
        return _COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS[comp_cls_type]._create_from_ptr(ptr)
    else:
        return _GenericBaseComponent._create_from_ptr(ptr)


def _create_known_component_from_ptr_and_get_ref(ptr, comp_cls_type):
    if comp_cls_type is not None:
        return _COMP_CLS_TYPE_TO_GENERIC_COMP_PYCLS[comp_cls_type]._create_from_ptr_and_get_ref(ptr)
    else:
        return _GenericBaseComponent._create_from_ptr_and_get_ref(ptr)


def _create_known_component_class_from_ptr_and_get_ref(ptr, comp_cls_type):
    if comp_cls_type is not None:
        return _COMP_CLS_TYPE_TO_GENERIC_COMP_CLS_PYCLS[comp_cls_type]._create_from_ptr_and_get_ref(ptr)
    else:
        return _GenericBaseComponentClass._create_from_ptr_and_get_ref(ptr)


def _trim_docstring(docstring):
    lines = docstring.expandtabs().splitlines()
    indent = sys.maxsize

    for line in lines[1:]:
        stripped = line.lstrip()

        if stripped:
            indent = min(indent, len(line) - len(stripped))

    trimmed = [lines[0].strip()]

    if indent < sys.maxsize:
        for line in lines[1:]:
            trimmed.append(line[indent:].rstrip())

    while trimmed and not trimmed[-1]:
        trimmed.pop()

    while trimmed and not trimmed[0]:
        trimmed.pop(0)

    return '\n'.join(trimmed)


# Metaclass for component classes defined by Python code.
#
# The Python user can create a standard Python class which inherits one
# of the three base classes (_UserSourceComponent, _UserFilterComponent,
# or _UserSinkComponent). Those base classes set this class
# (_UserComponentType) as their metaclass.
#
# Once the body of a user-defined component class is executed, this
# metaclass is used to create and initialize the class. The metaclass
# creates a native BT component class of the corresponding type and
# associates it with this user-defined class. The metaclass also defines
# class methods like the `name` and `description` properties to match
# the _GenericComponentClass interface.
#
# The component class name which is used is either:
#
# * The `name` parameter of the class:
#
#       class MySink(bt2.SinkComponent, name='my-custom-sink'):
#           ...
#
# * If the `name` class parameter is not used: the name of the class
#   itself (`MySink` in the example above).
#
# The component class description which is used is the user-defined
# class's docstring:
#
#     class MySink(bt2.SinkComponent):
#         'Description goes here'
#         ...
#
# A user-defined Python component class can have an __init__() method
# which must at least accept the `params` and `name` arguments:
#
#     def __init__(self, params, name, something_else):
#         ...
#
# The user-defined component class can also have a _finalize() method
# (do NOT use __del__()) to be notified when the component object is
# finalized.
#
# User-defined source and filter component classes must use the
# `message_iterator_class` class parameter to specify the
# message iterator class to use for this component class:
#
#     class MyMessageIterator(bt2._UserMessageIterator):
#         ...
#
#     class MySource(bt2._UserSourceComponent,
#                    message_iterator_class=MyMessageIterator):
#         ...
#
# This message iterator class must inherit
# bt2._UserMessageIterator, and it must define the _get() and
# _next() methods. The message iterator class can also define an
# __init__() method: this method has access to the original Python
# component object which was used to create it as the `component`
# property. The message iterator class can also define a
# _finalize() method (again, do NOT use __del__()): this is called when
# the message iterator is (really) destroyed.
#
# When the user-defined class is destroyed, this metaclass's __del__()
# method is called: the native BT component class pointer is put (not
# needed anymore, at least not by any Python code since all references
# are dropped for __del__() to be called).
class _UserComponentType(type):
    # __new__() is used to catch custom class parameters
    def __new__(meta_cls, class_name, bases, attrs, **kwargs):
        return super().__new__(meta_cls, class_name, bases, attrs)

    def __init__(cls, class_name, bases, namespace, **kwargs):
        super().__init__(class_name, bases, namespace)

        # skip our own bases; they are never directly instantiated by the user
        own_bases = (
            '_UserComponent',
            '_UserFilterSinkComponent',
            '_UserSourceComponent',
            '_UserFilterComponent',
            '_UserSinkComponent',
        )

        if class_name in own_bases:
            return

        comp_cls_name = kwargs.get('name', class_name)
        utils._check_str(comp_cls_name)
        comp_cls_descr = None
        comp_cls_help = None

        if hasattr(cls, '__doc__') and cls.__doc__ is not None:
            utils._check_str(cls.__doc__)
            docstring = _trim_docstring(cls.__doc__)
            lines = docstring.splitlines()

            if len(lines) >= 1:
                comp_cls_descr = lines[0]

            if len(lines) >= 3:
                comp_cls_help = '\n'.join(lines[2:])

        iter_cls = kwargs.get('message_iterator_class')

        if _UserSourceComponent in bases:
            _UserComponentType._set_iterator_class(cls, iter_cls)
            cc_ptr = native_bt.py3_component_class_source_create(cls,
                                                                 comp_cls_name,
                                                                 comp_cls_descr,
                                                                 comp_cls_help)
        elif _UserFilterComponent in bases:
            _UserComponentType._set_iterator_class(cls, iter_cls)
            cc_ptr = native_bt.py3_component_class_filter_create(cls,
                                                                 comp_cls_name,
                                                                 comp_cls_descr,
                                                                 comp_cls_help)
        elif _UserSinkComponent in bases:
            if not hasattr(cls, '_consume'):
                raise bt2.IncompleteUserClass("cannot create component class '{}': missing a _consume() method".format(class_name))

            cc_ptr = native_bt.py3_component_class_sink_create(cls,
                                                               comp_cls_name,
                                                               comp_cls_descr,
                                                               comp_cls_help)
        else:
            raise bt2.IncompleteUserClass("cannot find a known component class base in the bases of '{}'".format(class_name))

        if cc_ptr is None:
            raise bt2.CreationError("cannot create component class '{}'".format(class_name))

        cls._cc_ptr = cc_ptr

    def _init_from_native(cls, comp_ptr, params_ptr):
        # create instance, not user-initialized yet
        self = cls.__new__(cls)

        # pointer to native private component object (weak/borrowed)
        self._ptr = comp_ptr

        # call user's __init__() method
        if params_ptr is not None:
            params = bt2.value._create_from_ptr_and_get_ref(params_ptr)
        else:
            params = None

        self.__init__(params)
        return self

    def __call__(cls, *args, **kwargs):
        raise bt2.Error('cannot directly instantiate a user component from a Python module')

    @staticmethod
    def _set_iterator_class(cls, iter_cls):
        if iter_cls is None:
            raise bt2.IncompleteUserClass("cannot create component class '{}': missing message iterator class".format(cls.__name__))

        if not issubclass(iter_cls, bt2.message_iterator._UserMessageIterator):
            raise bt2.IncompleteUserClass("cannot create component class '{}': message iterator class does not inherit bt2._UserMessageIterator".format(cls.__name__))

        if not hasattr(iter_cls, '__next__'):
            raise bt2.IncompleteUserClass("cannot create component class '{}': message iterator class is missing a __next__() method".format(cls.__name__))

        cls._iter_cls = iter_cls

    @property
    def name(cls):
        ptr = cls._AS_COMPONENT_CLASS(cls._cc_ptr)
        return native_bt.component_class_get_name(ptr)

    @property
    def description(cls):
        ptr = cls._AS_COMPONENT_CLASS(cls._cc_ptr)
        return native_bt.component_class_get_description(ptr)

    @property
    def help(cls):
        ptr = cls._AS_COMPONENT_CLASS(cls._cc_ptr)
        return native_bt.component_class_get_help(ptr)

    @property
    def addr(cls):
        return int(cls._cc_ptr)

    def _query_from_native(cls, query_exec_ptr, obj, params_ptr):
        # this can raise, in which case the native call to
        # bt_component_class_query() returns NULL
        if params_ptr is not None:
            params = bt2.value._create_from_ptr_and_get_ref(params_ptr)
        else:
            params = None

        query_exec = bt2.QueryExecutor._create_from_ptr_and_get_ref(
            query_exec_ptr)

        # this can raise, but the native side checks the exception
        results = cls._query(query_exec, obj, params)

        if results is NotImplemented:
            return results

        # this can raise, but the native side checks the exception
        results = bt2.create_value(results)

        if results is None:
            results_addr = int(native_bt.value_null)
        else:
            # return new reference
            results._get()
            results_addr = int(results._ptr)

        return results_addr

    def _query(cls, query_executor, obj, params):
        # BT catches this and returns NULL to the user
        return NotImplemented

    def __eq__(self, other):
        if not hasattr(other, 'addr'):
            return False

        return self.addr == other.addr

    def __del__(cls):
        if hasattr(cls, '_cc_ptr'):
            cc_ptr = cls._AS_COMPONENT_CLASS(cls._cc_ptr)
            native_bt.component_class_put_ref(cc_ptr)


class _UserComponent(metaclass=_UserComponentType):
    @property
    def name(self):
        ptr = self._AS_NOT_SELF_SPECIFIC_COMPONENT(self._ptr)
        ptr = self._AS_COMPONENT(ptr)
        name = native_bt.component_get_name(ptr)
        assert name is not None
        return name

    @property
    def component_class(self):
        comp_ptr = self._AS_NOT_SELF_SPECIFIC_COMPONENT(self._ptr)
        cc_ptr = self._BORROW_COMPONENT_CLASS(comp_ptr)
        return _create_known_component_class_from_ptr_and_get_ref(cc_ptr, self._COMP_CLS_TYPE)

    @property
    def addr(self):
        return int(self._ptr)

    def __init__(self, params=None):
        pass

    def _finalize(self):
        pass

    def _accept_port_connection(self, port, other_port):
        return True

    def _accept_port_connection_from_native(self, self_port_ptr, self_port_type, other_port_ptr):
        port = bt2.port._create_self_from_ptr_and_get_ref(
            self_port_ptr, self_port_type)
        other_port_type = native_bt.PORT_TYPE_INPUT if self_port_type == native_bt.PORT_TYPE_OUTPUT else native_bt.PORT_TYPE_OUTPUT
        other_port = bt2.port._create_from_ptr_and_get_ref(
            other_port_ptr, other_port_type)
        res = self._accept_port_connection(port, other_port_ptr)

        if type(res) is not bool:
            raise TypeError("'{}' is not a 'bool' object")

        return res

    def _port_connected(self, port, other_port):
        pass

    def _port_connected_from_native(self, self_port_ptr, self_port_type, other_port_ptr):
        port = bt2.port._create_self_from_ptr_and_get_ref(
            self_port_ptr, self_port_type)
        other_port_type = native_bt.PORT_TYPE_INPUT if self_port_type == native_bt.PORT_TYPE_OUTPUT else native_bt.PORT_TYPE_OUTPUT
        other_port = bt2.port._create_from_ptr_and_get_ref(
            other_port_ptr, other_port_type)
        self._port_connected(port, other_port)

    def _port_disconnected(self, port):
        pass

    def _port_disconnected_from_native(self, port_ptr):
        native_bt.get(port_ptr)
        port = bt2.port._create_private_from_ptr(port_ptr)

        try:
            self._port_disconnected(port)
        except:
            if not _NO_PRINT_TRACEBACK:
                traceback.print_exc()

    def _graph_is_configured_from_native(self):
        self._graph_is_configured()

    def _create_trace_class(self, env=None, uuid=None,
                            assigns_automatic_stream_class_id=None):
        ptr = self._AS_SELF_COMPONENT(self._ptr)
        tc_ptr = native_bt.trace_class_create(ptr)

        if tc_ptr is None:
            raise bt2.CreationError('could not create trace class')

        tc = bt2.TraceClass._create_from_ptr(tc_ptr)

        if env is not None:
            for key, value in env.items():
                tc.env[key] = value

        if uuid is not None:
            tc._uuid = uuid

        if assigns_automatic_stream_class_id is not None:
            tc._assigns_automatic_stream_class_id = assigns_automatic_stream_class_id

        return tc

    def _create_clock_class(self, name=None, frequency=None, description=None,
                            precision=None, offset=None, origin_is_unix_epoch=None,
                            uuid=None):
        ptr = self._AS_SELF_COMPONENT(self._ptr)
        cc_ptr = native_bt.clock_class_create(ptr)

        if cc_ptr is None:
            raise bt2.CreationError('could not create clock class')

        cc = bt2.ClockClass._create_from_ptr(cc_ptr)

        if name is not None:
            cc._name = name

        if frequency is not None:
            cc._frequency = frequency

        if description is not None:
            cc._description = description

        if precision is not None:
            cc._precision = precision

        if offset is not None:
            cc._offset = offset

        if origin_is_unix_epoch is not None:
            cc._origin_is_unix_epoch = origin_is_unix_epoch

        if uuid is not None:
            cc._uuid = uuid

        return cc


class _UserSourceComponent(_UserComponent, _SourceComponent):
    _AS_NOT_SELF_SPECIFIC_COMPONENT = native_bt.self_component_source_as_component_source
    _AS_SELF_COMPONENT = native_bt.self_component_source_as_self_component

    @property
    def _output_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.self_component_source_borrow_output_port_by_name,
                               native_bt.self_component_source_borrow_output_port_by_index,
                               lambda self_ptr: native_bt.component_source_get_output_port_count(self._AS_NOT_SELF_SPECIFIC_COMPONENT(self_ptr)),
                               bt2.port._SelfOutputPort)

    def _add_output_port(self, name):
        utils._check_str(name)
        fn = native_bt.self_component_source_add_output_port
        comp_status, self_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add output port to source component object')
        assert self_port_ptr is not None
        return bt2.port._SelfOutputPort._create_from_ptr(self_port_ptr)


class _UserFilterComponent(_UserComponent, _FilterComponent):
    _AS_NOT_SELF_SPECIFIC_COMPONENT = native_bt.self_component_filter_as_component_filter
    _AS_SELF_COMPONENT = native_bt.self_component_filter_as_self_component

    @property
    def _output_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.self_component_filter_borrow_output_port_by_name,
                               native_bt.self_component_filter_borrow_output_port_by_index,
                               lambda self_ptr: native_bt.component_filter_get_output_port_count(self._AS_NOT_SELF_SPECIFIC_COMPONENT(self_ptr)),
                               bt2.port._SelfOutputPort)

    @property
    def _input_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.self_component_filter_borrow_input_port_by_name,
                               native_bt.self_component_filter_borrow_input_port_by_index,
                               lambda self_ptr: native_bt.component_filter_get_input_port_count(self._AS_NOT_SELF_SPECIFIC_COMPONENT(self_ptr)),
                               bt2.port._SelfInputPort)

    def _add_output_port(self, name):
        utils._check_str(name)
        fn = native_bt.self_component_filter_add_output_port
        comp_status, priv_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add output port to filter component object')
        assert priv_port_ptr is not None
        return bt2.port._SelfOutputPort._create_from_ptr(priv_port_ptr)

    def _add_input_port(self, name):
        utils._check_str(name)
        fn = native_bt.self_component_filter_add_input_port
        comp_status, priv_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add input port to filter component object')
        assert priv_port_ptr is not None
        return bt2.port._SelfInputPort._create_from_ptr(priv_port_ptr)


class _UserSinkComponent(_UserComponent, _SinkComponent):
    _AS_NOT_SELF_SPECIFIC_COMPONENT = native_bt.self_component_sink_as_component_sink
    _AS_SELF_COMPONENT = native_bt.self_component_sink_as_self_component

    @property
    def _input_ports(self):
        return _ComponentPorts(self._ptr,
                               native_bt.self_component_sink_borrow_input_port_by_name,
                               native_bt.self_component_sink_borrow_input_port_by_index,
                               lambda self_ptr: native_bt.component_sink_get_input_port_count(self._AS_NOT_SELF_SPECIFIC_COMPONENT(self_ptr)),
                               bt2.port._SelfInputPort)

    def _add_input_port(self, name):
        utils._check_str(name)
        fn = native_bt.self_component_sink_add_input_port
        comp_status, priv_port_ptr = fn(self._ptr, name, None)
        _handle_component_status(comp_status,
                                 'cannot add input port to sink component object')
        assert priv_port_ptr
        return bt2.port._SelfInputPort._create_from_ptr(priv_port_ptr)
