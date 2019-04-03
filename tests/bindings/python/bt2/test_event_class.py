import unittest
import bt2
from test_utils.test_utils import run_in_component_init


class EventClassTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            return comp_self._create_trace_class()

        self._tc = run_in_component_init(f)
        self._context_ft = self._tc.create_structure_field_class()
        self._context_ft.append_field('allo', self._tc.create_string_field_class())
        self._context_ft.append_field('zola', self._tc.create_signed_integer_field_class(18))
        self._payload_ft = self._tc.create_structure_field_class()
        self._payload_ft.append_field('zoom', self._tc.create_string_field_class())
        if self._payload_ft == None:
            print('allo')
        #trace = tc()
        self._stream_class = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        self._ec = self._stream_class.create_event_class(id=18, name='my_event',
                                                         log_level=bt2.EventClassLogLevel.INFO,
                                                         emf_uri='yes',
                                                         specific_context_field_class=self._context_ft,
                                                         payload_field_class=self._payload_ft)


    def tearDown(self):
        del self._context_ft
        del self._payload_ft
        del self._ec

    def test_create(self):
        self.assertEqual(self._ec.name, 'my_event')
        self.assertEqual(self._ec.id, 18)
        self.assertEqual(self._ec.specific_context_field_class.addr, self._context_ft.addr)
        self.assertEqual(self._ec.payload_field_class.addr, self._payload_ft.addr)
        self.assertEqual(self._ec.emf_uri, 'yes')
        self.assertEqual(self._ec.log_level, bt2.EventClassLogLevel.INFO)

    def test_specific_context_field_class(self):
        ec = self._stream_class.create_event_class(id=12)
        self.assertIsNone(ec.specific_context_field_class)

        ft = self._tc.create_structure_field_class()
        ft.append_field('garou', self._tc.create_string_field_class())
        ec = self._stream_class.create_event_class(id=13, specific_context_field_class=ft)
        self.assertEqual(ec.specific_context_field_class.addr, ft.addr)

    def test_create_invalid_context_field_class(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(id=12, specific_context_field_class = 'lel')

    def test_payload_field_class(self):
        ec = self._stream_class.create_event_class(id=12)
        self.assertIsNone(ec.payload_field_class)

        ft = self._tc.create_structure_field_class()
        ft.append_field('garou', self._tc.create_string_field_class())
        ec = self._stream_class.create_event_class(id=13, payload_field_class=ft)
        self.assertEqual(ec.payload_field_class.addr, ft.addr)

    def test_create_invalid_payload_field_class(self):
        with self.assertRaises(TypeError):
            ec = self._stream_class.create_event_class(id=12, payload_field_class='lel')

    def test_stream_class_prop(self):
        self.assertEqual(self._ec.stream_class.addr, self._stream_class.addr)

    def test_emf_uri(self):
        ec = self._stream_class.create_event_class(id=111)
        self.assertEqual(ec.emf_uri, None)

        ec = self._stream_class.create_event_class(id=222, emf_uri='salut')
        self.assertEqual(ec.emf_uri, 'salut')

    def test_create_invalid_emf_uri(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(id=222, emf_uri=23)

    def test_log_level(self):
        ec = self._stream_class.create_event_class(id=111)
        self.assertEqual(ec.log_level, None)

        ec = self._stream_class.create_event_class(id=222, log_level=bt2.EventClassLogLevel.EMERGENCY)
        self.assertEqual(ec.log_level, bt2.EventClassLogLevel.EMERGENCY)

    def test_create_invalid_log_level(self):
        with self.assertRaises(ValueError):
            self._stream_class.create_event_class(id=123, log_level='zoom')
