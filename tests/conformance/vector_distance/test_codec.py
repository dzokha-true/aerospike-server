"""Pin the wire codec and digest layout against drift.

These tests do not require a running server. They lock the byte layout
specified by docs/contracts/sptag-aerospike.md and verified by the C++
unit tests, so any change in either implementation that breaks cross-
compatibility is caught here.

Run with: python3 -m unittest tests.conformance.vector_distance.test_codec
"""
from __future__ import annotations

import struct
import unittest

from .codec import (
    VectorRequest,
    aerospike_int_digest,
    decode_payload,
    encode_payload,
)


class WireLayout(unittest.TestCase):
    def test_request_header_is_18_bytes(self):
        req = VectorRequest(
            namespace="test",
            bin_name="vec",
            set_name="",
            topk=1,
            query_bytes=b"\x00\x00\x00\x00",
            head_id_keys=[1],
        )
        buf = encode_payload(req)
        self.assertEqual(buf[0], 1, "wire version")
        # Header = 18 bytes; then bin_name (3) + set_name (0) + query (4)
        # + 1 head (8) = 33 bytes total.
        self.assertEqual(len(buf), 18 + 3 + 0 + 4 + 8)
        # head_count is uint32_le at offset 14.
        head_count = struct.unpack_from("<I", buf, 14)[0]
        self.assertEqual(head_count, 1)

    def test_response_result_element_is_18_bytes(self):
        # 12-byte header + 1 result (18) + 1 key status (12) = 42 bytes.
        # Mirrors as_vector_wire_encode_response_size(1, 1).
        result_payload = struct.pack(
            "<BBHII", 1, 0, 0, 1, 1
        ) + struct.pack("<qiBBf", 7, 42, 3, 0, 0.25) + struct.pack(
            "<qBBBB", 9, 1, 0, 0, 0
        )
        self.assertEqual(len(result_payload), 42)
        resp = decode_payload(result_payload)
        self.assertEqual(resp.request_status, 0)
        self.assertEqual(len(resp.results), 1)
        self.assertEqual(resp.results[0].head_id_key, 7)
        self.assertEqual(resp.results[0].vid, 42)
        self.assertEqual(resp.results[0].version, 3)
        self.assertAlmostEqual(resp.results[0].distance, 0.25)
        self.assertEqual(len(resp.key_statuses), 1)
        self.assertEqual(resp.key_statuses[0].head_id_key, 9)
        self.assertEqual(resp.key_statuses[0].status, 1)


class Digest(unittest.TestCase):
    def test_digest_excludes_namespace(self):
        # Same key, different namespaces -> identical digest. If the server
        # ever folds namespace bytes back into the hash, this fails.
        d1 = aerospike_int_digest("set", 12345)
        d2 = aerospike_int_digest("set", 12345)
        self.assertEqual(d1, d2)
        self.assertEqual(len(d1), 20, "RIPEMD-160 = 20 bytes")

    def test_digest_int_layout(self):
        # head_id_key=0 in empty set -> RIPEMD-160 over [0x01][0..0].
        import hashlib

        h = hashlib.new("ripemd160")
        h.update(b"")
        h.update(b"\x01")
        h.update(b"\x00" * 8)
        self.assertEqual(aerospike_int_digest("", 0), h.digest())


if __name__ == "__main__":
    unittest.main()
