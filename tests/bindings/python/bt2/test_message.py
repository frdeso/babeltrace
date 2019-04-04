from bt2 import value
import collections
import unittest
import copy
import bt2


class AllMessagesTestCase(unittest.TestCase):
    def setUp(self):

        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                self._at = 0

            def __next__(self):
                if self._at == 0:
                    msg = self._create_stream_beginning_message(test_obj._stream)
                elif self._at == 1:
                    msg = self._create_stream_activity_beginning_message(test_obj._stream, default_clock_snapshot=self._at)
                elif self._at == 2:
                    msg = self._create_packet_beginning_message(test_obj._packet, self._at)
                elif self._at == 3:
                    msg = self._create_event_message(test_obj._event_class, test_obj._packet, self._at)
                elif self._at == 4:
                    msg = self._create_inactivity_message(test_obj._clock_class, self._at)
                elif self._at == 5:
                    msg = self._create_packet_end_message(test_obj._packet, self._at)
                elif self._at == 6:
                    msg = self._create_stream_activity_end_message(test_obj._stream, default_clock_snapshot=self._at)
                elif self._at == 7:
                    msg = self._create_stream_end_message(test_obj._stream)
                elif self._at >= 8:
                    raise bt2.Stop

                self._at += 1
                return msg

        class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

                tc = self._create_trace_class()
                cc = self._create_clock_class()
                sc = tc.create_stream_class(default_clock_class=cc)

                # Create payload field class
                my_int_fc = tc.create_signed_integer_field_class(32)
                payload_fc = tc.create_structure_field_class()
                payload_fc += collections.OrderedDict([
                    ('my_int', my_int_fc),
                ])

                ec = sc.create_event_class(name='salut', payload_field_class=payload_fc)

                trace = tc()
                stream = trace.create_stream(sc)
                packet = stream.create_packet()

                test_obj._trace = trace
                test_obj._stream = stream
                test_obj._packet = packet
                test_obj._event_class = ec
                test_obj._clock_class = cc

        test_obj = self
        self._graph = bt2.Graph()
        self._src_comp = self._graph.add_source_component(MySrc, 'my_source')
        self._msg_iter = self._graph.create_output_port_message_iterator(self._src_comp.output_ports['out'])

    def tearDown(self):
        del self._graph
        del self._src_comp
        del self._msg_iter
        del self._event_class
        del self._packet
        del self._stream

    def test_all_msg(self):
        for i, msg in enumerate(self._msg_iter):
            if i == 0:
                self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
            elif i == 1:
                self.assertIsInstance(msg, bt2.message._StreamActivityBeginningMessage)
                self.assertEqual(msg.default_clock_snapshot, i)
            elif i == 2:
                self.assertIsInstance(msg, bt2.message._PacketBeginningMessage)
                self.assertEqual(msg.packet.addr, self._packet.addr)
            elif i == 3:
                self.assertIsInstance(msg, bt2.message._EventMessage)
                self.assertEqual(msg.event.event_class.addr, self._event_class.addr)
            elif i == 4:
                self.assertIsInstance(msg, bt2.message._InactivityMessage)
                self.assertEqual(msg.default_clock_snapshot, i)
            elif i == 5:
                self.assertIsInstance(msg, bt2.message._PacketEndMessage)
                self.assertEqual(msg.packet.addr, self._packet.addr)
            elif i == 6:
                self.assertIsInstance(msg, bt2.message._StreamActivityEndMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.default_clock_snapshot, i)
            elif i == 7:
                self.assertIsInstance(msg, bt2.message._StreamEndMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
            else:
                raise Exception()
