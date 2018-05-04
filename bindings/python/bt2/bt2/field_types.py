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

from . import domain
from bt2 import internal

ByteOrder = domain._Domain._ByteOrder
Encoding = domain._Domain._Encoding
Base = domain._Domain._Base

class IntegerFieldType(internal._IntegerFieldType, domain._DomainProvider):
    pass


class FloatingPointNumberFieldType(internal._FloatingPointNumberFieldType, domain._DomainProvider):
    pass


class EnumerationFieldType(internal._EnumerationFieldType, domain._DomainProvider):
    pass


class StringFieldType(internal._StringFieldType, domain._DomainProvider):
    pass


class StructureFieldType(internal._StructureFieldType, domain._DomainProvider):
    pass


class VariantFieldType(internal._VariantFieldType, domain._DomainProvider):
    pass


class ArrayFieldType(internal._ArrayFieldType, domain._DomainProvider):
    pass


class SequenceFieldType(internal._SequenceFieldType, domain._DomainProvider):
    pass

domain._Domain._FIELD_TYPE_ID_TO_OBJ = {
    domain._Domain.FIELD_TYPE_ID_INTEGER: IntegerFieldType,
    domain._Domain.FIELD_TYPE_ID_FLOAT: FloatingPointNumberFieldType,
    domain._Domain.FIELD_TYPE_ID_ENUM: EnumerationFieldType,
    domain._Domain.FIELD_TYPE_ID_STRING: StringFieldType,
    domain._Domain.FIELD_TYPE_ID_STRUCT: StructureFieldType,
    domain._Domain.FIELD_TYPE_ID_ARRAY: ArrayFieldType,
    domain._Domain.FIELD_TYPE_ID_SEQUENCE: SequenceFieldType,
    domain._Domain.FIELD_TYPE_ID_VARIANT: VariantFieldType,
}

domain._Domain.IntegerFieldType = IntegerFieldType
domain._Domain.FloatingPointNumberFieldType = FloatingPointNumberFieldType
domain._Domain.EnumerationFieldType = EnumerationFieldType
domain._Domain.StringFieldType = StringFieldType
domain._Domain.StructureFieldType = StructureFieldType
domain._Domain.VariantFieldType = VariantFieldType
domain._Domain.ArrayFieldType = ArrayFieldType
domain._Domain.SequenceFieldType = SequenceFieldType

