import unittest
from test_utils.test_utils import get_dummy_trace_class


class TraceTestCase(unittest.TestCase):

    def test_create_default(self):
        tc = get_dummy_trace_class()
        trace = tc()
        self.assertEqual(trace.name, None)

    def test_create_full(self):
        tc = get_dummy_trace_class()
        trace = tc(name='my name')
        self.assertEqual(trace.name, 'my name')

    def test_create_invalid_name(self):
        tc = get_dummy_trace_class()
        with self.assertRaises(TypeError):
            trace = tc(name=17)

    def test_len(self):
        tc = get_dummy_trace_class()
        trace = tc()
        sc = tc.create_stream_class()
        self.assertEqual(len(trace), 0)

        trace.create_stream(sc)
        self.assertEqual(len(trace), 1)

    def test_iter(self):
        tc = get_dummy_trace_class()
        sc = tc.create_stream_class(assigns_automatic_stream_id=False)
        trace = tc()
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
