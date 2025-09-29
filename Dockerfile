FROM ubuntu:22.04

# These are the set of commands that will download the required dependecies and C++ compiler version g++-12
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y g++-12 gcc-12 cmake libboost-all-dev && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 60 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 60

WORKDIR /app
COPY . .

# Set of commands to compile the codes for Additive Shares MPC and get the executables
RUN g++ -std=c++20 -pthread gen_data.cpp -o gen_data
RUN g++ -std=c++20 -pthread pB.cpp -o p0 -DROLE_p0 -lboost_system -lboost_coroutine
RUN g++ -std=c++20 -pthread pB.cpp -o p1 -DROLE_p1 -lboost_system -lboost_coroutine
RUN g++ -std=c++20 -pthread p2.cpp -o p2 -lboost_system -lboost_coroutine

# These are the set of commans for the replicated MPC
RUN g++ -std=c++20 -pthread gen_data_replicated.cpp -o gen_data_replicated
RUN g++ -std=c++20 -pthread p_replicated.cpp -o p_replicated -lboost_system -lboost_coroutine


CMD ["sh", "-c", "exec /app/$ROLE"]