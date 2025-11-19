# CS670 Assignment 3 - Updating Item Profile using User-Side DPF Generation

When a user issues a query, the item profiles are updated according to:

$v_j ← v_j + u_i (1 − ⟨u_i, v_j⟩)$,

where $u_i$ is the user’s profile and $v_j$ is the profile of the item that was queried.

## Challenge

Updating item profiles securely is non-trivial because:

- The servers, who hold secret shares of the item profiles, do not know which item $j$ should be updated.
- The user, on the other hand, knows which item was queried, but does not know the update value

$M = u_i (1 − ⟨u_i, v_j⟩)$,

since it depends on $v_j$, which is shared between the servers.

## Ideal (Infeasible) Approach

If the user somehow knew $M$, they could directly perform a private update using a Distributed Point Function (DPF):

$(k_0, k_1) ← Gen(j, M)$.

Each server $P_b (b ∈ {0, 1})$ could then locally update its share of the item profiles as:

$V_b ← V_b + EvalFull(k_b)$,

which adds $M$ to the $j^{th}$ position of the shared item profile vector without revealing $j$. However, since the user does not know $M$, this approach is not feasible.

## Actual Protocol

To overcome this, we proceed as follows.

Step 1: User-side DPF generation.
The user generates a DPF that points to the desired index i* but carries a zero value:

$(k0, k1) ← Gen(i^*, 0)$.

Let each key have the structure $k_b = (s_b, cw_0, . . . , cw_h, FCW)$, where $FCW_b$ denotes the final correction word.
The user sends:

$k'_0 = (s_0, cw_0, . . . , cw_h, FCW_0)$ to $P_0, k'_1 = (s1, cw0, . . . , cwh, FCW1)$ to $P_1$,

such that $FCW_0 + FCW_1 = FCW$.

Step 2: Server-side computation of the update value.
Each server locally computes its share of the update term:

$M = u_i (1 − ⟨u_i, v_j⟩)$.

Denote these shares as $M_0$ and $M_1$, such that: $M = M_0 + M_1$.

Step 3: Adjusting the DPF final correction word.
Each server modifies the final correction word of its DPF key so that the combined DPF now encodes $M$ (instead of 0) at index $i^*$.
Each server sends the masked difference:

$P_0 : M_0 − FCW_0, P_1 : M_1 − FCW_1$.

After exchanging these values, both servers compute:

$FCW_m = (M_0 − FCW_) + (M_1 − FCW_1)$.

The updated DPF keys are thus: $k_b = (s_b, cw_0, . . . , cw_h, FCW_m)$.

Step 4: Applying the update.
Each server evaluates its DPF key and adds the result to its share of the item profiles:

$V_b ← V_b + EvalFull(k_b)$.

This procedure correctly and privately applies the update $M$ to the item profile $v_j$, without revealing the target index $j$ or the update value $M$ to either server or the user.

Conversion from XOR to Additive Shares

1. Note that the output of EvalFull will be in the form of XOR shares, whereas the item profiles are stored as additive shares. To make the update compatible, we must convert the XOR shares into additive shares.
2. This can be done by having one of the parties multiply its vector by −1. However, the challenge lies in determining which party should perform this negation. If the wrong party multiplies by −1, the result will have a sign error.
3. For the purposes of this assignment, you may assume an insecure method for determining which party negates its vector. Implementing the conversion securely will earn you bonus points.

## Security assumptions and threat model

- Trust model: two non-colluding honest-but-curious servers (P0 and P1). Each server follows the protocol but tries to learn additional information from its view.
- Cryptographic primitives: the prototype uses PRG-based expansion (xorshift64* wrapper). For production, replace with a CSPRNG or stream cipher.
- Secret shares: item profiles are stored as additive shares in `int64_t` fixed-point (`FieldT`) with scale `SCALE`.
- Goal: a single server should not learn the user's index `j` nor the update `M` from its local view. The insecure baseline conversion leaks which party negated; the simulated Beaver conversion aims to avoid that leakage (toy model).

## Protocol Logic
1. User chooses an index $i$ and an update value $v$.
2. User generates a pair of DPF keys $(k_0, k_1)$ such that $Eval(k_0, x)$ XOR $Eval(k_1, x)$ = 1 if $x = i$, else 0.
3. User sends $k_0$ to Server0 and $k_1$ to Server1. The value $v$ is encoded with the point function (e.g., multiply the point indicator by $v$ or produce masking shares).
4. Each server locally evaluates its key across the domain positions needed (or uses an $EvalFull$ routine for the index) producing XOR-style shares for each position.
5. Servers convert XOR-shares to additive shares for the data domain (conversion routines provided) so updates can be applied to an additive-shared database.
6. Servers apply their additive shares to their local database replicas. The combination (Server0 + Server1) equals the correct updated database.
7. Reconstruction: Servers (or a verifier) reconstruct the updated position for test/verification only.

Note: This protocol implement both a baseline conversion (direct, faster but less secure in some models) and a secure conversion (Beaver-style multiplication techniques) to transform XOR shares into additive shares.


