import unittest
from test_utils.test_utils import run_in_component_init


class StreamTestCase(unittest.TestCase):
    @staticmethod
    def _get_test_stream_class_and_trace():
        def f(comp_self):
            return comp_self._create_trace_class()

        tc = run_in_component_init(f)
        sc = tc.create_stream_class(assigns_automatic_stream_id=False)
        tr = tc()

        return sc, tr

    def test_attr_stream_class(self):
        sc, tr = self._get_test_stream_class_and_trace()
        stream = tr.create_stream(sc, id=1608)
        self.assertEqual(stream.stream_class, sc)

    def test_attr_name(self):
        sc, tr = self._get_test_stream_class_and_trace()
        a_stream_with_no_name = tr.create_stream(sc, id=1608)
        self.assertEqual(a_stream_with_no_name.name, None)

        a_stream_with_a_name = tr.create_stream(sc, id=1609, name='best stream')
        self.assertEqual(a_stream_with_a_name.name, 'best stream')

    def test_attr_id(self):
        sc, tr = self._get_test_stream_class_and_trace()
        stream = tr.create_stream(sc, id=1608)
        self.assertEqual(stream.id, 1608)

    def test_default_clock_always_known(self):
        pass

    def test_counter_snapshots(self):
        pass

    def test_packet_default_clock_value(self):
        pass
