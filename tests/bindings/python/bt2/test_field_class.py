import bt2.field
import unittest
import bt2
from test_utils.test_utils import run_in_component_init


def get_dummy_trace_class():
    def f(comp_self):
        return comp_self._create_trace_class()

    return run_in_component_init(f)


class _TestIntegerFieldTypeProps:
    def test_range(self):
        fc = self._create_func()
        self.assertEqual(fc.range, 64)

        fc = self._create_func(range=35)
        self.assertEqual(fc.range, 35)

        fc = self._create_func(36)
        self.assertEqual(fc.range, 36)

    def test_create_invalid_range(self):
        with self.assertRaises(TypeError):
            self._create_func('yes')

        with self.assertRaises(TypeError):
            self._create_func(range='yes')

        with self.assertRaises(ValueError):
            self._create_func(range=-2)

        with self.assertRaises(ValueError):
            self._create_func(range=0)

    def test_base(self):
        fc = self._create_func()
        self.assertEqual(fc.display_base, bt2.IntegerDisplayBase.DECIMAL)

        fc = self._create_func(display_base=bt2.IntegerDisplayBase.HEXADECIMAL)
        self.assertEqual(fc.display_base, bt2.IntegerDisplayBase.HEXADECIMAL)

    def test_create_invalid_base(self):
        with self.assertRaises(TypeError):
            self._create_func(display_base='yes')

    def test_create_full(self):
        fc = self._create_func(24, display_base=bt2.IntegerDisplayBase.OCTAL)
        self.assertEqual(fc.range, 24)
        self.assertEqual(fc.display_base, bt2.IntegerDisplayBase.OCTAL)


