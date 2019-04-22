import unittest
from test_utils.test_utils import get_dummy_trace_class


class TraceTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()

    def test_create_default(self):
        trace = self._tc()
        self.assertEqual(trace.name, None)

    def test_create_full(self):
        trace = self._tc(name='my name')
        self.assertEqual(trace.name, 'my name')

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            trace = self._tc(name=17)

    def test_len(self):
        trace = self._tc()
        sc = self._tc.create_stream_class()
        self.assertEqual(len(trace), 0)

        trace.create_stream(sc)
        self.assertEqual(len(trace), 1)

    def test_iter(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)
        trace = self._tc()
        trace.create_stream(sc, id=12)
        trace.create_stream(sc, id=15)
        trace.create_stream(sc, id=17)

        sids = set()

        for stream in trace:
            sids.add(stream.id)

        self.assertEqual(len(sids), 3)
        self.assertIn(12, sids)
        self.assertIn(15, sids)
        self.assertIn(17, sids)

    def test_create_stream(self):
        sc = self._tc.create_stream_class()
        trace = self._tc()
        s_bigras = trace.create_stream(sc, name='bigras')
        s_belanger = trace.create_stream(sc, name='belanger')
        s_boucher = trace.create_stream(sc, name='boucher')

        self.assertEqual(s_bigras.name, 'bigras')
        self.assertEqual(s_belanger.name, 'belanger')
        self.assertEqual(s_boucher.name, 'boucher')

    def test_destruction_listener(self):
        def on_trace_class_destruction(trace_class):
            nonlocal trace_class_destroyed
            trace_class_destroyed = True

        def on_trace_destruction(trace):
            nonlocal trace_destroyed
            trace_destroyed = True

        trace_destroyed = False
        trace_class_destroyed = False

        trace_class = get_dummy_trace_class()
        stream_class = trace_class.create_stream_class()
        trace = trace_class()
        stream = trace.create_stream(stream_class)

        trace_class.add_destruction_listener(on_trace_class_destruction)
        trace.add_destruction_listener(on_trace_destruction)

        self.assertFalse(trace_class_destroyed)
        self.assertFalse(trace_destroyed)

        del trace

        self.assertFalse(trace_class_destroyed)
        self.assertFalse(trace_destroyed)

        del stream

        self.assertFalse(trace_class_destroyed)
        self.assertTrue(trace_destroyed)

        del trace_class

        self.assertFalse(trace_class_destroyed)
        self.assertTrue(trace_destroyed)

        del stream_class

        self.assertTrue(trace_class_destroyed)
        self.assertTrue(trace_destroyed)
