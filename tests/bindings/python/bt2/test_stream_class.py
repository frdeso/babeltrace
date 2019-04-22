import unittest
import bt2
from test_utils.test_utils import run_in_component_init


class StreamClassTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            tc = comp_self._create_trace_class(assigns_automatic_stream_class_id = False)
            cc = comp_self._create_clock_class()
            return tc, cc

        self._tc, self._cc = run_in_component_init(f)

        self._packet_context_fc = self._tc.create_structure_field_class()
        self._packet_context_fc.append_field(
            'menu', self._tc.create_real_field_class())
        self._packet_context_fc.append_field(
            'sticker', self._tc.create_string_field_class())
        self._event_common_context_fc = self._tc.create_structure_field_class()
        self._event_common_context_fc.append_field(
            'msg', self._tc.create_string_field_class())

        self._trace = self._tc()

        self._sc = self._tc.create_stream_class(id=12, name='my_stream_class',
                                                assigns_automatic_event_class_id=False,
                                                event_common_context_field_class=self._event_common_context_fc,
                                                packet_context_field_class=self._packet_context_fc)

        # Create event class 1
        context_fc = self._tc.create_structure_field_class()
        context_fc.append_field('allo', self._tc.create_string_field_class())
        context_fc.append_field(
            'zola', self._tc.create_unsigned_integer_field_class(18))

        payload_fc = self._tc.create_structure_field_class()
        payload_fc.append_field('zoom', self._tc.create_string_field_class())

        self._ec1 = self._sc.create_event_class(id=23,
                                                specific_context_field_class=context_fc,
                                                payload_field_class=payload_fc)

        # Create event class 2
        context_fc = self._tc.create_structure_field_class()
        context_fc.append_field('allo', self._tc.create_string_field_class())
        context_fc.append_field(
            'zola', self._tc.create_unsigned_integer_field_class(18))

        payload_fc = self._tc.create_structure_field_class()
        payload_fc.append_field('zoom', self._tc.create_string_field_class())

        self._ec2 = self._sc.create_event_class(id=17,
                                                specific_context_field_class=context_fc,
                                                payload_field_class=payload_fc)

    def tearDown(self):
        del self._packet_context_fc
        del self._event_common_context_fc
        del self._ec1
        del self._ec2
        del self._sc

    def test_create(self):
        self.assertEqual(self._sc.name, 'my_stream_class')
        self.assertEqual(self._sc.id, 12)
        self.assertEqual(self._sc.packet_context_field_class.addr,
                         self._packet_context_fc.addr)
        self.assertEqual(self._sc.event_common_context_field_class.addr,
                         self._event_common_context_fc.addr)
        self.assertEqual(self._sc[23]._ptr, self._ec1._ptr)
        self.assertEqual(self._sc[17]._ptr, self._ec2._ptr)
        self.assertEqual(len(self._sc), 2)

    def test_automatic_ids(self):
        sc = self._tc.create_stream_class(
            id=14, assigns_automatic_stream_id=True, assigns_automatic_event_class_id=True)
        ec = sc.create_event_class()
        stream = self._trace.create_stream(sc)
        self.assertTrue(sc.assigns_automatic_stream_id)
        self.assertTrue(sc.assigns_automatic_event_class_id)
        self.assertIsNotNone(ec.id)
        self.assertIsNotNone(stream.id)

    def test_no_automatic_ids(self):
        sc = self._tc.create_stream_class(
            id=14, assigns_automatic_stream_id=False, assigns_automatic_event_class_id=False)
        ec = sc.create_event_class(id=121)
        stream = self._trace.create_stream(sc, id=0x1112)
        self.assertFalse(sc.assigns_automatic_stream_id)
        self.assertFalse(sc.assigns_automatic_event_class_id)
        self.assertIsNotNone(ec.id)
        self.assertIsNotNone(stream.id)

    def test_event_class_create_full(self):
        sc = self._tc.create_stream_class(id=1212)
        context_fc = self._tc.create_structure_field_class()
        context_fc.append_field('allo', self._tc.create_string_field_class())
        context_fc.append_field(
            'zola', self._tc.create_unsigned_integer_field_class(18))

        payload_fc = self._tc.create_structure_field_class()
        payload_fc.append_field('zoom', self._tc.create_string_field_class())

        # Create event class
        ec1 = self._sc.create_event_class(24, 'jannine', bt2.EventClassLogLevel.EMERGENCY,
                                                'my/emf/uri', specific_context_field_class=context_fc,
                                                payload_field_class=payload_fc)
        self.assertEqual(ec1.id, 24)
        self.assertEqual(ec1.name, 'jannine')
        self.assertEqual(ec1.log_level, bt2.EventClassLogLevel.EMERGENCY)
        self.assertEqual(ec1.emf_uri, 'my/emf/uri')
        self.assertEqual(ec1.specific_context_field_class._ptr, context_fc._ptr)
        self.assertEqual(ec1.payload_field_class._ptr, payload_fc._ptr)

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(id=14, name=17)

    def test_assign_id(self):
        sc = self._tc.create_stream_class(id=1717)
        self.assertEqual(sc.id, 1717)

    def test_assign_invalid_id(self):
        with self.assertRaises(TypeError):
            self._tc.create_stream_class(id='lel')

    def test_default_clock_class(self):
        sc = self._tc.create_stream_class(id=1717, default_clock_class=self._cc)
        self.assertEqual(sc.default_clock_class.addr, self._cc.addr)

    def test_no_default_clock_class(self):
        sc = self._tc.create_stream_class(id=1717)
        self.assertIsNone(sc.default_clock_class)

    def test_clock_always_known(self):
        sc = self._tc.create_stream_class(id=1717, default_clock_class=self._cc)
        self.assertTrue(sc.default_clock_always_known)

    def test_create_no_packet_context_field_class(self):
        sc = self._tc.create_stream_class(id=1717)
        self.assertIsNone(sc.packet_context_field_class)

    def test_create_event_common_context_field_class(self):
        ft = self._tc.create_structure_field_class()
        ft.append_field(
            'champ', self._tc.create_unsigned_integer_field_class(19))
        sc = self._tc.create_stream_class(id=1717, event_common_context_field_class=ft)
        self.assertEqual(sc.event_common_context_field_class.addr, ft.addr)

    def test_assign_no_event_common_context_field_class(self):
        sc = self._tc.create_stream_class(id=1717)
        self.assertIsNone(sc.event_common_context_field_class)

    def test_trace_class_prop(self):
        sc = self._tc.create_stream_class(id=1717)
        self.assertEqual(sc.trace_class.addr, self._tc.addr)

    def test_getitem(self):
        self.assertEqual(self._sc[23]._ptr, self._ec1._ptr)
        self.assertEqual(self._sc[17]._ptr, self._ec2._ptr)

    def test_getitem_wrong_key_type(self):
        with self.assertRaises(TypeError):
            self._sc['event23']

    def test_getitem_wrong_key(self):
        with self.assertRaises(KeyError):
            self._sc[19]

    def test_len(self):
        self.assertEqual(len(self._sc), 2)

    def test_iter(self):
        for ec_id, event_class in self._sc.items():
            self.assertIsInstance(event_class, bt2.event_class._EventClass)

            if ec_id == 23:
                self.assertEqual(event_class._ptr, self._ec1._ptr)
            elif ec_id == 17:
                self.assertEqual(event_class._ptr, self._ec2._ptr)