class IntegerFieldTypeTestCase(_TestIntegerFieldTypeProps, unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()
        self._create_func = self._tc.create_signed_integer_field_class


class RealFieldTypeTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()
        self._fc = self._tc.create_real_field_class()

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        pass

    def test_create_full(self):
        fc = self._tc.create_real_field_class(is_single_precision=True)
        self.assertTrue(fc.single_precision)


class EnumerationFieldTypeTestCase(_TestIntegerFieldTypeProps, unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()
        self._fc = self._tc.create_signed_enumeration_field_class(range=35)
        self._unsigned_fc = self._tc.create_unsigned_enumeration_field_class(range=35)
        self._create_func = self._tc.create_signed_enumeration_field_class

    def tearDown(self):
        del self._fc
        del self._unsigned_fc

    def test_create_from_invalid_type(self):
        with self.assertRaises(TypeError):
            self._tc.create_signed_enumeration_field_class('coucou')

    def test_create_from_invalid_fc(self):
        with self.assertRaises(TypeError):
            fc = self._tc.create_real_field_class()
            self._tc.create_signed_enumeration_field_class(fc)

    def test_add_mapping_simple(self):
        self._fc.map_range('hello', 24)
        mapping = self._fc['hello']
        first_range = next(iter(mapping))
        self.assertEqual(mapping.label, 'hello')
        self.assertEqual(first_range.lower, 24)
        self.assertEqual(first_range.upper, 24)

    def test_add_mapping_simple_kwargs(self):
        self._fc.map_range(label='hello', lower=17, upper=23)
        mapping = self._fc['hello']
        first_range = next(iter(mapping))
        self.assertEqual(mapping.label, 'hello')
        self.assertEqual(first_range.lower, 17)
        self.assertEqual(first_range.upper, 23)

    def test_add_mapping_range(self):
        self._fc.map_range('hello', 21, 199)
        mapping = self._fc['hello']
        first_range = next(iter(mapping))
        self.assertEqual(mapping.label, 'hello')
        self.assertEqual(first_range.lower, 21)
        self.assertEqual(first_range.upper, 199)

    def test_add_mapping_invalid_name(self):
        with self.assertRaises(TypeError):
            self._fc.map_range(17, 21, 199)

    def test_add_mapping_invalid_signedness_lower(self):
        with self.assertRaises(ValueError):
            self._unsigned_fc.map_range('hello', -21, 199)

    def test_add_mapping_invalid_signedness_upper(self):
        with self.assertRaises(ValueError):
            self._unsigned_fc.map_range('hello', 21, -199)

    def test_add_mapping_simple_signed(self):
        self._fc.map_range('hello', -24)
        mapping = self._fc['hello']
        first_range = next(iter(mapping))
        self.assertEqual(mapping.label, 'hello')
        self.assertEqual(first_range.lower, -24)
        self.assertEqual(first_range.upper, -24)

    def test_add_mapping_range_signed(self):
        self._fc.map_range('hello', -21, 199)
        mapping = self._fc['hello']
        first_range = next(iter(mapping))
        self.assertEqual(mapping.label, 'hello')
        self.assertEqual(first_range.lower, -21)
        self.assertEqual(first_range.upper, 199)

    def test_iadd(self):
        enum_fc = self._tc.create_signed_enumeration_field_class(range=16)
        enum_fc.map_range('c', 4, 5)
        enum_fc.map_range('d', 6, 18)
        enum_fc.map_range('e', 20, 27)
        self._fc.map_range('a', 0, 2)
        self._fc.map_range('b', 3)
        self._fc += enum_fc

        self.assertEqual(self._fc['a'].label, 'a')
        first_range = next(iter(self._fc['a']))
        self.assertEqual(first_range.lower, 0)
        self.assertEqual(first_range.upper, 2)

        self.assertEqual(self._fc['b'].label, 'b')
        first_range = next(iter(self._fc['b']))
        self.assertEqual(first_range.lower, 3)
        self.assertEqual(first_range.upper, 3)

        self.assertEqual(self._fc['c'].label, 'c')
        first_range = next(iter(self._fc['c']))
        self.assertEqual(first_range.lower, 4)
        self.assertEqual(first_range.upper, 5)

        self.assertEqual(self._fc['d'].label, 'd')
        first_range = next(iter(self._fc['d']))
        self.assertEqual(first_range.lower, 6)
        self.assertEqual(first_range.upper, 18)

        self.assertEqual(self._fc['e'].label, 'e')
        first_range = next(iter(self._fc['e']))
        self.assertEqual(first_range.lower, 20)
        self.assertEqual(first_range.upper, 27)

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._fc.map_range('a', 0)
        self.assertTrue(self._fc)

    def test_len(self):
        self._fc.map_range('a', 0)
        self._fc.map_range('b', 1)
        self._fc.map_range('c', 2)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        self._fc.map_range('a', 0)
        self._fc.map_range('b', 1, 3)
        self._fc.map_range('c', 5)
        mapping = self._fc['b']
        self.assertEqual(mapping.label, 'b')
        first_range = next(iter(mapping))
        self.assertEqual(first_range.lower, 1)
        self.assertEqual(first_range.upper, 3)

    def test_contains(self):
        self._fc.map_range('a', 0)
        self._fc.map_range('a', 2, 23)
        self._fc.map_range('b', 2)
        self._fc.map_range('c', 5)

        a_mapping = self._fc['a']
        b_mapping = self._fc['b']
        first_range = next(iter(a_mapping))

        self.assertIn(first_range, a_mapping)
        self.assertNotIn(first_range, b_mapping)

    def test_iter(self):
        mappings = (
            ('a', 1, 5),
            ('b', 10, 17),
            ('c', 20, 1504),
            ('d', 22510, 99999),
        )

        for mapping in mappings:
            self._fc.map_range(*mapping)

        for ft_mapping, mapping in zip(self._fc, mappings):
            self.assertEqual(ft_mapping.label, mapping[0])
            first_range = next(iter(ft_mapping))
            self.assertEqual(first_range.lower, mapping[1])
            self.assertEqual(first_range.upper, mapping[2])

    def _test_find_by_name(self, ft):
        ft.map_range('a', 0)
        ft.map_range('b', 1, 3)
        ft.map_range('a', 5)
        ft.map_range('a', 17, 123)
        ft.map_range('C', 5)
        mapping = ft['a']
        a0 = False
        a5 = False
        a17_123 = False

        self.assertEqual(mapping.label, 'a')

        for range in mapping:
            if range.lower == 0 and range.upper == 0:
                a0 = True
            elif range.lower == 5 and range.upper == 5:
                a5 = True
            elif range.lower == 17 and range.upper == 123:
                a17_123 = True

        self.assertEqual(len(mapping), 3)
        self.assertTrue(a0)
        self.assertTrue(a5)
        self.assertTrue(a17_123)

    def test_find_by_name_signed(self):
        self._test_find_by_name(self._tc.create_signed_enumeration_field_class(range=8))

    def test_find_by_name_unsigned(self):
        self._test_find_by_name(self._tc.create_unsigned_enumeration_field_class(range=8))

    def _test_find_by_value(self, ft):
        ft.map_range('a', 0)
        ft.map_range('b', 1, 3)
        ft.map_range('c', 5, 19)
        ft.map_range('d', 8, 15)
        ft.map_range('e', 10, 21)
        ft.map_range('f', 0)
        ft.map_range('g', 14)

        labels = ft.labels_by_value(14)

        expected_labels = ['c', 'd', 'e', 'g']

        self.assertTrue(all(label in labels for label in expected_labels))

    def test_find_by_value_signed(self):
        self._test_find_by_value(self._tc.create_signed_enumeration_field_class(range=8))

    def test_find_by_value_unsigned(self):
        self._test_find_by_value(self._tc.create_unsigned_enumeration_field_class(range=8))


class StringFieldTypeTestCase(unittest.TestCase):
    def setUp(self):
        tc = get_dummy_trace_class()
        self._fc = tc.create_string_field_class()

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        self.assertIsNotNone(self._fc)


class _TestFieldContainer():
    def test_append_field(self):
        int_field_class = self._tc.create_signed_integer_field_class(32)
        self._fc.append_field('int32', int_field_class)
        field_class = self._fc['int32']
        self.assertEqual(field_class.addr, int_field_class.addr)

    def test_append_field_kwargs(self):
        int_field_class = self._tc.create_signed_integer_field_class(32)
        self._fc.append_field(name='int32', field_class=int_field_class)
        field_class = self._fc['int32']
        self.assertEqual(field_class.addr, int_field_class.addr)

    def test_append_field_invalid_name(self):
        with self.assertRaises(TypeError):
            self._fc.append_field(23, self._tc.create_string_field_class())

    def test_append_field_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._fc.append_field('yes', object())

    def test_iadd(self):
        struct_fc = self._tc.create_structure_field_class()
        c_field_class = self._tc.create_string_field_class()
        d_field_class = self._tc.create_signed_enumeration_field_class(range=32)
        e_field_class = self._tc.create_structure_field_class()
        struct_fc.append_field('c_string', c_field_class)
        struct_fc.append_field('d_enum', d_field_class)
        struct_fc.append_field('e_struct', e_field_class)
        a_field_class = self._tc.create_real_field_class()
        b_field_class = self._tc.create_signed_integer_field_class(17)
        self._fc.append_field('a_float', a_field_class)
        self._fc.append_field('b_int', b_field_class)
        self._fc += struct_fc
        self.assertEqual(self._fc['a_float'].addr, a_field_class.addr)
        self.assertEqual(self._fc['b_int'].addr, b_field_class.addr)
        self.assertEqual(self._fc['c_string'].addr, c_field_class.addr)
        self.assertEqual(self._fc['d_enum'].addr, d_field_class.addr)
        self.assertEqual(self._fc['e_struct'].addr, e_field_class.addr)

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._fc.append_field('a', self._tc.create_string_field_class())
        self.assertTrue(self._fc)

    def test_len(self):
        fc = self._tc.create_string_field_class()
        self._fc.append_field('a', fc)
        self._fc.append_field('b', fc)
        self._fc.append_field('c', fc)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._fc.append_field('a', a_fc)
        self._fc.append_field('b', b_fc)
        self._fc.append_field('c', c_fc)
        self.assertEqual(self._fc['b'].addr, b_fc.addr)

    def test_getitem_invalid_key_type(self):
        with self.assertRaises(TypeError):
            self._fc[0]

    def test_getitem_invalid_key(self):
        with self.assertRaises(KeyError):
            self._fc['no way']

    def test_contains(self):
        self.assertFalse('a' in self._fc)
        self._fc.append_field('a', self._tc.create_string_field_class())
        self.assertTrue('a' in self._fc)

    def test_iter(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        fields = (
            ('a', a_fc),
            ('b', b_fc),
            ('c', c_fc),
        )

        for field in fields:
            self._fc.append_field(*field)

        for (name, ft_field_class), field in zip(self._fc.items(), fields):
            self.assertEqual(name, field[0])
            self.assertEqual(ft_field_class.addr, field[1].addr)

    def test_at_index(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._fc.append_field('c', c_fc)
        self._fc.append_field('a', a_fc)
        self._fc.append_field('b', b_fc)
        self.assertEqual(self._fc.at_index(1).addr, a_fc.addr)

    def test_at_index_invalid(self):
        self._fc.append_field('c', self._tc.create_signed_integer_field_class(32))

        with self.assertRaises(TypeError):
            self._fc.at_index('yes')

    def test_at_index_out_of_bounds_after(self):
        self._fc.append_field('c', self._tc.create_signed_integer_field_class(32))

        with self.assertRaises(IndexError):
            self._fc.at_index(len(self._fc))


class StructureFieldTypeTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()
        self._fc = self._tc.create_structure_field_class()

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        self.assertIsNotNone(self._fc)


class VariantFieldTypeTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()
        selector_fc = self._tc.create_unsigned_enumeration_field_class(range=42)
        selector_fc.map_range('first', 1)
        selector_fc.map_range('second', 2, 434)

        self._fc = self._tc.create_variant_field_class(selector_fc)

    def tearDown(self):
        del self._fc

    def test_selector_field_path(self):
        selector_fc = self._tc.create_unsigned_enumeration_field_class(range=42)
        selector_fc.map_range('first', 1)
        selector_fc.map_range('second', 2, 434)

        fc = self._tc.create_variant_field_class(selector_fc)
        fc.append_field('a', self._tc.create_real_field_class())
        fc.append_field('b', self._tc.create_signed_integer_field_class(21))
        fc.append_field('c', self._tc.create_unsigned_integer_field_class(34))

        foo_fc = self._tc.create_real_field_class()
        bar_fc = self._tc.create_string_field_class()
        baz_fc = self._tc.create_string_field_class()

        inner_struct_fc = self._tc.create_structure_field_class()
        inner_struct_fc.append_field('selector', selector_fc)
        inner_struct_fc.append_field('bar', bar_fc)
        inner_struct_fc.append_field('baz', baz_fc)
        inner_struct_fc.append_field('variant', fc)

        outer_struct_fc = self._tc.create_structure_field_class()
        outer_struct_fc.append_field('foo', foo_fc)
        outer_struct_fc.append_field('inner_struct', inner_struct_fc)

        # The path to the selector field is resolved when the sequence is
        # actually used, for example in a packet context.
        self._tc.create_stream_class(packet_context_field_class=outer_struct_fc)

        self.assertEqual(list(fc.selector_field_path), [1, 0])


class StaticArrayFieldTypeTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()
        self._elem_fc = self._tc.create_signed_integer_field_class(23)
        self._fc = self._tc.create_static_array_field_class(self._elem_fc, 45)

    def tearDown(self):
        del self._fc
        del self._elem_fc

    def test_create_default(self):
        self.assertEqual(self._fc.element_field_class.addr, self._elem_fc.addr)
        self.assertEqual(self._fc.length, 45)

    def test_create_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_static_array_field_class(object(), 45)

    def test_create_invalid_length(self):
        with self.assertRaises(ValueError):
            self._tc.create_static_array_field_class(self._tc.create_string_field_class(), -17)

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._tc.create_static_array_field_class(self._tc.create_string_field_class(), 'the length')


class DynamicArrayFieldTypeTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_dummy_trace_class()
        self._elem_fc = self._tc.create_signed_integer_field_class(23)
        self._len_fc = self._tc.create_unsigned_integer_field_class(12)
        self._fc = self._tc.create_dynamic_array_field_class(self._elem_fc, self._len_fc)

    def test_create_default(self):
        self.assertEqual(self._fc.element_field_class.addr, self._elem_fc.addr)

    def test_field_path(self):
        foo_fc = self._tc.create_real_field_class()
        bar_fc = self._tc.create_string_field_class()
        baz_fc = self._tc.create_string_field_class()

        inner_struct_fc = self._tc.create_structure_field_class()
        inner_struct_fc.append_field('bar', bar_fc)
        inner_struct_fc.append_field('baz', baz_fc)
        inner_struct_fc.append_field('len', self._len_fc)
        inner_struct_fc.append_field('dyn_array', self._fc)

        outer_struct_fc = self._tc.create_structure_field_class()
        outer_struct_fc.append_field('foo', foo_fc)
        outer_struct_fc.append_field('inner_struct', inner_struct_fc)

        # The path to the length field is resolved when the sequence is
        # actually used, for example in a packet context.
        self._tc.create_stream_class(packet_context_field_class=outer_struct_fc)

        self.assertEqual(list(self._fc.length_field_path), [1, 2])

    def test_create_invalid_field_class(self):
        len_fc = self._tc.create_unsigned_integer_field_class(12)
        with self.assertRaises(TypeError):
            self._tc.create_dynamic_array_field_class(object(), len_fc)

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._tc.create_dynamic_array_field_class(self._tc.create_string_field_class(), 17)


if __name__ == "__main__":
    unittest.main()
