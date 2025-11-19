FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    libboost-all-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

# Build Task-1 binaries
RUN g++ -std=c++17 gen_data.cpp -o gen_data -lstdc++fs
RUN g++ -std=c++17 p2.cpp -o p2 -lboost_system -pthread
RUN g++ -std=c++17 pB.cpp -o p0 -DROLE=0 -lboost_system -pthread
RUN g++ -std=c++17 pB.cpp -o p1 -DROLE=1 -lboost_system -pthread

CMD ["/bin/bash"]