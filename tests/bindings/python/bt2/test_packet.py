from collections import OrderedDict
import unittest
from test_utils.test_utils import run_in_component_init


class PacketTestCase(unittest.TestCase):
    def setUp(self):
        self._packet = self._create_packet()

    def tearDown(self):
        del self._packet

    def _create_packet(self, first=True, with_ph=True, with_pc=True):
        def create_clock_class(comp_self):
            cc = comp_self._create_clock_class('my_cc', 1000)
            tc = comp_self._create_trace_class()
            return (cc, tc)

        clock_class, tc = run_in_component_init(create_clock_class)

        # packet header
        if with_ph:
            ph = tc.create_structure_field_class()
            ph += OrderedDict((
                ('magic', tc.create_signed_integer_field_class(32)),
                ('stream_id', tc.create_signed_integer_field_class(16)),
            ))
            tc.packet_header_field_class = ph

        # stream event context
        sec = tc.create_structure_field_class()
        sec += OrderedDict((
            ('cpu_id', tc.create_signed_integer_field_class(8)),
            ('stuff', tc.create_real_field_class()),
        ))

        # packet context
        pc = None
        if with_pc:
            pc = tc.create_structure_field_class()
            pc += OrderedDict((
                ('something', tc.create_signed_integer_field_class(8)),
                ('something_else', tc.create_real_field_class()),
                ('events_discarded', tc.create_unsigned_integer_field_class(64)),
                ('packet_seq_num', tc.create_unsigned_integer_field_class(64)),
            ))

        # stream class
        sc = tc.create_stream_class(default_clock_class=clock_class,
                                    event_common_context_field_class=sec,
                                    packet_context_field_class=pc)

        # event context
        ec = tc.create_structure_field_class()
        ec += OrderedDict((
            ('ant', tc.create_signed_integer_field_class(16)),
            ('msg', tc.create_string_field_class()),
        ))

        # event payload
        ep = tc.create_structure_field_class()
        ep += OrderedDict((
            ('giraffe', tc.create_signed_integer_field_class(32)),
            ('gnu', tc.create_signed_integer_field_class(8)),
            ('mosquito', tc.create_signed_integer_field_class(8)),
        ))

        # event class
        event_class = sc.create_event_class(name='ec', payload_field_class=ep)
        event_class.common_context_field_class = ec

        # trace
        trace = tc()

        # stream
        stream = trace.create_stream(sc)

        # packet
        # We create 3 packets because we need 2 frozen packets. A packet is
        # frozen when the next packet is created.
        packet1 = stream.create_packet()
        packet2 = stream.create_packet()
        _ = stream.create_packet()

        if first:
            return packet1
        else:
            return packet2

    def test_attr_stream(self):
        self.assertIsNotNone(self._packet.stream)

    def test_get_context_field(self):
        self.assertIsNotNone(self._packet.context_field)

    def test_no_context_field(self):
        packet = self._create_packet(with_pc=False)
        self.assertIsNone(packet.context_field)
