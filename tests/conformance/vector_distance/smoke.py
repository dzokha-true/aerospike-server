"""EC528: live VECTOR_DISTANCE smoke against a running asd.

Pure stdlib. Writes its own posting fixtures over the wire (single-bin blob
puts), scores them via the VECTOR_DISTANCE batch op using codec.py, and
validates results against a local Python reference. Supports capturing a
results snapshot per SIMD mode and comparing two snapshots (scalar vs neon
wire equivalence).

Usage:
  python3 -m tests.conformance.vector_distance.smoke run \
      --host 127.0.0.1 --port 3000 --out /tmp/scalar.json
  python3 -m tests.conformance.vector_distance.smoke compare a.json b.json
"""
from __future__ import annotations

import argparse
import json
import random
import socket
import struct
import sys
import time

from . import codec

DIM = 64
SEED = 528
SET_NAME = "sptag"
BIN_NAME = "value"
NAMESPACE = "test"

HEAD_KEYS = [1, 2, 3, 4]
TAILS_PER_HEAD = 50
MISSING_KEY = 999
MALFORMED_KEY = 5

FIELD_SET = 1
FIELD_DIGEST = 4
INFO2_WRITE = 0x01
OP_WRITE = 2
PARTICLE_BLOB = 4

KEY_OK = 0
KEY_NOT_FOUND = 2
KEY_MALFORMED = 5

REL_TOL = 1e-5


# ---------------------------------------------------------------- wire I/O

def _send_msg(host: str, port: int, msg: bytes) -> bytes:
    with socket.create_connection((host, port), timeout=10) as s:
        s.sendall(msg)
        hdr = _recv_exact(s, 8)
        (proto,) = struct.unpack(">Q", hdr)
        sz = proto & ((1 << 48) - 1)
        return _recv_exact(s, sz)


