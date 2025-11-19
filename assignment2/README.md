# CS670 Assignment 2: Distributed Point unctions (DPF) 



## Objective

Write a C++ program named `gen_queries.cpp` that generates Distributed Point Function (DPF) queries and verifies their correctness.

Specifications:

1. Program Name: `gen_queries.cpp`
2. Command-Line Arguments:
	- Usage: `./gen_queries <DPF_size> <num_DPFs>`
	- `<DPF_size>`: Size of each DPF (domain size).
	- `<num_DPFs>`: Number of DPF instances to generate.
3. Functionality:
	- For each DPF:
	  - Randomly choose an index (`location`) within the DPF’s domain.
	  - Randomly choose a target value for that index (this will be determined by the final correction word).
	  - Call your `generateDPF()` function to generate the DPF keys.
	- Save or print the generated DPF queries as appropriate.
4. Correctness Testing (`EvalFull` function):
	- Implement a function named `EvalFull()` that:
	  - Evaluates the DPF across all domain points for both keys.
	  - Combines the outputs and checks if they reconstruct the correct target value at the chosen index, and zero elsewhere.
	  - Prints `Test Passed` if the evaluation is correct and `Test Failed` otherwise.
5. Implementation Notes:
	- Use a cryptographically secure random generator (e.g., `std::random_device` and `std::mt19937_64` or platform CSPRNGs).
	- Keep the DPF interface modular:
	  - `generateDPF(location, value)`
	  - `evalDPF(key, index)`
	  - `EvalFull(key, size)`
6. Example Usage:
	- `./gen_queries 1024 10`
	  - This should generate 10 DPFs, each of size 1024, test their correctness using `EvalFull`, and print the results.


## File Descriptions
- `gen_queries.cpp`: Main implementation of the DPF protocol, including key generation, evaluation, and verification functions. Contains the `main()` function for running tests and verification.

## Changing Runtime Parameters
To hange the runtime parameters by passing command-line arguments to the executable:
- `<DPF_size>`: Size of the DPF domain (must be a power of 2).
- `<num_DPFs>`: Number of DPF instances to generate and test.
- `--verify-sample N`: (Optional) Number of random non-target indices to sample for verification. If omitted, full verification is performed for small domains, and sampling is used for large domains by default.

Compilation:
```bash
g++ -std=c++20 gen_queries.cpp -o gen_queries -lcrypto
```  


Example usage:
```bash
./gen_queries 1024 10
```




