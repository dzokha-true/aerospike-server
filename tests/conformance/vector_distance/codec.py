"""EC528: VECTOR_DISTANCE wire codec and Aerospike digest reference.

Used by conformance harness to drive a running asd over a raw socket and to
pin the on-the-wire byte layout that the SPTAG client must produce.
"""
from __future__ import annotations

import hashlib
import struct
from dataclasses import dataclass, field
from typing import List, Sequence

WIRE_VERSION = 1

FIELD_NAMESPACE = 0
FIELD_VECTOR_DISTANCE = 44
FIELD_VECTOR_DISTANCE_RESPONSE = 45

INFO1_BATCH = 0x08
INFO3_LAST = 0x01

PARTICLE_TYPE_INTEGER = 1


def aerospike_int_digest(set_name: str, key: int) -> bytes:
    """Reference implementation of the Aerospike client digest for an int64
    key. Server-side `as_vector_digest_compute` must match this exactly.
    """
    h = hashlib.new("ripemd160")
    h.update(set_name.encode("utf-8"))
    h.update(bytes([PARTICLE_TYPE_INTEGER]))
    h.update(struct.pack(">q", key))
    return h.digest()


@dataclass
class VectorRequest:
    namespace: str
    bin_name: str
    set_name: str
    topk: int
    query_bytes: bytes
    head_id_keys: Sequence[int]


@dataclass
class ScoredTail:
    head_id_key: int
    vid: int
    version: int
    distance: float


@dataclass
class KeyStatus:
    head_id_key: int
    status: int


@dataclass
class VectorResponse:
    request_status: int
    results: List[ScoredTail] = field(default_factory=list)
    key_statuses: List[KeyStatus] = field(default_factory=list)


def encode_payload(req: VectorRequest) -> bytes:
    bin_b = req.bin_name.encode("utf-8")
    set_b = req.set_name.encode("utf-8")
    header = struct.pack(
        "<BBHIHHHI",
        WIRE_VERSION,
        0,
        0,
        req.topk,
        len(bin_b),
        len(set_b),
        len(req.query_bytes),
        len(req.head_id_keys),
    )
    body = bin_b + set_b + req.query_bytes
    body += b"".join(struct.pack("<q", k) for k in req.head_id_keys)
    return header + body


def decode_payload(buf: bytes) -> VectorResponse:
    if len(buf) < 12:
        raise ValueError("response too short")
    version, status, _r, result_count, status_count = struct.unpack_from(
        "<BBHII", buf, 0
    )
    if version != WIRE_VERSION:
        raise ValueError(f"unsupported response version {version}")
    off = 12
    resp = VectorResponse(request_status=status)
    for _ in range(result_count):
        head, vid, ver, _pad, dist = struct.unpack_from("<qiBBf", buf, off)
        # struct '<qiBBf' = 8 + 4 + 1 + 1 + 4 = 18 bytes
        resp.results.append(ScoredTail(head, vid, ver, dist))
        off += 18
    for _ in range(status_count):
        head, st = struct.unpack_from("<qB", buf, off)
        resp.key_statuses.append(KeyStatus(head, st))
        off += 12
    return resp


def build_msg(req: VectorRequest) -> bytes:
    """Wrap an EC528 vector-distance payload in an Aerospike proto/as_msg."""
    payload = encode_payload(req)
    ns_b = req.namespace.encode("utf-8")

    # Two msg fields: namespace + vector-distance.
    field_ns = struct.pack(">IB", len(ns_b) + 1, FIELD_NAMESPACE) + ns_b
    field_vd = (
        struct.pack(">IB", len(payload) + 1, FIELD_VECTOR_DISTANCE) + payload
    )
    fields = field_ns + field_vd

    # as_msg header: header_sz, info1, info2, info3, info4, result_code,
    # generation, record_ttl, transaction_ttl, n_fields, n_ops.
    as_msg = struct.pack(
        ">BBBBBBIIIHH",
        22,  # header_sz
        INFO1_BATCH,
        0,
        INFO3_LAST,
        0,
        0,
        0,
        0,
        0,
        2,  # n_fields
        0,
    )

    body = as_msg + fields
    proto = struct.pack(">BB", 2, 3) + struct.pack(">H", 0) + struct.pack(
        ">I", len(body)
    )
    # proto.sz is the low 48 bits in a 64-bit field; pack accordingly:
    proto = struct.pack(">Q", (2 << 56) | (3 << 48) | len(body))
    return proto + body
