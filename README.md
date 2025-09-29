# CS670 Assignment 1: Secure User-Profile Update in MPC

We have an User data and an Item data in the form of matrices for a reccomendation system. An user interacts with an items to produce a prediction $r_{ij} = \langle u_{i},v_{j}\rangle$ which is then used to update the user profile. There are two $S_{0}$ and $S_{1}$ which holds the additive secret share of $U$ and $V$. We have to use additive secret sharing technique in MPC to secure update son user profile vectors in a reccomendation system as $u_{i} \leftarrow u_{i} + v_{j}(1-\langle u_{i},v_{j}\rangle)$ without revealing the private vectors $u_i$ and $v_j$. I have done this using additive secret sharing in MPC and also with the replicated secret sharing in MPC, with every solution Dockerised.

---
## Procedure to Run the Code

Please install the modern version of Docker in our system, go to the project directory, unzip the zip file and run all the commands from there only.

### Additive 2-Party Computation

It uses two computation parties (`p0`, `p1`) and a third party (`p2`) that acts as a dealer for Beaver Triples.

1.  **Build the Docker Images:**
    ```bash
    docker compose build
    ```

2.  **Run the Protocol:**
    ```bash
    docker compose up
    ```

This command will first start a temporary container to generate the secret-shared data files. It will then start the three parties, which will connect, perform the secure computation for the number of queries specified in `docker-compose.yml`, and print all required debugging output to the console before exiting cleanly.

You can change nubmer of quesries, items,users and features in `docker-compose.yml` for additive secret sharing protocol and `docker-compose-replicated.yml` for replicated secret sharing protocol.

### Replicated 3-Party Computation

It uses three computation parties (`p0`, `p1`, `p2`) in a replicated secret sharing scheme.

1.  **Build the Docker Images:**
    ```bash
    docker compose -f docker-compose-replicated.yml build
    ```

2.  **Run the Protocol:**
    ```bash
    docker compose -f docker-compose-replicated.yml up
    ```
This command sequence will generate the replicated share files and then start the three parties. The parties will connect, perform the secure computation and party `p2` will print the reconstructed result to the console before all containers exit.

---

## Technical Report

### Part 1: Secret-Sharing Implementation

Both the secret sharing schemes operate over a ring of 32-bit unsigned integers, meaning all arithmetic is performed modulo $2^{32}$.

#### Additive Secret Sharing

This is a **2-out-of-2 secret sharing** scheme. A secret value $s$ is split into two random shares, $s_0$ and $s_1$, such that $s = s_0 + s_1$.
- **Sharing:** Party `p0` holds share $s_0$ and party `p1` holds share $s_1$.
- **Reconstruction:** To reconstruct the secret, both parties must provide their shares. Any single party learns nothing about the secret value.
- **Implementation:** The `gen_data.cpp` program implements this by generating a random share for `p0` and calculating `p1`'s share to maintain the additive relationship.

#### Replicated Secret Sharing

This is a **3-party secret sharing** scheme that provides security against one semi-honest party. A secret value $s$ is split into three random shares, $s_0, s_1, s_2$, such that $s = s_0 + s_1 + s_2$.
- **Sharing:** The shares are distributed in a replicated manner to ensure no single party can reconstruct the secret.
    - `p0` holds shares $(s_0, s_1)$
    - `p1` holds shares $(s_1, s_2)$
    - `p2` holds shares $(s_2, s_0)$
- **Reconstruction:** All three parties are required to combine their shares to reveal the secret.
- **Implementation:** The `gen_data_replicated.cpp` program generates these shares for the user and item vectors.

### Part 2: Secure Computation of Inner Products and Updates

The core task is the secure computation of $u_{i} \leftarrow u_{i} + v_{j}(1-\langle u_{i},v_{j}\rangle)$. This was broken down into secure addition, secure multiplication and secure dot product.

#### Secure Addition

In both the secret sharing schemes that we used, addition is a purely local operation that requires **zero communication**. Since the sharing is linear, parties can simply add their respective shares of two secrets to obtain a valid share of the sum. For vectors, this is done element-wise.

#### Secure Multiplication

Secure multiplication is the most complex primitive and is handled differently in each protocol.

* **Additive Protocol (Beaver's Triples):**
    Multiplying two additively shared secrets $[x]$ and $[y]$ requires interaction so i used Beaver's Triple method:
    1.  **Preprocessing Phase:** A trusted dealer (`p2`) generates a random triple $(a, b, c)$ where $c=a \cdot b$. It sends additive shares $([a_0], [a_1])$, $([b_0], [b_1])$, and $([c_0], [c_1])$ to `p0` and `p1`. This is handled by `p2.cpp`.
    2.  **Online Phase:** `p0` and `p1` compute shares of $\epsilon = x - a$ and $\delta = y - b$ locally. They exchange these shares to publicly reconstruct the values $\epsilon$ and $\delta$.
    3.  The final product $xy$ is computed as $xy = \epsilon\delta + \epsilon b + \delta a + c$. This expression is computed securely, with `p0` taking the public term $\epsilon\delta$ and the rest of the terms being computed locally using each party's shares of $a, b, c$. This is implemented in the `MPC_Multiply` function.

* **Replicated Protocol:**
    To securely multiply two secrets $[x]$ and $[y]$, where `p_i` holds shares $(x_i, x_{i-1})$ and $(y_i, y_{i-1})$, the parties proceed as follows:
    1.  Each party `p_i` can locally compute the term $x_i y_i$.
    2.  To compute the cross-product terms, each party `p_i` locally computes a partial product $d_i = x_i y_{i-1} + y_i x_{i-1}$.
    3.  Each party sends its partial product $d_i$ to its successor party (`p_{i+1}`).
    4.  The final share of the product for party `p_i` is $z_i = x_i y_i + d_i + d_{i-1}$ (where $d_{i-1}$ is the value received from the predecessor). This protocol is implemented in `MPC_Multiply_Replicated`.

#### Secure Dot Product and Final Update

- **`MPC_DOTPRODUCT`:** This function computes the inner product of two shared vectors. It iterates through the vectors, calling the appropriate `MPC_Multiply` function for each pair of elements and locally summing the resulting shares.
- **Final Update:** The full update rule is implemented by combining these primitives. `delta = 1 - dot_product` is computed (where `1` is shared as `(1, 0)`), followed by a secure vector-scalar multiplication (`MPC_VectorScalarMultiply`), and a final secure vector addition (`secure_add`).

### Part 3: Communication Rounds and Efficiency

- **Communication Analysis:**
    - **Addition:** 0 rounds. It is a free local computation.
    - **Multiplication (Additive):** 1 round of communication in the online phase to exchange the masked values. This is preceded by 1 round of communication with the dealer in the offline phase to receive the Beaver Triple.
    - **Multiplication (Replicated):** 1 round of communication where each party sends a partial product to its neighbor.
- **Efficiency:**
    - The performance of both protocols is dominated by the number of secure multiplications. The online phase is extremely fast, involving only lightweight arithmetic and a constant number of communication rounds per multiplication.
    - The main difference lies in the setup and trust model. The additive scheme requires a dedicated, trusted dealer for preprocessing, but the online phase only involves two parties. The replicated scheme distributes trust among three parties and does not require a separate dealer, but all three parties must participate in every multiplication. For high-latency networks, the number of rounds is the main bottleneck, making both protocols very efficient for the online phase.