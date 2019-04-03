# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
# Copyright (c) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
# Copyright (c) 2019 Simon Marchi <simon.marchi@efficios.com>
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

__all__ = ['TraceClass']

import bt2
from bt2 import native_bt, utils
import uuid as uuidp
import collections.abc
import functools


class _TraceClassEnvIterator(collections.abc.Iterator):
    def __init__(self, trace_class_env):
        self._trace_class_env = trace_class_env
        self._at = 0

    def __next__(self):
        if self._at == len(self._trace_class_env):
            raise StopIteration

        trace_class_ptr = self._trace_class_env._trace_class._ptr
        entry_name, value = native_bt.trace_class_borrow_environment_entry_by_index_const(trace_class_ptr, self._at)
        assert entry_name is not None
        self._at += 1
        return entry_name


class _TraceClassEnv(collections.abc.MutableMapping):
    def __init__(self, trace_class):
        self._trace_class = trace_class

    def __getitem__(self, key):
        utils._check_str(key)

        # TODO: Use bt_trace_class_borrow_environment_entry_value_by_name_const ?
        for idx, entry in enumerate(self):
            if entry == key:
                entry_name, value_ptr = native_bt.trace_class_borrow_environment_entry_by_index_const(self._trace_class._ptr, idx)
                return bt2.value._create_from_ptr_and_get_ref(value_ptr)

        raise KeyError(key)

    def __setitem__(self, key, value):
        if isinstance(value, str):
            set_env_entry_fn = native_bt.trace_class_set_environment_entry_string
        elif isinstance(value, int):
            set_env_entry_fn = native_bt.trace_class_set_environment_entry_integer
        else:
            raise TypeError('expected str or int, got {}'.format(type(value)))

        ret = set_env_entry_fn(self._trace_class._ptr, key, value)

        utils._handle_ret(ret, "cannot set trace class object's environment entry")

    def __delitem__(self, key):
        raise NotImplementedError

    def __len__(self):
        count = native_bt.trace_class_get_environment_entry_count(self._trace_class._ptr)
        assert count >= 0
        return count

    def __iter__(self):
        # TODO Use yield?
        return _TraceClassEnvIterator(self)


class _StreamClassIterator(collections.abc.Iterator):
    def __init__(self, trace_class):
        self._trace_class = trace_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._trace_class):
            raise StopIteration

        sc_ptr = native_bt.trace_class_borrow_stream_class_by_index_const(self._trace_class._ptr, self._at)
        assert sc_ptr
        id = native_bt.stream_class_get_id(sc_ptr)
        assert id >= 0
        self._at += 1
        return id


def _trace_class_destruction_listener_from_native(user_listener, trace_class_ptr):
    trace_class = bt2.trace_class.TraceClass._create_from_ptr_and_get_ref(trace_class_ptr)
    user_listener(trace_class)


