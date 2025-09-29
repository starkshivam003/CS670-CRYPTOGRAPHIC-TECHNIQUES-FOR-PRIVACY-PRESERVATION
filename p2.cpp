#include "common.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <vector>

using boost::asio::ip::tcp;

struct BeaverTripleShare {
    uint32_t a, b, c;
};

awaitable<void> dealer_session(tcp::socket p0_socket, tcp::socket p1_socket, size_t total_triples) {
    try {
        std::cout << "[p2] Session started. Providing " << total_triples << " triples..." << std::endl;
        for (size_t i = 0; i < total_triples; ++i) {
            uint32_t a = random_uint32();
            uint32_t b = random_uint32();
            uint32_t c = a * b;

            BeaverTripleShare share0 = {random_uint32(), random_uint32(), random_uint32()};
            BeaverTripleShare share1 = {a - share0.a, b - share0.b, c - share0.c};

            co_await boost::asio::async_write(p0_socket, boost::asio::buffer(&share0, sizeof(share0)), use_awaitable);
            co_await boost::asio::async_write(p1_socket, boost::asio::buffer(&share1, sizeof(share1)), use_awaitable);
        }
        std::cout << "[p2] All triples provided. Shutting down." << std::endl;
    } catch (std::exception&) {
        // A party might disconnect early if there's an error.
    }
}

awaitable<void> listener(size_t num_queries, size_t num_features) {
    auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, {tcp::v4(), 9002});
    
    // Each query requires (features * 2) multiplications (dot product + vector-scalar multiply)
    size_t total_triples_needed = num_queries * num_features * 2;

    std::cout << "[p2] Dealer is running, waiting for p0 and p1..." << std::endl;
    
    tcp::socket p0_socket = co_await acceptor.async_accept(use_awaitable);
    tcp::socket p1_socket = co_await acceptor.async_accept(use_awaitable);
    
    co_await dealer_session(std::move(p0_socket), std::move(p1_socket), total_triples_needed);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <num_queries> <num_features>" << std::endl;
        return 1;
    }
    size_t num_queries = std::stoul(argv[1]);
    size_t num_features = std::stoul(argv[2]);

    try {
        boost::asio::io_context io_context;
        co_spawn(io_context, listener(num_queries, num_features), detached);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception in P2: " << e.what() << "\n";
    }
    return 0;
}