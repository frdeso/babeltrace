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

__all__ = ['IntegerDisplayBase', 'UnsignedIntegerFieldClass', 'SignedIntegerFieldClass',
           'RealFieldClass', 'UnsignedEnumerationFieldClass', 'SignedEnumerationFieldClass',
           'StringFieldClass', 'StructureFieldClass', 'VariantFieldClass',
           'StaticArrayFieldClass', 'DynamicArrayFieldClass']

import collections
from bt2 import native_bt, utils, object
import bt2


def _create_field_class_from_ptr_and_get_ref(ptr):
    typeid = native_bt.field_class_get_type(ptr)
    return _FIELD_CLASS_TYPE_TO_OBJ[typeid]._create_from_ptr_and_get_ref(ptr)


class IntegerDisplayBase:
    BINARY = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY
    OCTAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL
    DECIMAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
    HEXADECIMAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL


class _FieldClass(object._SharedObject):
    _GET_REF_FUNC = native_bt.field_class_get_ref
    _PUT_REF_FUNC = native_bt.field_class_put_ref

    def _check_create_status(self, ptr):
        if ptr is None:
            raise bt2.CreationError('cannot create {} field class object'.format(self._NAME.lower()))


class _IntegerFieldClass(_FieldClass):
    @property
    def range(self):
        size = native_bt.field_class_integer_get_field_value_range(self._ptr)
        assert(size >= 1)
        return size

    def _range(self, size):
        if size < 1 or size > 64:
            raise ValueError("IntegerFieldClass range is not in the range [1,64] ({})".format(size))
        native_bt.field_class_integer_set_field_value_range(self._ptr, size)

    _range = property(fset=_range)

    @property
    def display_base(self):
        base = native_bt.field_class_integer_get_preferred_display_base(
            self._ptr)
        assert(base >= 0)
        return base

    def _display_base(self, base):
        utils._check_int(base)
        native_bt.field_class_integer_set_preferred_display_base(
            self._ptr, base)

    _display_base = property(fset=_display_base)


class _UnsignedIntegerFieldClass(_IntegerFieldClass):
    pass


class _SignedIntegerFieldClass(_IntegerFieldClass):
    pass


class UnsignedIntegerFieldClass(_UnsignedIntegerFieldClass):
    _NAME = 'UnsignedInteger'


class SignedIntegerFieldClass(_SignedIntegerFieldClass):
    _NAME = 'SignedInteger'


class RealFieldClass(_FieldClass):
    _NAME = 'Real'

    @property
    def single_precision(self):
        return native_bt.field_class_real_is_single_precision(self._ptr)

    def _single_precision(self, is_single_precision):
        utils._check_bool(is_single_precision)
        native_bt.field_class_real_set_is_single_precision(
            self._ptr, is_single_precision)

    _single_precision = property(fset=_single_precision)


class _EnumerationFieldClassMappingRange:
    def __init__(self, lower, upper):
        self._lower = lower
        self._upper = upper

    @property
    def lower(self):
        return self._lower

    @property
    def upper(self):
        return self._upper

    def __eq__(self, other):
        return self.lower == other.lower and self.upper == other.upper


class _EnumerationFieldClassMapping(collections.abc.Set):
    def __init__(self, mapping_ptr, label, is_signed):
        self._mapping_ptr = mapping_ptr
        self._is_signed = is_signed
        self._label = label
        if self._is_signed:
            self._range_count = native_bt.field_class_signed_enumeration_mapping_ranges_get_range_count(self._mapping_ptr)
        else:
            self._range_count = native_bt.field_class_unsigned_enumeration_mapping_ranges_get_range_count(self._mapping_ptr)

    @property
    def label(self):
        return self._label

    def __len__(self):
        return self._range_count

    def __contains__(self, other_range):
        for curr_range in self:
            if curr_range == other_range:
                return True
        return False

    def __iter__(self):
        if self._is_signed:
            mapping_ranges_get_range_by_index_fn = native_bt.field_class_signed_enumeration_mapping_ranges_get_range_by_index
        else:
            mapping_ranges_get_range_by_index_fn = native_bt.field_class_unsigned_enumeration_mapping_ranges_get_range_by_index

        for idx in range(len(self)):
            lower, upper = mapping_ranges_get_range_by_index_fn(self._mapping_ptr, idx)
            yield _EnumerationFieldClassMappingRange(lower, upper)


