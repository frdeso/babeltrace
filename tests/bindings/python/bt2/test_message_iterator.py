from bt2 import value
import collections
import unittest
import copy
import bt2

class SelfPortInputMessageIteratorTestCase(unittest.TestCase):
    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                pass

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                next(self._msg_iter)

            def _graph_is_configured(self):
                self._msg_iter = self._input_ports['in'].create_message_iterator()
                nonlocal src_comp_addr
                nonlocal src_comp_cls_addr
                src_comp_addr = self._msg_iter.component.addr
                src_comp_cls_addr = self._msg_iter.component.component_class.addr

        src_comp_addr = 0
        src_comp_cls_addr = 0
        graph = bt2.Graph()
        src_comp = graph.add_source_component(MySource, 'src')
        sink_comp = graph.add_sink_component(MySink, 'sink')
        graph.connect_ports(src_comp.output_ports['out'],
                            sink_comp.input_ports['in'])

        graph.run()

        self.assertEqual(src_comp_addr, src_comp.addr)
        self.assertEqual(src_comp_cls_addr, MySource.addr)


class UserMessageIteratorTestCase(unittest.TestCase):
    @staticmethod
    def _create_graph(src_comp_cls):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                next(self._msg_iter)

            def _graph_is_configured(self):
                self._msg_iter = self._input_ports['in'].create_message_iterator()

        graph = bt2.Graph()
        src_comp = graph.add_source_component(src_comp_cls, 'src')
        sink_comp = graph.add_sink_component(MySink, 'sink')
        graph.connect_ports(src_comp.output_ports['out'],
                            sink_comp.input_ports['in'])
        return graph

    def test_init(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                nonlocal initialized
                initialized = True

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        initialized = False
        graph = self._create_graph(MySource)
        graph.run()
        self.assertTrue(initialized)

    def test_finalize(self):
        class MyIter(bt2._UserMessageIterator):
            def _finalize(self):
                nonlocal finalized
                finalized = True

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        finalized = False
        graph = self._create_graph(MySource)
        graph.run()
        del graph
        self.assertTrue(finalized)

    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                nonlocal salut
                salut = self._component._salut

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')
                self._salut = 23

        salut = None
        graph = self._create_graph(MySource)
        graph.run()
        self.assertEqual(salut, 23)

    def test_addr(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                nonlocal addr
                addr = self.addr

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        addr = None
        graph = self._create_graph(MySource)
        graph.run()
        self.assertIsNotNone(addr)
        self.assertNotEqual(addr, 0)


class OutputPortMessageIteratorTestCase(unittest.TestCase):
    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                self._at = 0

            def __next__(self):
                if self._at == 7:
                    raise bt2.Stop

                if self._at == 0:
                    msg = self._create_stream_beginning_message(test_obj._stream)
                elif self._at == 1:
                    msg = self._create_packet_beginning_message(test_obj._packet)
                elif self._at == 5:
                    msg = self._create_packet_end_message(test_obj._packet)
                elif self._at == 6:
                    msg = self._create_stream_end_message(test_obj._stream)
                else:
                    msg = self._create_event_message(test_obj._event_class, test_obj._packet)
                    msg.event.payload_field['my_int'] = self._at * 3

                self._at += 1
                return msg

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

                trace_class = self._create_trace_class()
                stream_class = trace_class.create_stream_class()

                # Create payload field class
                my_int_ft = trace_class.create_signed_integer_field_class(32)
                payload_ft = trace_class.create_structure_field_class()
                payload_ft += collections.OrderedDict([
                    ('my_int', my_int_ft),
                ])

                event_class = stream_class.create_event_class(name='salut', payload_field_class=payload_ft)

                trace = trace_class()
                stream = trace.create_stream(stream_class)
                packet = stream.create_packet()

                test_obj._event_class = event_class
                test_obj._stream = stream
                test_obj._packet = packet

        test_obj = self
        graph = bt2.Graph()
        src = graph.add_source_component(MySource, 'src')
        msg_iter = graph.create_output_port_message_iterator(src.output_ports['out'])

        for at, msg in enumerate(msg_iter):
            if at == 0:
                self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)
            elif at == 1:
                self.assertIsInstance(msg, bt2.message._PacketBeginningMessage)
            elif at == 5:
                self.assertIsInstance(msg, bt2.message._PacketEndMessage)
            elif at == 6:
                self.assertIsInstance(msg, bt2.message._StreamEndMessage)
            else:
                self.assertIsInstance(msg, bt2.message._EventMessage)
                self.assertEqual(msg.event.event_class.name, 'salut')
                field = msg.event.payload_field['my_int']
                self.assertEqual(field, at * 3)
