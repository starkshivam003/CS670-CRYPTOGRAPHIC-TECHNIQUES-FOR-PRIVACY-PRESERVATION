
# CS670 Assignment 1.2 - Replicated MPC using Du-Atallah

## Background

In recommendation systems users and items are represented by latent vectors:
$U \in \mathbb{Z}^{m\times k},\; V \in \mathbb{Z}^{n\times k}$.

- $m$ is the number of users.
- $n$ is the number of items.
- $k$ is the number of features (latent dimensions).

For a user $i$ with profile $u_i$ and an item $j$ with profile $v_j$, the prediction is the inner product:
$$
\hat r_{ij} = \langle u_i, v_j \rangle
$$

After observing a query the user profile is updated by:
$$
u_i \leftarrow u_i + v_j \cdot (1 - \langle u_i, v_j \rangle)
$$

For this assignment all vectors live in integral space (signed 64-bit integers) and are secret-shared between parties.

## Objective

Two servers, $S_0$ and $S_1$, hold additive shares of the user and item matrices:
$$
U = U_0 + U_1, \qquad V = V_0 + V_1
$$

When user $i$ queries item $j$ the protocol must securely perform:

1. Compute the inner product $\langle u_i, v_j \rangle$ in MPC.
2. Compute $\delta = 1 - \langle u_i, v_j \rangle$ securely (as additive shares).
3. Update $u_i \leftarrow u_i + v_j \cdot \delta$ in secret-shared form.
4. Re-share the updated $u_i$ across $S_0$ and $S_1$ (i.e., produce new additive shares that sum to the updated profile).

The implementation uses Beaver triples to perform secure multiplications (dot-products and vector-scalar products) while preserving the privacy of each party's shares.

## Protocol Logic
1. **Replicated Sharing:**
   - Each value $x$ is split into three shares $s_0, s_1, s_2$ such that $x = s_0 + s_1 + s_2 \mod p$.
   - Each party holds two consecutive shares: Party 0 $(s_0, s_1)$, Party 1 $(s_1, s_2)$, Party 2 $(s_2, s_0)$.
2. **Du–Atallah Multiplication:**
   - Secure multiplication of secret-shared values using random masks and communication between parties.
3. **Secure Dot Product:**
   - Each party computes the dot product in the secret-shared domain using replicated multiplication.
4. **Delta Update:**
   - After reconstructing $\delta = 1 - \langle u_i, v_j \rangle$, each party updates their shares: $u_i \leftarrow u_i + \delta \cdot v_j$.
5. **Verification:**
   - Parties exchange partial sums to reconstruct and verify the dot product for correctness.
6. **Arithmetic:**
   - All arithmetic is performed modulo $p = 2^{61} - 1$.


## File Descriptions
- `gen_data_replicated.cpp`: Generates random user/item vectors and creates replicated shares for each party. Outputs are written to `output/U*_rep.txt` and `output/V*_rep.txt`.
- `p_replicated.cpp`: Implements the protocol logic for each party, including reading shares, secure computation, networking, and delta updates.
- `shares.hpp`: Defines the field, modular arithmetic, and replicated share structure.
- `common.hpp`: Utility functions for randomness and support routines.
- `Dockerfile`: Builds the binaries in an Ubuntu container, installing dependencies and compiling the code.
- `docker-compose-replicated.yml`: Orchestrates the protocol, running the data generator and launching three parties with correct arguments.
- `output/`: Contains generated replicated shares for users (`U*_rep.txt`) and items (`V*_rep.txt`) for each party.

## Changing Runtime Parameters
To modify vector sizes, field modulus, or other runtime parameters, edit the relevant constants in `gen_data_replicated.cpp` and `p_replicated.cpp`. Rebuild the containers after making changes:
```sh
docker compose -f docker-compose-replicated.yml build --no-cache
```
```sh
docker compose -f docker-compose-replicated.yml up --build --force-recreate
```


## Proof of Correctness and Security
**Correctness:**
- The replicated sharing ensures that $x = s_0 + s_1 + s_2 \mod p$ is always satisfied.
- The Du–Atallah protocol securely computes products without leaking information, and the delta update is performed consistently in the secret-shared domain.

**Security:**
- No single party can reconstruct the secret alone; at least two shares are needed.
- Communication is limited to masked values, preventing information leakage.
- The protocol is robust against honest-but-curious adversaries, as all operations are symmetric and no party learns more than their own shares.

