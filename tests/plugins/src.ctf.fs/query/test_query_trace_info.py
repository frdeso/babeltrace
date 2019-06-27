# Copyright (C) 2019 Simon Marchi <simon.marchi@efficios.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; only version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

import unittest
import bt2
import os
import re


test_ctf_traces_path = os.environ['BT_CTF_TRACES_PATH']


# Key to sort streams in a predictable order.
def sort_predictably(stream):
    if 'range-ns' in stream:
        return stream['range-ns']['begin']
    else:
        return stream['paths'][0]


class QueryTraceInfoClockOffsetTestCase(unittest.TestCase):

    def setUp(self):
        ctf = bt2.find_plugin('ctf')
        self._fs = ctf.source_component_classes['fs']

        self._paths = [os.path.join(test_ctf_traces_path, 'intersection', '3eventsintersect')]
        self._executor = bt2.QueryExecutor()

    def _check(self, trace, offset):
        self.assertEqual(trace['range-ns']['begin'], 13515309000000000 + offset)
        self.assertEqual(trace['range-ns']['end'], 13515309000000120 + offset)
        self.assertEqual(trace['intersection-range-ns']['begin'], 13515309000000070 + offset)
        self.assertEqual(trace['intersection-range-ns']['end'], 13515309000000100 + offset)

        streams = sorted(trace['streams'], key=sort_predictably)
        self.assertEqual(streams[0]['range-ns']['begin'], 13515309000000000 + offset)
        self.assertEqual(streams[0]['range-ns']['end'], 13515309000000100 + offset)
        self.assertEqual(streams[1]['range-ns']['begin'], 13515309000000070 + offset)
        self.assertEqual(streams[1]['range-ns']['end'], 13515309000000120 + offset)

    # Test various cominations of the clock-class-offset-s and
    # clock-class-offset-ns parameters to trace-info queries.

    # Without clock class offset

    def test_no_clock_class_offset(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
        })
        trace = res[0]
        self._check(trace, 0)

    # With clock-class-offset-s

    def test_clock_class_offset_s(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
            'clock-class-offset-s': 2,
        })
        trace = res[0]
        self._check(trace, 2000000000)

    # With clock-class-offset-ns

    def test_clock_class_offset_ns(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
            'clock-class-offset-ns': 2,
        })
        trace = res[0]
        self._check(trace, 2)

    # With both, negative

    def test_clock_class_offset_both(self):
        res = self._executor.query(self._fs, 'trace-info', {
            'paths': self._paths,
            'clock-class-offset-s': -2,
            'clock-class-offset-ns': -2,
        })
        trace = res[0]
        self._check(trace, -2000000002)

    def test_clock_class_offset_s_wrong_type(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-s': "2",
            })

    def test_clock_class_offset_s_wrong_type_none(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-s': None,
            })

    def test_clock_class_offset_ns_wrong_type(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-ns': "2",
            })

    def test_clock_class_offset_ns_wrong_type_none(self):
        with self.assertRaises(bt2.InvalidQueryParams):
            self._executor.query(self._fs, 'trace-info', {
                'paths': self._paths,
                'clock-class-offset-ns': None,
            })


class QueryTraceInfoPortNameTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin("ctf")
        self._fs = ctf.source_component_classes["fs"]

        self._executor = bt2.QueryExecutor()

    def test_trace_uuid_stream_class_id_no_stream_id(self):
        res = self._executor.query(
            self._fs,
            "trace-info",
            {
                "paths": [
                    os.path.join(
                        test_ctf_traces_path, "intersection", "3eventsintersect"
                    )
                ]
            },
        )
        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = sorted(trace["streams"], key=sort_predictably)
        self.assertEqual(len(streams), 2)
        self.assertRegexpMatches(
            str(streams[0]["port-name"]),
            r"^7afe8fbe-79b8-4f6a-bbc7-d0c782e7ddaf \| 0 \| .*/tests/data/ctf-traces/intersection/3eventsintersect/test_stream_0$",
        )
        self.assertRegexpMatches(
            str(streams[1]["port-name"]),
            r"^7afe8fbe-79b8-4f6a-bbc7-d0c782e7ddaf \| 0 \| .*/tests/data/ctf-traces/intersection/3eventsintersect/test_stream_1$",
        )

    def test_trace_uuid_no_stream_class_id_no_stream_id(self):
        res = self._executor.query(
            self._fs,
            "trace-info",
            {"paths": [os.path.join(test_ctf_traces_path, "succeed", "succeed1")]},
        )
        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = sorted(trace["streams"], key=sort_predictably)
        self.assertEqual(len(streams), 1)
        self.assertRegexpMatches(
            str(streams[0]["port-name"]),
            r"^2a6422d0-6cee-11e0-8c08-cb07d7b3a564 \| .*/tests/data/ctf-traces/succeed/succeed1/dummystream$",
        )


class QueryTraceInfoRangeTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin("ctf")
        self._fs = ctf.source_component_classes["fs"]

        self._executor = bt2.QueryExecutor()

    def test_trace_no_range(self):
        # This trace has no `timestamp_begin` and `timestamp_end` in its packet
        # context. The `trace-info` query should omit the `range-ns` fields in
        # the `trace` and `stream` data structures.

        res = self._executor.query(
            self._fs,
            "trace-info",
            {"paths": [os.path.join(test_ctf_traces_path, "succeed", "succeed1")]},
        )

        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = trace["streams"]
        self.assertEqual(len(streams), 1)

        self.assertRaises(KeyError, lambda: trace['range-ns'])
        self.assertRaises(KeyError, lambda: streams[0]['range-ns'])


class QueryTraceInfoPacketTimestampQuirksTestCase(unittest.TestCase):
    def setUp(self):
        ctf = bt2.find_plugin('ctf')
        self._fs = ctf.source_component_classes['fs']
        self._path = os.path.join(test_ctf_traces_path, 'succeed')

        self._executor = bt2.QueryExecutor()

    def _test_lttng_quirks(self, path):
        # Reuse code, for test traces sharing the same base trace and having
        # the same work around.
        res = self._executor.query(
            self._fs,
            "trace-info",
            {"paths": [path]},
        )

        self.assertEqual(len(res), 1)
        trace = res[0]
        streams = trace["streams"]
        self.assertEqual(len(streams), 1)

        self.assertEqual(streams[0]['range-ns']['begin'], 1561489497322777352)
        self.assertEqual(streams[0]['range-ns']['end'], 1561489497327275498)

    def test_event_after_packet(self):
        self._test_lttng_quirks(os.path.join(self._path, "lttng-event-after-packet"))

    def test_lttng_crash(self):
        self._test_lttng_quirks(os.path.join(self._path, "lttng-crash"))


if __name__ == '__main__':
    unittest.main()