class _EnumerationFieldClass(_IntegerFieldClass, collections.abc.Mapping):
    def __len__(self):
        return native_bt.field_class_enumeration_get_mapping_count(self._ptr)

    def map_range(self, label, lower, upper=None):
        utils._check_str(label)

        if upper is None:
            upper = lower

        if self._is_signed:
            utils._check_int64(lower)
            utils._check_int64(upper)
            map_range_fn = native_bt.field_class_signed_enumeration_map_range
        else:
            utils._check_uint64(lower)
            utils._check_uint64(upper)
            map_range_fn = native_bt.field_class_unsigned_enumeration_map_range

        map_range_fn(self._ptr, label, lower, upper)

    def labels_by_value(self, value):
        if self._is_signed:
            utils._check_int64(value)
            get_mapping_labels_fn = native_bt.field_class_signed_enumeration_get_mapping_labels_by_value
        else:
            utils._check_uint64(value)
            get_mapping_labels_fn = native_bt.field_class_unsigned_enumeration_get_mapping_labels_by_value

        ret, labels = get_mapping_labels_fn(self._ptr, value);
        utils._handle_ret(ret, "cannot get mapping labels for {}".format(self._NAME.lower()))

        return labels

    def __iter__(self):
        if self._is_signed:
            borrow_mapping_by_index_fn = native_bt.field_class_signed_enumeration_borrow_mapping_by_index_const
        else:
            borrow_mapping_by_index_fn = native_bt.field_class_unsigned_enumeration_borrow_mapping_by_index_const

        for idx in range(len(self)):
            label, mapping_ptr = borrow_mapping_by_index_fn(self._ptr, idx)
            yield _EnumerationFieldClassMapping(mapping_ptr, label, is_signed=self._is_signed)

    def __getitem__(self, key):
        utils._check_str(key)
        for mapping in self:
            if mapping.label in key:
                return mapping

    def __iadd__(self, mappings):
        for mapping in mappings:
            for range in mapping:
                self.map_range(mapping.label, range.lower, range.upper)

        return self


class UnsignedEnumerationFieldClass(_EnumerationFieldClass, _UnsignedIntegerFieldClass):
    _NAME = 'UnsignedEnumerationFieldClass'
    _is_signed = False


class SignedEnumerationFieldClass(_EnumerationFieldClass, _SignedIntegerFieldClass):
    _NAME = 'SignedEnumerationFieldClass'
    _is_signed = True


class StringFieldClass(_FieldClass):
    _NAME = 'String'

class _FieldContainer(collections.abc.Mapping):
    def __len__(self):
        count = self._count()
        assert(count >= 0)
        return count

    def __getitem__(self, key):
        if not isinstance(key, str):
            raise TypeError("'{}' is not a 'str' object".format(key.__class__.__name__))

        ptr = self._borrow_field_class_ptr_by_name(key)

        if ptr is None:
            raise KeyError(key)

        return _create_field_class_from_ptr_and_get_ref(ptr)

    def __iter__(self):
        return self._ITER_CLS(self)

    def append_field(self, name, field_class):
        utils._check_str(name)
        utils._check_type(field_class, _FieldClass)
        ret = self._add_field(name, field_class._ptr)
        utils._handle_ret(ret, "cannot add field to {} field class object".format(self._NAME.lower()))

    def __iadd__(self, fields):
        for name, field_class in fields.items():
            self.append_field(name, field_class)

        return self

    def at_index(self, index):
        utils._check_uint64(index)
        return self._at(index)


class _StructureFieldClassFieldIterator(collections.abc.Iterator):
    def __init__(self, struct_field_class):
        self._struct_field_class = struct_field_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._struct_field_class):
            raise StopIteration

        get_fc_by_index = native_bt.field_class_structure_borrow_member_by_index_const
        name, field_class_ptr = get_fc_by_index(self._struct_field_class._ptr, self._at)
        self._at += 1
        return name


