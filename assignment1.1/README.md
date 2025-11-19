
# CS670 Assignment 1.1 — 2-Party Additive MPC using Beaver Triples

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

 1. **Data Generation:** Initial user and item vectors are randomly generated and split into additive shares for each party.
 2. **Dealer:** A trusted dealer generates Beaver triples for secure multiplication and coordinates masked message exchanges.
 3. **Computation:** Each party uses its shares and the Beaver triples to compute masked values, exchange messages via the dealer, and locally compute the updated user share.
 4. **Output:** Each party writes its updated share to disk; the true updated vector can be reconstructed by summing both shares.


 ## File Descriptions

 - `gen_data.cpp`: Generates random user/item vectors and writes additive shares to `output/`.
 - `pB.cpp`: Party code (compiled twice for `p0` and `p1`). Handles masking, communication, and local computation.
 - `p2.cpp`: Trusted dealer. Listens for party connections, generates Beaver triples, and forwards masked messages.
 - `common.hpp`, `mpc_ops.hpp`, `shares.hpp`: Utility headers for vector operations, parsing, and IO.
 - `Dockerfile`: Builds all binaries in a containerized environment.
 - `docker-compose.yml`: Defines and runs all protocol services.
 - `verify.py`: Python script to reconstruct and verify the correctness of the updated user vector.
 - `output/`: Contains share files:
	 - `share_p0_0.txt`, `share_p1_0.txt`: Initial shares.
	 - `updated_share_p0_0.txt`, `updated_share_p1_0.txt`: Updated shares after protocol run.

 ## Changing Runtime Parameters

 - Default parameters (number of users, items, vector dimension, queries) are set in `docker-compose.yml` under the `command` fields.
 - To change, edit the values in `docker-compose.yml`. For example, to set dimension to 16:
	 ```yaml
	 gen_data:
		 command: ["./gen_data", "1", "1", "16"]
	 p0:
		 command: ["./p0", "1", "1", "16", "1", "p2", "9002"]
	 p1:
		 command: ["./p1", "1", "1", "16", "1", "p2", "9002"]
	 ```
 - After editing, rebuild and rerun the protocol using:
	 ```sh
	 docker compose build --no-cache
	 docker compose up --build --force-recreate
	 ```



## Proof of Correctness


The protocol securely computes the update $
u_i \leftarrow u_i + v_j \cdot (1 - \langle u_i, v_j \rangle)
$ using additive secret sharing and Beaver triples. Each party holds additive shares of $u$ and $v$, and the dealer provides shares of random Beaver triples $(a, b, c)$ such that $c = a \cdot b$. The multiplication of secret-shared values is performed as follows:

1. Each party computes masked values $\alpha_i = u_i + a_i$ and $\beta_i = v_i + b_i$ and sends them to the other party (via the dealer).
2. Both parties reconstruct $\alpha = \alpha_0 + \alpha_1$ and $\beta = \beta_0 + \beta_1$.
3. Each party computes its share of $u \cdot v$ using the Beaver triple and the reconstructed masked values, following the addition-based masking algebra in the code.
4. The update $
u_i \leftarrow u_i + v_j \cdot (1 - \langle u_i, v_j \rangle)
$ is then computed locally using the shares.

By the linearity of secret sharing and the properties of Beaver triples, the reconstructed value matches the intended arithmetic, and the protocol produces correct outputs as verified by summing the shares and comparing to the direct computation.

## Proof of Security

The protocol is secure in the semi-honest model, assuming the dealer is trusted and does not collude with either party:

- **Privacy:** Each party only sees masked values and never learns the other party's actual shares. The use of Beaver triples ensures that multiplication is performed without revealing the underlying secrets.
- **Dealer's Role:** The dealer only sees masked differences, which are uniformly random and independent of the actual secrets, due to the randomness of the Beaver triples.
- **No Information Leakage:** The messages exchanged (masked values, Beaver triple shares) do not reveal any information about the private vectors $u$ and $v$ beyond what is computable from the output.

Thus, the protocol achieves privacy and correctness for two-party computation of the specified update, under the assumption of a trusted dealer and honest-but-curious parties.



 ## Verification

 - After the protocol run, use `verify.py` to reconstruct the updated user vector and check correctness:
	 ```sh
	 python3 verify.py
	 ```
 - The script reads initial and updated shares, reconstructs vectors, computes the dot product, and verifies the update formula.