def _recv_exact(s: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("short read")
        buf += chunk
    return buf


def _parse_as_msg(body: bytes):
    (header_sz, info1, info2, info3, info4, result, gen, ttl, tttl,
            n_fields, n_ops) = struct.unpack_from(">BBBBBBIIIHH", body, 0)
    off = header_sz
    fields = {}
    for _ in range(n_fields):
        (fsz,) = struct.unpack_from(">I", body, off)
        ftype = body[off + 4]
        fields[ftype] = body[off + 5:off + 4 + fsz]
        off += 4 + fsz
    return result, fields


def build_write_msg(namespace: str, set_name: str, key: int, bin_name: str,
        blob: bytes) -> bytes:
    """Standard single-bin blob write keyed by the int64 digest reference."""
    ns_b = namespace.encode()
    set_b = set_name.encode()
    digest = codec.aerospike_int_digest(set_name, key)
    bin_b = bin_name.encode()

    f_ns = struct.pack(">IB", len(ns_b) + 1, codec.FIELD_NAMESPACE) + ns_b
    f_set = struct.pack(">IB", len(set_b) + 1, FIELD_SET) + set_b
    f_dig = struct.pack(">IB", len(digest) + 1, FIELD_DIGEST) + digest
    fields = f_ns + f_set + f_dig

    op_sz = 4 + len(bin_b) + len(blob)
    op = struct.pack(">IBBBB", op_sz, OP_WRITE, PARTICLE_BLOB, 0,
            len(bin_b)) + bin_b + blob

    as_msg = struct.pack(">BBBBBBIIIHH", 22, 0, INFO2_WRITE, 0, 0, 0, 0, 0,
            1000, 3, 1)
    body = as_msg + fields + op
    proto = struct.pack(">Q", (2 << 56) | (3 << 48) | len(body))
    return proto + body


# ------------------------------------------------------------ fixtures

def make_fixtures():
    rng = random.Random(SEED)
    postings = {}
    vid = 0
    for head in HEAD_KEYS:
        elems = []
        for _ in range(TAILS_PER_HEAD):
            vec = [rng.uniform(-1.0, 1.0) for _ in range(DIM)]
            elems.append((vid, 1, vec))
            vid += 1
        postings[head] = elems
    query = [rng.uniform(-1.0, 1.0) for _ in range(DIM)]
    return postings, query


def posting_blob(elems) -> bytes:
    out = b""
    for vid, version, vec in elems:
        out += struct.pack("<i", vid) + bytes([version])
        out += struct.pack(f"<{DIM}f", *vec)
    return out


def f32(x: float) -> float:
    return struct.unpack("<f", struct.pack("<f", x))[0]


def ref_l2(q, v) -> float:
    # float32 arithmetic to mirror the server kernels closely enough for
    # REL_TOL comparison.
    q32 = [f32(x) for x in q]
    v32 = [f32(x) for x in v]
    acc = 0.0
    for a, b in zip(q32, v32):
        d = f32(a - b)
        acc = f32(acc + f32(d * d))
    return acc


def close(a: float, b: float) -> bool:
    return abs(a - b) <= REL_TOL * max(1.0, abs(a), abs(b))


# ------------------------------------------------------------ cases

def run(host: str, port: int, out_path: str) -> int:
    postings, query = make_fixtures()
    qbytes = struct.pack(f"<{DIM}f", *query)

    def write_with_retry(key, blob, what):
        # Right after startup the single node can briefly report partition
        # unavailable (result 11) before rebalance completes - retry.
        for attempt in range(20):
            body = _send_msg(host, port,
                    build_write_msg(NAMESPACE, SET_NAME, key, BIN_NAME, blob))
            result, _ = _parse_as_msg(body)
            if result == 0:
                return True
            if result != 11:
                print(f"FAIL: write {what} result {result}")
                return False
            time.sleep(0.5)
        print(f"FAIL: write {what} still unavailable after retries")
        return False

    # 1. Write fixtures (plus one malformed posting blob).
    for head, elems in postings.items():
        if not write_with_retry(head, posting_blob(elems), f"head {head}"):
            return 1
    # not a stride multiple:
    if not write_with_retry(MALFORMED_KEY, b"\x01\x02\x03", "malformed fixture"):
        return 1

    def vector_distance(keys, topk):
        req = codec.VectorRequest(NAMESPACE, BIN_NAME, SET_NAME, topk,
                qbytes, keys)
        body = _send_msg(host, port, codec.build_msg(req))
        result, fields = _parse_as_msg(body)
        if result != 0:
            raise RuntimeError(f"vector request result_code {result}")
        payload = fields[codec.FIELD_VECTOR_DISTANCE_RESPONSE]
        return codec.decode_payload(payload)

    failures = 0

    # 2. Scoring vs local reference (top-K across all listed heads).
    resp = vector_distance(HEAD_KEYS, 10)
    if resp.request_status != 0:
        print(f"FAIL: request_status {resp.request_status}")
        failures += 1
    expected = []
    for head, elems in postings.items():
        for vid, ver, vec in elems:
            expected.append((ref_l2(query, vec), vid, head, ver))
    expected.sort(key=lambda t: (t[0], t[1]))
    exp10 = expected[:10]

    if len(resp.results) != 10:
        print(f"FAIL: expected 10 results, got {len(resp.results)}")
        failures += 1
    got_sorted = sorted(resp.results, key=lambda r: (r.distance, r.vid))
    exp_vids = {e[1] for e in exp10}
    got_vids = {r.vid for r in got_sorted}
    # Distance-tolerant set comparison: ties may swap boundary members.
    for r in got_sorted:
        matches = [e for e in exp10 if e[1] == r.vid]
        if matches:
            if not close(matches[0][0], r.distance):
                print(f"FAIL: vid {r.vid} distance {r.distance} vs "
                        f"{matches[0][0]}")
                failures += 1
        else:
            # Allow only near-boundary substitutions.
            worst = exp10[-1][0]
            if not (r.distance <= worst or close(r.distance, worst)):
                print(f"FAIL: vid {r.vid} (d={r.distance}) not in expected "
                        f"top-10 (worst {worst})")
                failures += 1
    if len(exp_vids - got_vids) > 2:
        print(f"FAIL: top-10 overlap too low: missing {exp_vids - got_vids}")
        failures += 1

    # 3. Per-key statuses: missing key and malformed posting.
    resp2 = vector_distance([HEAD_KEYS[0], MISSING_KEY, MALFORMED_KEY], 5)
    st = {s.head_id_key: s.status for s in resp2.key_statuses}
    if st.get(MISSING_KEY) != KEY_NOT_FOUND:
        print(f"FAIL: missing-key status {st.get(MISSING_KEY)} != 2")
        failures += 1
    if st.get(MALFORMED_KEY) != KEY_MALFORMED:
        print(f"FAIL: malformed-key status {st.get(MALFORMED_KEY)} != 5")
        failures += 1
    if any(r.head_id_key not in (HEAD_KEYS[0],) for r in resp2.results):
        print("FAIL: results leaked from non-OK keys")
        failures += 1

    # 4. Top-K truncation.
    resp3 = vector_distance(HEAD_KEYS, 3)
    if len(resp3.results) != 3:
        print(f"FAIL: topk=3 returned {len(resp3.results)}")
        failures += 1

    snapshot = {
        "results": [
            {"head": r.head_id_key, "vid": r.vid, "version": r.version,
             "distance": r.distance}
            for r in sorted(resp.results, key=lambda r: (r.distance, r.vid))
        ],
        "statuses": sorted(
            [[s.head_id_key, s.status] for s in resp2.key_statuses]),
        "topk3": sorted(r.vid for r in resp3.results),
    }
    with open(out_path, "w") as f:
        json.dump(snapshot, f, indent=1)

    print(f"cases complete, failures={failures}, snapshot={out_path}")
    return 1 if failures else 0


def compare(path_a: str, path_b: str) -> int:
    with open(path_a) as f:
        a = json.load(f)
    with open(path_b) as f:
        b = json.load(f)

    failures = 0

    if a["statuses"] != b["statuses"]:
        print(f"FAIL: statuses differ: {a['statuses']} vs {b['statuses']}")
        failures += 1
    if a["topk3"] != b["topk3"]:
        print(f"FAIL: topk3 sets differ: {a['topk3']} vs {b['topk3']}")
        failures += 1

    avids = {r["vid"]: r for r in a["results"]}
    bvids = {r["vid"]: r for r in b["results"]}
    if set(avids) != set(bvids):
        print(f"FAIL: result sets differ: {sorted(avids)} vs "
                f"{sorted(bvids)}")
        failures += 1
    else:
        for vid, ra in avids.items():
            rb = bvids[vid]
            if not close(ra["distance"], rb["distance"]):
                print(f"FAIL: vid {vid} distance {ra['distance']} vs "
                        f"{rb['distance']}")
                failures += 1
            if ra["version"] != rb["version"] or ra["head"] != rb["head"]:
                print(f"FAIL: vid {vid} metadata differs")
                failures += 1

    print("compare:", "FAIL" if failures else "OK")
    return 1 if failures else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    runp = sub.add_parser("run")
    runp.add_argument("--host", default="127.0.0.1")
    runp.add_argument("--port", type=int, default=3000)
    runp.add_argument("--out", required=True)
    cmpp = sub.add_parser("compare")
    cmpp.add_argument("a")
    cmpp.add_argument("b")
    args = ap.parse_args()

    if args.cmd == "run":
        return run(args.host, args.port, args.out)
    return compare(args.a, args.b)


if __name__ == "__main__":
    sys.exit(main())
