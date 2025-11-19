
#include "common.hpp"
#include "shares.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/asio/detached.hpp>
#include <utility>

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::ip::tcp;

// Read a ReplicatedVector from file
ReplicatedVector read_repvec(const std::string& filename, size_t idx, size_t k) {
    std::ifstream file("output/" + filename);
    if (!file.is_open()) throw std::runtime_error("Could not open " + filename);
    std::string line_local, line_next;
    for (size_t i = 0; i <= idx * 2; ++i) std::getline(file, line_local);
    std::getline(file, line_next);
    ReplicatedVector vec(k);
    std::istringstream ss_local(line_local), ss_next(line_next);
    for (size_t j = 0; j < k; ++j) {
        Field a, b;
        ss_local >> a;
        ss_next >> b;
        vec[j] = ReplicatedShare(a, b);
    }
    return vec;
}

// Du-Atallah batched multiplication for a single feature
awaitable<ReplicatedShare> duatallah_mul(Field x0, Field x1, Field y0, Field y1, int my_id, tcp::socket& prev_sock, tcp::socket& next_sock) {
 
    Field r = random_uint64() % MODULUS;
    Field sendA, recvA, sendB, recvB;
    if (my_id == 0) {
        sendA = add_mod(x0, r);
        co_await boost::asio::async_write(next_sock, boost::asio::buffer(&sendA, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&recvB, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_write(next_sock, boost::asio::buffer(&r, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&recvA, sizeof(Field)), use_awaitable);
        Field T = mul_mod(sendA, recvB);
        Field w_local = sub_mod(0, mul_mod(r, recvB));
        Field w_next = add_mod(T, mul_mod(r, recvA));
        co_return ReplicatedShare(w_local, w_next);
    } else if (my_id == 1) {
        sendB = add_mod(y0, r);
        co_await boost::asio::async_write(next_sock, boost::asio::buffer(&sendB, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&recvA, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_write(next_sock, boost::asio::buffer(&r, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&recvB, sizeof(Field)), use_awaitable);
        Field T = mul_mod(recvA, sendB);
        Field w_local = sub_mod(0, mul_mod(r, recvA));
        Field w_next = add_mod(T, mul_mod(r, recvB));
        co_return ReplicatedShare(w_local, w_next);
    } else {
        co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&recvA, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&recvB, sizeof(Field)), use_awaitable);
        co_await boost::asio::async_write(next_sock, boost::asio::buffer(&r, sizeof(Field)), use_awaitable);
        Field T = mul_mod(recvA, recvB);
        Field w_local = add_mod(T, mul_mod(r, recvA));
        Field w_next = sub_mod(0, mul_mod(r, recvB));
        co_return ReplicatedShare(w_local, w_next);
    }
}

// Replicated vector multiplication using Du-Atallah
awaitable<ReplicatedVector> replicated_mulvec(const ReplicatedVector& u, const ReplicatedVector& v, int my_id, tcp::socket& prev_sock, tcp::socket& next_sock) {
    size_t k = u.size();
    ReplicatedVector result(k);
    for (size_t j = 0; j < k; ++j) {
        result[j] = co_await duatallah_mul(u[j].s_local, u[j].s_next, v[j].s_local, v[j].s_next, my_id, prev_sock, next_sock);
    }
    co_return result;
}

// Dot product in replicated shares
Field dot_product(const ReplicatedVector& u, const ReplicatedVector& v) {
    Field sum = 0;
    for (size_t j = 0; j < u.size(); ++j) {
        sum = add_mod(sum, add_mod(mul_mod(u[j].s_local, v[j].s_local), mul_mod(u[j].s_next, v[j].s_next)));
    }
    return sum;
}

// Delta update: u_i <- u_i + delta * v_j
void delta_update(ReplicatedVector& u, const ReplicatedVector& v, Field delta) {
    for (size_t j = 0; j < u.size(); ++j) {
        u[j].s_local = add_mod(u[j].s_local, mul_mod(v[j].s_local, delta));
        u[j].s_next  = add_mod(u[j].s_next,  mul_mod(v[j].s_next,  delta));
    }
}

awaitable<void> run_replicated(int my_id, size_t num_queries, size_t num_users, size_t num_items, size_t num_features) {
    auto exec = co_await boost::asio::this_coro::executor;
    std::string ROLE = "p" + std::to_string(my_id);
    int next_id = (my_id + 1) % 3;
    int prev_id = (my_id + 2) % 3;
    std::map<int, std::string> party_hostnames = {{0, "p0"}, {1, "p1"}, {2, "p2"}};
    const short port = 9000;

    tcp::socket prev_sock(exec), next_sock(exec);
    tcp::acceptor acceptor(exec, {tcp::v4(), port});
    tcp::resolver resolver(exec);

    if (my_id == 0) {
        prev_sock = co_await acceptor.async_accept(use_awaitable);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        co_await next_sock.async_connect(*resolver.resolve(party_hostnames[next_id], std::to_string(port)), use_awaitable);
    } else {
        co_await next_sock.async_connect(*resolver.resolve(party_hostnames[next_id], std::to_string(port)), use_awaitable);
        prev_sock = co_await acceptor.async_accept(use_awaitable);
    }

    std::cout << "[" << ROLE << "] Connections established. Starting replicated MPC." << std::endl;

    for (size_t q = 0; q < num_queries; ++q) {
        size_t user_idx = q % num_users;
        size_t item_idx = q % num_items;

        ReplicatedVector u = read_repvec("U" + std::to_string(my_id) + "_rep.txt", user_idx, num_features);
        ReplicatedVector v = read_repvec("V" + std::to_string(my_id) + "_rep.txt", item_idx, num_features);

        // Secure dot product
        ReplicatedVector prod = co_await replicated_mulvec(u, v, my_id, prev_sock, next_sock);
        Field dot_share = dot_product(prod, prod); 

        Field recon = dot_share;
        if (my_id != 0) {
            Field sum_from_prev;
            co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&sum_from_prev, sizeof(Field)), use_awaitable);
            recon = add_mod(recon, sum_from_prev);
        }
        if (my_id != 2) {
            co_await boost::asio::async_write(next_sock, boost::asio::buffer(&recon, sizeof(Field)), use_awaitable);
        }
        if (my_id == 2) {
            std::cout << "[" << ROLE << "] Reconstructed dot product for query " << q+1 << ": " << recon << std::endl;
        }

        // Delta update
        Field delta = sub_mod(1, recon);
        delta_update(u, v, delta);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <my_id> <num_users> <num_items> <num_features> <num_queries>" << std::endl;
        return 1;
    }
    int my_id = std::stoi(argv[1]);
    size_t num_users = std::stoul(argv[2]);
    size_t num_items = std::stoul(argv[3]);
    size_t num_features = std::stoul(argv[4]);
    size_t num_queries = std::stoul(argv[5]);

    try {
        boost::asio::io_context io_context;
        co_spawn(io_context, run_replicated(my_id, num_queries, num_users, num_items, num_features), detached);
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "[p" << my_id << "] Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}