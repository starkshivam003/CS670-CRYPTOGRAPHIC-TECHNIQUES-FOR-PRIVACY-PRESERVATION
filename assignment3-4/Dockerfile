# Multi-stage Dockerfile: build C++ binaries (builder) and produce a minimal runtime image
FROM ubuntu:24.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# Build the project using provided makefile
RUN make -f makefile || make

# Final runtime image: minimal OS + ca-certificates
FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built binaries from builder stage
COPY --from=builder /src/user /usr/local/bin/user
COPY --from=builder /src/server_sim /usr/local/bin/server_sim
COPY --from=builder /src/bench /usr/local/bin/bench
COPY --from=builder /src/test_protocol /usr/local/bin/test_protocol

# Only copy plots (no scripts, no Python)
COPY --from=builder /src/plots /app/plots

ENV PATH=/usr/local/bin:$PATH

CMD ["/bin/bash"]
