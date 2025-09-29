#include "common.hpp"
#include "shares.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <map>

struct ReplicatedShare {
    SharedVector s_i;
    SharedVector s_j;
};

ReplicatedShare read_replicated_shares(const std::string& filename, size_t line_index);
awaitable<void> run_replicated(int my_id, size_t num_queries, size_t num_users, size_t num_items);
awaitable<uint32_t> MPC_Multiply_Replicated(uint32_t x_i, uint32_t x_j, uint32_t y_i, uint32_t y_j, tcp::socket& prev_sock, tcp::socket& next_sock);

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
        co_spawn(io_context, run_replicated(my_id, num_queries, num_users, num_items), detached);
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "[p" << my_id << "] Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

ReplicatedShare read_replicated_shares(const std::string& filename, size_t line_index) {
    std::ifstream file("output/" + filename);
    if (!file.is_open()) throw std::runtime_error("FATAL: Could not open output/" + filename);
    std::string line1, line2;
    for (size_t i = 0; i < (line_index * 2) + 2; ++i) {
        if (!std::getline(file, i % 2 == 0 ? line1 : line2)) {
            throw std::runtime_error("FATAL: Could not read lines from " + filename);
        }
    }
    ReplicatedShare rep_share;
    std::stringstream ss1(line1), ss2(line2);
    uint32_t val;
    while (ss1 >> val) rep_share.s_i.data.push_back(val);
    while (ss2 >> val) rep_share.s_j.data.push_back(val);
    return rep_share;
}

awaitable<uint32_t> MPC_Multiply_Replicated(uint32_t x_i, uint32_t x_j, uint32_t y_i, uint32_t y_j, tcp::socket& prev_sock, tcp::socket& next_sock) {
    uint32_t d_i = x_i * y_j + x_j * y_i;
    uint32_t d_prev;
    
    co_await boost::asio::async_write(next_sock, boost::asio::buffer(&d_i, sizeof(d_i)), use_awaitable);
    co_await boost::asio::async_read(prev_sock, boost::asio::buffer(&d_prev, sizeof(d_prev)), use_awaitable);

    uint32_t z_i = x_i * y_i + d_i + d_prev;
    co_return z_i;
}

awaitable<void> run_replicated(int my_id, size_t num_queries, size_t num_users, size_t num_items) {
    auto io_context = co_await this_coro::executor;
    std::string ROLE = "p" + std::to_string(my_id);
    int next_id = (my_id == 2) ? 0 : my_id + 1;
    std::map<int, std::string> party_hostnames = {{0, "p0"}, {1, "p1"}, {2, "p2"}};
    const short port = 9000;
    
    tcp::socket prev_sock(io_context), next_sock(io_context);
    tcp::acceptor acceptor(io_context, {boost::asio::ip::tcp::v4(), port});
    tcp::resolver resolver(io_context);
    
    if (my_id == 0) {
        prev_sock = co_await acceptor.async_accept(use_awaitable); 
        std::this_thread::sleep_for(std::chrono::seconds(1));
        co_await next_sock.async_connect(*resolver.resolve(party_hostnames[next_id], std::to_string(port)), use_awaitable);
    } else {
        co_await next_sock.async_connect(*resolver.resolve(party_hostnames[next_id], std::to_string(port)), use_awaitable);
        prev_sock = co_await acceptor.async_accept(use_awaitable);
    }
    
    std::cout << "[" << ROLE << "] Connections established. Starting replicated MPC." << std::endl;
    
    for(size_t q = 0; q < num_queries; ++q) {
        size_t user_idx = q % num_users;
        size_t item_idx = q % num_items;

        ReplicatedShare u = read_replicated_shares("U" + std::to_string(my_id) + "_rep.txt", user_idx);
        ReplicatedShare v = read_replicated_shares("V" + std::to_string(my_id) + "_rep.txt", item_idx);
        
        uint32_t dot_prod_share = 0;
        for (size_t k = 0; k < u.s_i.size(); ++k) {
            dot_prod_share += co_await MPC_Multiply_Replicated(u.s_i.data[k], u.s_j.data[k], v.s_i.data[k], v.s_j.data[k], prev_sock, next_sock);
        }
        
        uint32_t current_sum = dot_prod_share;
        if (my_id != 0) {
            uint32_t sum_from_prev;
            co_await prev_sock.async_receive(boost::asio::buffer(&sum_from_prev, sizeof(sum_from_prev)), use_awaitable);
            current_sum += sum_from_prev;
        }

        if (my_id != 2) {
            co_await next_sock.async_send(boost::asio::buffer(&current_sum, sizeof(current_sum)), use_awaitable);
        }

        if (my_id == 2) {
             std::cout << "[" << ROLE << "] Reconstructed dot product for query " << q+1 << ": " << current_sum << std::endl;
        }
    }
}