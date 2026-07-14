# C client rejects from-source asd: "version is invalid: <sha>"

Symptom: `aerospike_connect` fails with code -1 "Failed to connect" while raw
TCP and the info protocol both work. With `as_log_set_level(DEBUG)` the real
error is:

```
Failed to connect to seed <host> 3000. AEROSPIKE_ERR_CLIENT
Node <id> <ip>:3000 version is invalid: d6f6b9969
```

Cause: `build/gen_version` falls back to the git short SHA when the checkout
has no upstream version tags (`git describe --tags` finds nothing on a fork
cloned without tags). The client validates the node's `build` info value as
`X.Y.Z...` and rejects the node otherwise.

Fix: `git fetch upstream --tags` before building the image (the docker build
COPYs `.git`, so local tags flow into `gen_version`). Any environment that
builds asd from a fork must carry the version tags.

Diagnosis ladder that found it (worth reusing): harness error -> bash
`/dev/tcp` connectivity -> stdlib info-protocol probe -> minimal C probe
linking libaerospike with a DEBUG `as_log` callback. The client's default
error string ("Failed to connect") hides the node-validation detail.