## Files in This Repository
- `dpf.h`, `dpf.cpp`: DPF key generation, representation, and evaluation routines (Eval, EvalFull, key serialization).
- `conversion.h`, `conversion.cpp`: XOR-to-additive conversion implementations (baseline and secure variants).
- `user.cpp`: Command-line utility that constructs DPF keys and demonstrates a client-side update/query.
- `server.cpp`: Server-side simulation that evaluates DPF keys, performs conversions, and applies updates to local storage.
- `bench.cpp`: Micro-benchmark harness that measures runtime cost of key operations and writes `plots/bench_results.csv`.
- `tests/test_protocol.cpp`: Unit tests validating correctness for representative domain sizes and random inputs.
- `makefile`: Build rules to produce binaries (e.g., `user`, `server`, `bench`, `test_protocol`).
- `run.sh`: Convenience script to run common scenarios used in development or grading.
- `Dockerfile`: Optional containerized build environment for reproducible compilation and runs.
- `dpf.h`/`dpf.cpp` and `conversion.*`: core crypto/primitive implementations; treat them as the trust-critical code.

## Running the  Protocol


### Dockerfile

Build:

```bash
docker build -t cs670-dpf:latest .
```
Run:
```bash
docker run --rm -it cs670-dpf:latest /usr/local/bin/test_protocol
```


## Verification & Tests
- Unit tests: `tests/test_protocol.cpp` covers correctness for small domains and random inputs. Run with `./tests/test_protocol`.
- Sanity checks: the test harness reconstructs the updated value by adding server shares and compares with the expected update.
- Benchmarks: `bench.cpp` measures latency for key operations; results are written to `plots/bench_results.csv`.

Suggested verification steps (local):
1. `make all`
2. `./tests/test_protocol` — expect a clear success message.
3. `./bench` and inspect `plots/bench_results.csv` for timing numbers.

## Proofs of Correctness and Security

Correctness:
- DPF correctness: By construction, for all x the bitwise XOR of evaluations equals the point function: $Eval(k_0,x)$ XOR $Eval(k_1,x)$ = $[x=i]$.
- Value encoding: If the user encodes the update $v$ by multiplying the point indicator by $v$ (or by producing multiplicative shares), then server-local results are additive shares whose sum equals $v$ at index $i$ and $0$ elsewhere.
- Conversion correctness: The XOR-to-additive conversion routines are designed so that, given matching XOR shares from both servers, the produced additive shares sum to the original XOR result at every coordinate.

Security:
- Single-server privacy: A single server only receives one DPF key ($k_b$). For well-constructed DPFs, $k_b$ is indistinguishable from random under standard PRF assumptions; hence a single key reveals no information about the target index $i$.
- Conversion leakage: The secure conversion routine uses masked multiplications (or preprocessed randomness like Beaver triples) so that intermediate messages do not leak which party had a $1$ at the secret index.
- Formal argument: a simulator for a single-server view can generate a synthetic view using only the server's input and outputs (local randomness and public parameters) such that the distribution is computationally indistinguishable from the real view under the PRF and secure multiplication assumptions.

# CS670 Assignment 4 - Performance Evaluation

## Objective 
In this assignment, you will evaluate your protocol. Vary the number of queries, items, and users,
and plot the time taken for one user profile update and one item profile update.

The goal is to measure the cost of:
- Secure item updates (DPF-based, index-private)
- Local user updates (baseline)

Experiments were run for $N ∈$ \{50,100, 200, 300, 400, 500, 600, 700, 800, 900, 1000}\. For each setting, I recorded execution time (in ns) averaged over many runs.

## Observed Trend from `plots/bench_results.csv`
- **User update time is constant**. User vector update is local, dimension d = 2, so
complexity is $O(1)$.

- **User-local updates are tiny and stable.** Typical user-local times are on the order of tens of nanoseconds (e.g., ~15–25 ns), with very low variance across runs.
- **DPF-based secure updates are larger (microsecond range).** Representative values from the CSV:
  - `vector_dim = 2`: secure averages around ~6–8 μs (6,000–8,000 ns).
  - `vector_dim = 4`: secure averages commonly range from ~13–35 μs, with some experiments showing higher variance.
  - `vector_dim = 8`: secure averages cluster around ~35–40 μs.

- **Vector dimension (`vector_dim`) is a major cost driver.** Increasing the vector dimension raises the cost of `EvalFull` and the XOR→additive conversion, so secure-time increases significantly with dimension.
- **Domain size (`N`) shows weaker effects in many experiments.** For several parameter groups secure times vary less with `N` than with `vector_dim`; this indicates per-vector work (and vector dimension) often dominates the measured cost in this prototype. Nonetheless, some parameter sets show higher variance for larger `N` or run counts.
- **Variance & outliers.** Some CSV rows show large max values and higher stddev; these are likely due to transient system effects (scheduling, cache, CPU frequency scaling) during runs and are visible in the `secure_time_ns_max` and `secure_time_ns_stddev` columns.

## Plots
![Short alt text](/plots/linear_final.png)
![Short alt text](/plots/log_final.png)

## Interpretation

- The linear plot shows absolute timing differences — secure updates appear much larger than user-local updates in raw time units.
- The log plot emphasizes the multiplicative gap: secure updates are often hundreds-to-thousands of times slower than the user-local baseline depending on vector dimension and experimental noise.