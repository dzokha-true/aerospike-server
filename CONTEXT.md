# SPTAG + Aerospike

SPTAG is the smart client that owns ANN search state and chooses candidate heads. This Aerospike fork is the durable posting storage backend for SPTAG `AEROSPIKEIO`, and computes distances for listed posting records on the nodes that own those records.

## Language

**Smart Client**: SPTAG process that owns query orchestration, head graph traversal, candidate Head ID selection, and global result merge.

**Head Graph**: SPTAG in-memory ANN graph used for coarse search. Aerospike does not store or traverse it.

**Head ID**: SPTAG candidate identifier used as the Aerospike record key for one posting list. On the wire for `VECTOR_DISTANCE`, this is **`head_id_key`**: an Aerospike integer record key encoded as `int64_le`. Valid values for SPTAG compatibility in Phase 3 are `0..INT32_MAX`.

**Posting List**: Whole Aerospike value blob for one Head ID.

**Posting Element**: One vector-info unit inside a Posting List. It includes a vector ID, a version byte, and one typed vector payload.

**Tail Vector**: Embedding payload inside a Posting Element. This is the distance target and excludes the vector ID and version metadata.

**VID**: SPTAG vector ID stored inside a Posting Element as `int32_le`. Valid range in Phase 3 is `0..INT32_MAX`.

**AEROSPIKEIO**: SPTAG storage mode where posting lists are stored in Aerospike while graph files and graph traversal remain in SPTAG.

**ComputeDistance**: SPTAG distance output used on Tail Vector bytes. Aerospike Phase 3 mirrors SPTAG `ComputeDistance` semantics exactly. Returned scores sort **smaller-is-better** for all metrics, including `inner-product`.

**Owner-Local Top K**: Per owner-node `VECTOR_DISTANCE` response: the best K Posting Elements across all listed local Head IDs in one request, after server-side dedupe by VID.

**Partition-Local Distance**: Key-scoped distance work that runs on the Aerospike node that owns the listed Head ID records. It is not a partition scan.

**ValueType**: Vector element type for Tail Vector and query payloads, configured as `float`, `uint8`, `int8`, or `int16`.

**Dimension**: Number of elements in each Tail Vector. The server-side distance contract must match the SPTAG index dimension.

**DistMethod**: Distance metric configured as `l2`, `cosine`, or `inner-product`. Posting bytes do not imply the metric.

## Relationships

- One **Head ID** maps to one Aerospike record containing one **Posting List**.
- One **Posting List** contains zero or more **Posting Elements**.
- One **Posting Element** contains one **Tail Vector** payload.
- **Head Graph** search happens only in the **Smart Client**.
- **Partition-Local Distance** touches only listed Head ID records on their owner nodes.
- **Owner-Local Top K** is partial; **Smart Client** performs global top-K merge across owner responses.

## Flagged Ambiguities

(none)
