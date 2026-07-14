# EC528 vector-server images

Build (from the repo root; submodules must be initialized):

```bash
docker build -f docker/Dockerfile -t as-vector:check .
```

Single node (client port 3000):

```bash
docker compose -f docker/compose-single.yml up -d
bash docker/smoke-up.sh   # build + readiness + vector-config check, prints SERVER_VECTOR_READY
```

3-node mesh cluster (client ports 3000/3010/3020):

```bash
docker compose -f docker/compose-cluster3.yml up -d
```

Environment knobs (both compose files):

| Env | Default | Meaning |
|---|---|---|
| `VECTOR_DIM` | 64 | namespace `vector-dimension` |
| `VECTOR_TYPE` | float | namespace `vector-value-type` (float/uint8/int8/int16) |
| `VECTOR_METRIC` | l2 | namespace `vector-metric` (l2/cosine/inner-product) |
| `AEROSPIKE_VECTOR_SIMD` | auto | kernel ISA: auto/scalar/sse/avx2/avx512/neon; invalid or unavailable values crash the first VECTOR_DISTANCE request by design |

The namespace is `test`, in-memory storage, 1 GiB. The image builds asd from
this source tree (factory branch state), so server-side SIMD kernels match
the checked-out commit.
