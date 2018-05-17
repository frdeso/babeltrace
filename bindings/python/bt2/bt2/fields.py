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

__all__ = ['_ArrayField', '_EnumerationField', '_Field'
        ,'_FloatingPointNumberField', '_IntegerField', '_SequenceField'
        ,'_StringField', '_StructureField', '_VariantField']

from . import domain
from bt2 import internal

class _Field(internal._Field, domain._DomainProvider):
    pass

class _NumericField(internal._NumericField, _Field, domain._DomainProvider):
    pass

class _IntegralField(internal._IntegralField, _NumericField, domain._DomainProvider):
    pass

class _RealField(internal._RealField, _NumericField, domain._DomainProvider):
    pass

class _IntegerField(internal._IntegerField, _IntegralField, domain._DomainProvider):
    pass

class _FloatingPointNumberField(internal._FloatingPointNumberField, _RealField, domain._DomainProvider):
    pass

class _EnumerationField(internal._EnumerationField, _IntegerField, domain._DomainProvider):
    pass

class _StringField(internal._StringField, _Field,  domain._DomainProvider):
    pass

class _ContainerField(internal._ContainerField, _Field, domain._DomainProvider):
    pass

class _StructureField(internal._StructureField, _ContainerField, domain._DomainProvider):
    pass

class _VariantField(internal._VariantField, _Field, domain._DomainProvider):
    pass

class _ArraySequenceField(internal._ArraySequenceField, _ContainerField, domain._DomainProvider):
    pass

class _ArrayField(internal._ArrayField, _ArraySequenceField, domain._DomainProvider):
    pass

class _SequenceField(internal._SequenceField, _ArraySequenceField, domain._DomainProvider):
    pass

domain._Domain._FIELD_ID_TO_OBJ = {
    domain._Domain.FIELD_ID_INTEGER: _IntegerField,
    domain._Domain.FIELD_ID_FLOAT: _FloatingPointNumberField,
    domain._Domain.FIELD_ID_ENUM: _EnumerationField,
    domain._Domain.FIELD_ID_STRING: _StringField,
    domain._Domain.FIELD_ID_STRUCT: _StructureField,
    domain._Domain.FIELD_ID_ARRAY: _ArrayField,
    domain._Domain.FIELD_ID_SEQUENCE: _SequenceField,
    domain._Domain.FIELD_ID_VARIANT: _VariantField,
}

domain._Domain.IntegerField=_IntegerField
domain._Domain.FloatingPointNumberField=_FloatingPointNumberField
domain._Domain.EnumerationField=_EnumerationField
domain._Domain.StringField=_StringField
domain._Domain.StructureField=_StructureField
domain._Domain.VariantField=_VariantField
domain._Domain.ArraySequenceField=_ArraySequenceField
domain._Domain.ArrayField=_ArrayField
domain._Domain.SequenceField=_SequenceField