class TraceClass(bt2._SharedObject, collections.abc.Mapping):
    _GET_REF_FUNC = native_bt.trace_class_get_ref
    _PUT_REF_FUNC = native_bt.trace_class_put_ref

    @property
    def uuid(self):
        uuid_bytes = native_bt.trace_class_get_uuid(self._ptr)
        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    def _uuid(self, uuid):
        utils._check_type(uuid, uuidp.UUID)
        native_bt.trace_class_set_uuid(self._ptr, uuid.bytes)

    _uuid = property(fset=_uuid)

    # TODO: Choose between this, or a _create_trace method.
    def __call__(self, name=None):
        '''Instantiate a trace of this class.'''

        trace_ptr = native_bt.trace_create(self._ptr)

        if trace_ptr is None:
            raise bt2.CreationError('cannot create trace class object')

        trace = bt2.trace.Trace._create_from_ptr(trace_ptr)

        if name is not None:
            trace._name = name

        return trace

    def __len__(self):
        '''Number of stream classes in this trace class.'''
        count = native_bt.trace_class_get_stream_class_count(self._ptr)
        assert count >= 0
        return count

    def __getitem__(self, key):
        '''Get a stream class by stream id.'''
        utils._check_int64(key)

        # TODO: Use bt_trace_class_borrow_stream_class_by_id_const ?
        for idx, sc_id in enumerate(self):
            if sc_id == key:
                sc_ptr = native_bt.trace_class_borrow_stream_class_by_index_const(self._ptr, idx)
                return bt2.stream_class._StreamClass._create_from_ptr_and_get_ref(sc_ptr)

        raise KeyError(key)

    def __iter__(self):
        # TODO: Use yield?
        return _StreamClassIterator(self)

    @property
    def env(self):
        return _TraceClassEnv(self)

    def create_stream_class(self, id=None,
                            name=None,
                            packet_context_field_class=None,
                            event_common_context_field_class=None,
                            default_clock_class=None,
                            default_clock_always_known=None,
                            assigns_automatic_event_class_id=None,
                            assigns_automatic_stream_id=None):

        if self.assigns_automatic_stream_class_id:
            sc_ptr = native_bt.stream_class_create(self._ptr)
        else:
            if id is None:
                raise bt2.CreationError('cannot create stream class object')
            sc_ptr = native_bt.stream_class_create_with_id(self._ptr, id)

        sc = bt2.stream_class._StreamClass._create_from_ptr(sc_ptr)

        if name is not None:
            sc._name = name

        if packet_context_field_class is not None:
            sc._packet_context_field_class = packet_context_field_class

        if event_common_context_field_class is not None:
            sc._event_common_context_field_class = event_common_context_field_class

        if default_clock_class is not None:
            sc._default_clock_class = default_clock_class

        if default_clock_always_known is not None:
            # Ignored for now
            pass

        if assigns_automatic_event_class_id is not None:
            sc._assigns_automatic_event_class_id = assigns_automatic_event_class_id

        if assigns_automatic_stream_id is not None:
            sc._assigns_automatic_stream_id = assigns_automatic_stream_id

        return sc

    @property
    def assigns_automatic_stream_class_id(self):
        return native_bt.trace_class_assigns_automatic_stream_class_id(self._ptr)

    def _assigns_automatic_stream_class_id(self, auto_id):
        utils._check_bool(auto_id)
        return native_bt.trace_class_set_assigns_automatic_stream_class_id(self._ptr, auto_id)

    _assigns_automatic_stream_class_id = property(fset=_assigns_automatic_stream_class_id)

    # Field class creation methods.

    def _check_create_status(self, ptr):
        if ptr is None:
            raise bt2.CreationError(
                'cannot create {} field type object'.format(self._NAME.lower()))

    def _create_integer_field_class(self, create_func, py_cls, range, display_base):
        field_class_ptr = create_func(self._ptr)
        self._check_create_status(field_class_ptr)

        field_class = py_cls._create_from_ptr(field_class_ptr)

        if range is not None:
            field_class._range = range

        if display_base is not None:
            field_class._display_base = display_base

        return field_class

    def create_signed_integer_field_class(self, range=None, display_base=None):
        return self._create_integer_field_class(native_bt.field_class_signed_integer_create,
                                                bt2.field_class.SignedIntegerFieldClass,
                                                range, display_base)

    def create_unsigned_integer_field_class(self, range=None, display_base=None):
        return self._create_integer_field_class(native_bt.field_class_unsigned_integer_create,
                                                bt2.field_class.UnsignedIntegerFieldClass,
                                                range, display_base)

    def create_signed_enumeration_field_class(self, range=None, display_base=None):
        return self._create_integer_field_class(native_bt.field_class_signed_enumeration_create,
                                                bt2.field_class.SignedEnumerationFieldClass,
                                                range, display_base)

    def create_unsigned_enumeration_field_class(self, range=None, display_base=None):
        return self._create_integer_field_class(native_bt.field_class_unsigned_enumeration_create,
                                                bt2.field_class.UnsignedEnumerationFieldClass,
                                                range, display_base)

    def create_real_field_class(self, is_single_precision=None):
        field_class_ptr = native_bt.field_class_real_create(self._ptr)
        self._check_create_status(field_class_ptr)

        field_class = bt2.field_class.RealFieldClass._create_from_ptr(field_class_ptr)

        if is_single_precision is not None:
            field_class._single_precision = is_single_precision

        return field_class

    def create_structure_field_class(self):
        field_class_ptr = native_bt.field_class_structure_create(self._ptr)
        self._check_create_status(field_class_ptr)

        return bt2.field_class.StructureFieldClass._create_from_ptr(field_class_ptr)

    def create_string_field_class(self):
        field_class_ptr = native_bt.field_class_string_create(self._ptr)
        self._check_create_status(field_class_ptr)

        return bt2.field_class.StringFieldClass._create_from_ptr(field_class_ptr)

    def create_static_array_field_class(self, elem_fc, length):
        utils._check_type(elem_fc, bt2.field_class._FieldClass)
        utils._check_uint64(length)
        ptr = native_bt.field_class_static_array_create(self._ptr, elem_fc._ptr, length)
        self._check_create_status(ptr)

        return bt2.field_class.StaticArrayFieldClass._create_from_ptr_and_get_ref(ptr)

    def create_dynamic_array_field_class(self, elem_fc, length_fc=None):
        utils._check_type(elem_fc, bt2.field_class._FieldClass)
        ptr = native_bt.field_class_dynamic_array_create(self._ptr, elem_fc._ptr)
        self._check_create_status(ptr)
        obj = bt2.field_class.DynamicArrayFieldClass._create_from_ptr(ptr)

        if length_fc is not None:
            obj.length_field_class = length_fc

        return obj

    def create_variant_field_class(self, selector_fc=None):
        ptr = native_bt.field_class_variant_create(self._ptr)
        self._check_create_status(ptr)
        obj = bt2.field_class.VariantFieldClass._create_from_ptr(ptr)

        if selector_fc is not None:
            obj.selector_field_class = selector_fc

        return obj

    def add_destruction_listener(self, listener):
        '''Add a listener to be called when the trace class is destroyed.'''
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.py3_trace_class_add_destruction_listener
        listener_from_native = functools.partial(_trace_class_destruction_listener_from_native,
                                                 listener)

        listener_id = fn(self._ptr, listener_from_native)
        if listener_id is None:
            utils._raise_bt2_error('cannot add destruction listener to trace class object')

        return bt2._ListenerHandle(listener_id, self)