class StructureFieldClass(_FieldClass, _FieldContainer):
    _NAME = 'Structure'
    _ITER_CLS = _StructureFieldClassFieldIterator
    def _count(self):
        return native_bt.field_class_structure_get_member_count(self._ptr)

    def _add_field(self, name, ptr):
        return native_bt.field_class_structure_append_member(self._ptr, name, ptr)

    def _borrow_field_class_ptr_by_name(self, key):
        field_class_ptr = native_bt.field_class_structure_borrow_member_field_class_by_name(self._ptr, key)
        return field_class_ptr

    def _at(self, index):
        if index < 0 or index >= len(self):
            raise IndexError

        name, field_class_ptr = native_bt.field_class_structure_borrow_member_by_index_const(self._ptr, index)

        return _create_field_class_from_ptr_and_get_ref(field_class_ptr)


class _VariantFieldClassFieldIterator(collections.abc.Iterator):
    def __init__(self, variant_field_class):
        self._variant_field_class = variant_field_class
        self._at = 0

    def __next__(self):
        if self._at == len(self._variant_field_class):
            raise StopIteration

        name, field_class_ptr = native_bt.field_class_variant_borrow_option_by_index_const(self._variant_field_class._ptr,
                                                                                           self._at)
        self._at += 1
        return name


class VariantFieldClass(_FieldClass, _FieldContainer):
    _NAME = 'Variant'
    _ITER_CLS = _VariantFieldClassFieldIterator
    @property
    def selector_field_path(self):
        ptr = native_bt.field_class_variant_borrow_selector_field_path_const(self._ptr)
        if ptr is None:
            return

        return bt2.FieldPath._create_from_ptr_and_get_ref(ptr)

    def _set_selector_field_class(self, selector_ft):
        ret = native_bt.field_class_variant_set_selector_field_class(self._ptr, selector_ft._ptr)
        utils._handle_ret(ret, "cannot set variant selector field type")

    selector_field_class = property(fset = _set_selector_field_class)

    def _count(self):
        return native_bt.field_class_variant_get_option_count(self._ptr)

    def _add_field(self, name, ptr):
        return native_bt.field_class_variant_append_option(self._ptr, name, ptr)

    def _borrow_field_class_ptr_by_name(self, key):
        field_class_ptr = native_bt.field_class_variant_borrow_option_field_class_by_name(self._ptr, key)
        return field_class_ptr

    def _at(self, index):
        if index < 0 or index >= len(self):
            raise IndexError

        name, field_class_ptr = native_bt.field_class_variant_borrow_option_by_index_const(
            self._ptr, index)

        return _create_field_class_from_ptr_and_get_ref(field_class_ptr)


class ArrayFieldClass(_FieldClass):
    @property
    def element_field_class(self):
        elem_fc_ptr = native_bt.field_class_array_borrow_element_field_class_const(self._ptr)
        return _create_field_class_from_ptr_and_get_ref(elem_fc_ptr)


class StaticArrayFieldClass(ArrayFieldClass):
    @property
    def length(self):
        return native_bt.field_class_static_array_get_length(self._ptr)


class DynamicArrayFieldClass(ArrayFieldClass):
    @property
    def length_field_path(self):
        ptr = native_bt.field_class_dynamic_array_borrow_length_field_path_const(self._ptr)
        if ptr is None:
            return

        return bt2.FieldPath._create_from_ptr_and_get_ref(ptr)

    def _set_length_field_class(self, length_ft):
        utils._check_type(length_ft, _UnsignedIntegerFieldClass)
        ret = native_bt.field_class_dynamic_array_set_length_field_class(self._ptr, length_ft._ptr)
        utils._handle_ret(ret, "cannot set dynamic array length field type")

    length_field_class = property(fset=_set_length_field_class)


_FIELD_CLASS_TYPE_TO_OBJ = {
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: UnsignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: SignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_REAL: RealFieldClass,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: UnsignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: SignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRING: StringFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: StructureFieldClass,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: StaticArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY: DynamicArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_VARIANT: VariantFieldClass,
}
