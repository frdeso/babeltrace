import uuid
import unittest
import bt2
from test_utils.test_utils import run_in_component_init, get_dummy_trace_class


class TraceClassTestCase(unittest.TestCase):

    def test_create_default(self):
        tc = run_in_component_init(lambda comp_self: comp_self._create_trace_class())

        self.assertEqual(len(tc), 0)
        self.assertIsNone(tc.uuid)
        self.assertEqual(tc.assigns_automatic_stream_class_id, True)

    def test_uuid(self):
        tc_uuid = uuid.UUID('da7d6b6f-3108-4706-89bd-ab554732611b')

        tc = run_in_component_init(lambda comp_self: comp_self._create_trace_class(uuid=tc_uuid))

        self.assertEqual(tc.uuid, tc_uuid)

    # TODO: Move this to test_stream_class
    def test_create_stream_class_full(self):
        tc, cc = run_in_component_init(lambda comp_self: (comp_self._create_trace_class(), comp_self._create_clock_class()))

        packet_context_fc = tc.create_structure_field_class()
        packet_context_fc.append_field('magic', tc.create_signed_integer_field_class(32))

        event_common_context_fc = tc.create_structure_field_class()
        event_common_context_fc.append_field('magic', tc.create_signed_integer_field_class(32))
        sc = tc.create_stream_class(id=12,
                                    packet_context_field_class=packet_context_fc,
                                    event_common_context_field_class=event_common_context_fc,
                                    default_clock_class=cc,
                                    default_clock_always_known=True,
                                    assigns_automatic_event_class_id=False,
                                    assigns_automatic_stream_id=False)

        self.assertEqual(sc.packet_context_field_class.addr, packet_context_fc.addr)
        self.assertEqual(sc.event_common_context_field_class.addr, event_common_context_fc.addr)
        self.assertEqual(sc.default_clock_class.addr, cc.addr)
        self.assertTrue(sc.default_clock_always_known)
        self.assertFalse(sc.assigns_automatic_event_class_id)
        self.assertFalse(sc.assigns_automatic_stream_id)

    def test_env_get(self):
        tc = run_in_component_init(lambda comp_self: comp_self._create_trace_class(env={'hello': 'you', 'foo': -5}))

        self.assertEqual(tc.env['hello'], 'you')
        self.assertEqual(tc.env['foo'], -5)

        with self.assertRaises(KeyError):
            tc.env['lel']

    @staticmethod
    def _create_trace_class_with_some_stream_classes():
        tc = run_in_component_init(lambda comp_self: comp_self._create_trace_class(assigns_automatic_stream_class_id=False))
        sc1 = tc.create_stream_class(id=12)
        sc2 = tc.create_stream_class(id=54)
        sc3 = tc.create_stream_class(id=2018)
        return tc, sc1, sc2, sc3

    def test_getitem(self):
        tc, _, _, sc3 = self._create_trace_class_with_some_stream_classes()
        self.assertEqual(tc[2018].addr, sc3.addr)

    def test_getitem_wrong_key_type(self):
        tc, _, _, _ = self._create_trace_class_with_some_stream_classes()
        with self.assertRaises(TypeError):
            tc['hello']

    def test_getitem_wrong_key(self):
        tc, _, _, _ = self._create_trace_class_with_some_stream_classes()
        with self.assertRaises(KeyError):
            tc[4]

    def test_len(self):
        tc = get_dummy_trace_class()
        self.assertEqual(len(tc), 0)
        tc.create_stream_class()
        self.assertEqual(len(tc), 1)

    def test_iter(self):
        tc, sc1, sc2, sc3 = self._create_trace_class_with_some_stream_classes()

        for sc_id, stream_class in tc.items():
            self.assertIsInstance(stream_class, bt2.stream_class._StreamClass)

            if sc_id == 12:
                self.assertEqual(stream_class.addr, sc1.addr)
            elif sc_id == 54:
                self.assertEqual(stream_class.addr, sc2.addr)
            elif sc_id == 2018:
                self.assertEqual(stream_class.addr, sc3.addr)

    # destruction listener is test in test_trace.py, along with trace's
    # destruction listener.
