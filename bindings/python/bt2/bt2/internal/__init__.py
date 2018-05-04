from bt2.internal.object import _Object
from bt2.internal.clock_class import _ClockClass, ClockClassOffset
from bt2.internal.event_class import _EventClass
from bt2.internal.stream_class import _StreamClass
from bt2.internal.field_types import _IntegerFieldType, _FloatingPointNumberFieldType, _EnumerationFieldType, _StringFieldType, _StructureFieldType, _ArrayFieldType, _SequenceFieldType, _VariantFieldType
#
from bt2.internal.trace import  _Trace
from bt2.internal.event import  _Event
from bt2.internal.stream import _Stream
from bt2.internal.domain import _Domain

from bt2.internal.fields import _Field, _NumericField, _IntegralField, _RealField, _IntegerField, _FloatingPointNumberField, _EnumerationField, _StringField, _ContainerField, _StructureField, _VariantField, _ArraySequenceField, _ArrayField, _SequenceField


class Error(Exception):
    pass


class CreationError(Error):
    pass


class InvalidQueryObject(Error):
    pass


class InvalidQueryParams(Error):
    pass


class UnsupportedFeature(Exception):
    pass


class NoSinkComponent(Exception):
    pass


class TryAgain(Exception):
    pass


class Stop(StopIteration):
    pass


class PortConnectionRefused(Exception):
    pass


class IncompleteUserClass(Error):
    pass


class CannotConsumeGraph(Error):
    pass


class GraphCanceled(Exception):
    pass


class QueryExecutorCanceled(Exception):
    pass


class NotificationIteratorCanceled(Exception):
    pass


class ConnectionEnded(Exception):
    pass
