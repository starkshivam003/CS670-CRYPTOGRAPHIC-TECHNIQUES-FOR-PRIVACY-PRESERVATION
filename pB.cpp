/* This is the main logic fortwo primary parties (p0 and p1) in 2PC additive secret sharing protocol
 * This program connects to dealer (p2) and a peer (the other party) to securely compute the user profile update
 */

#include "common.hpp"
#include "mpc_ops.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>

// Compile-time check to ensure a role is defined for the party

#if !defined(ROLE_p0) && !defined(ROLE_p1)
#error "Define roles as ROLE_p0 or ROLE_p1"
#endif

// --- Function Prototypes ---

awaitable<void> run_queries(boost::asio::io_context& io_context, size_t num_queries, size_t num_users, size_t num_items);
SharedVector read_vector_from_file(const std::string& filename, size_t line_index);
awaitable<void> send_vector(tcp::socket& sock, const SharedVector& vec);
awaitable<SharedVector> recv_vector(tcp::socket& sock, size_t size);
SharedVector secure_add(const SharedVector& a, const SharedVector& b);


struct BeaverTripleShare;
awaitable<uint32_t> MPC_Multiply(uint32_t x_share, uint32_t y_share, tcp::socket& p2_sock, tcp::socket& peer_sock);
awaitable<uint32_t> MPC_DOTPRODUCT(const SharedVector& u, const SharedVector& v, tcp::socket& p2_sock, tcp::socket& peer_sock);
awaitable<SharedVector> MPC_VectorScalarMultiply(const SharedVector& v, uint32_t s_share, tcp::socket& p2_sock, tcp::socket& peer_sock);



int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <num_users> <num_items> <num_features> <num_queries>" << std::endl;
        return 1;
    }

    size_t num_users = std::stoul(argv[1]);
    size_t num_items = std::stoul(argv[2]);
    size_t num_features = std::stoul(argv[3]);
    size_t num_queries = std::stoul(argv[4]);

    try {
        std::cout.setf(std::ios::unitbuf); 
        boost::asio::io_context io_context(1);
        
        co_spawn(io_context, run_queries(io_context, num_queries, num_users, num_items), boost::asio::detached);
        
        io_context.run();
    } catch (const std::exception& e) {
        #if defined(ROLE_p0)
            std::cerr << "[p0] Exception: " << e.what() << std::endl;
        #else 
            std::cerr << "[p1] Exception: " << e.what() << std::endl;
        #endif
        return 1;
    }
    return 0;
}

// A struct to hold shares of Beaver Triple.
struct BeaverTripleShare {
    uint32_t a, b, c;
};


SharedVector read_vector_from_file(const std::string& filename, size_t line_index) {
    std::ifstream file("output/" + filename);
    if (!file.is_open()) throw std::runtime_error("Could not open output/" + filename);
    
    std::string line;
    for (size_t i = 0; i <= line_index; ++i) {
        if (!std::getline(file, line)) throw std::runtime_error("Not enough lines in " + filename);
    }
    
    SharedVector vec;
    std::stringstream ss(line);
    uint32_t value;
    while (ss >> value) vec.data.push_back(value);
    return vec;
}

// --- Networking Helper Coroutines ---

awaitable<void> send_coroutine(tcp::socket& sock, uint32_t value) {
    co_await boost::asio::async_write(sock, boost::asio::buffer(&value, sizeof(value)), use_awaitable);
}
awaitable<void> recv_coroutine(tcp::socket& sock, uint32_t& out) {
    co_await boost::asio::async_read(sock, boost::asio::buffer(&out, sizeof(out)), use_awaitable);
}
awaitable<void> send_vector(tcp::socket& sock, const SharedVector& vec) {
    co_await boost::asio::async_write(sock, boost::asio::buffer(vec.data), use_awaitable);
}
awaitable<SharedVector> recv_vector(tcp::socket& sock, size_t size) {
    SharedVector vec;
    vec.resize(size);
    co_await boost::asio::async_read(sock, boost::asio::buffer(vec.data), use_awaitable);
    co_return vec;
}


SharedVector secure_add(const SharedVector& a, const SharedVector& b) {
    SharedVector result;
    result.resize(a.size());
    for (size_t i = 0; i < a.size(); ++i) result.data[i] = a.data[i] + b.data[i];
    return result;
}

/**
 * Performs secure multiplication of two shared numbers using Beaver's Triple method.
 * x_share: Party's share of the first number.
 * y_share: Party's share of the second number.
 * p2_sock: Socket connected to the dealer (p2).
 * peer_sock: Socket connected to the other party.
 * z_share: Party's share of the product (x * y).
 */
awaitable<uint32_t> MPC_Multiply(uint32_t x_share, uint32_t y_share, tcp::socket& p2_sock, tcp::socket& peer_sock) {
    
    BeaverTripleShare triple_share;
    co_await boost::asio::async_read(p2_sock, boost::asio::buffer(&triple_share, sizeof(triple_share)), use_awaitable);
    
    
    uint32_t epsilon_share = x_share - triple_share.a;
    uint32_t delta_share = y_share - triple_share.b;
    
    
    uint32_t epsilon_other, delta_other;
    #if defined(ROLE_p0)
        co_await send_coroutine(peer_sock, epsilon_share);
        co_await recv_coroutine(peer_sock, epsilon_other);
        co_await send_coroutine(peer_sock, delta_share);
        co_await recv_coroutine(peer_sock, delta_other);
    #else
        co_await recv_coroutine(peer_sock, epsilon_other);
        co_await send_coroutine(peer_sock, epsilon_share);
        co_await recv_coroutine(peer_sock, delta_other);
        co_await send_coroutine(peer_sock, delta_share);
    #endif
    
    
    uint32_t epsilon = epsilon_share + epsilon_other;
    uint32_t delta = delta_share + delta_other;
    
    
    uint32_t z_share;
    #if defined(ROLE_p0)
        z_share = (epsilon * delta) + (epsilon * triple_share.b) + (delta * triple_share.a) + triple_share.c;
    #else
        z_share = (epsilon * triple_share.b) + (delta * triple_share.a) + triple_share.c;
    #endif
    co_return z_share;
}


// Performs a secure dot product on two shared vectors.

awaitable<uint32_t> MPC_DOTPRODUCT(const SharedVector& u, const SharedVector& v, tcp::socket& p2_sock, tcp::socket& peer_sock) {
    uint32_t result_share = 0;
    // Iterate through the vectors and securely multiply each pair of elements.
    for (size_t i = 0; i < u.size(); ++i) {
        result_share += co_await MPC_Multiply(u.data[i], v.data[i], p2_sock, peer_sock);
    }
    co_return result_share;
}

// Performs a secure multiplication of a shared vector by a shared scalar.

awaitable<SharedVector> MPC_VectorScalarMultiply(const SharedVector& v, uint32_t s_share, tcp::socket& p2_sock, tcp::socket& peer_sock) {
    SharedVector result;
    result.resize(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        result.data[i] = co_await MPC_Multiply(v.data[i], s_share, p2_sock, peer_sock);
    }
    co_return result;
}

// The main coroutine that orchestrates the entire MPC protocol for a set of queries.

awaitable<void> run_queries(boost::asio::io_context& io_context, size_t num_queries, size_t num_users, size_t num_items) {
    
    #if defined(ROLE_p0)
        const std::string ROLE = "p0";
    #else
        const std::string ROLE = "p1";
    #endif

    
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // --- Network Connection Setup ---
    tcp::resolver resolver(io_context);
    // Connect to the dealer (p2).
    tcp::socket p2_socket(io_context);
    co_await p2_socket.async_connect(*resolver.resolve("p2", "9002"), use_awaitable);
    
    // Connect to the peer (the other party).
    // p0 acts as the client, p1 acts as the server.
    tcp::socket peer_socket(io_context);
    #ifdef ROLE_p0
        co_await peer_socket.async_connect(*resolver.resolve("p1", "9001"), use_awaitable);
    #else
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 9001));
        peer_socket = co_await acceptor.async_accept(use_awaitable);
    #endif
    
    std::cout << "[" << ROLE << "] All connections established. Starting MPC for " << num_queries << " queries." << std::endl;

    // --- Main Protocol Loop ---
    for (size_t q = 0; q < num_queries; ++q) {
        size_t user_idx = q % num_users;
        size_t item_idx = q % num_items;
        std::cout << "\n--- [" << ROLE << "] Processing Query " << q+1 << ": User " << user_idx << ", Item " << item_idx << " ---" << std::endl;
        
        
        SharedVector u_i = read_vector_from_file((ROLE == "p0" ? "U0.txt" : "U1.txt"), user_idx);
        SharedVector v_j = read_vector_from_file((ROLE == "p0" ? "V0.txt" : "V1.txt"), item_idx);
        
        
        uint32_t dot_prod_share = co_await MPC_DOTPRODUCT(u_i, v_j, p2_socket, peer_socket);
        
        
        uint32_t delta_share = (ROLE == "p0" ? 1 : 0) - dot_prod_share;
        
        
        SharedVector update_term_share = co_await MPC_VectorScalarMultiply(v_j, delta_share, p2_socket, peer_socket);
        
        
        SharedVector new_u_i_share = secure_add(u_i, update_term_share);

        // --- Verification Step (for debugging) ---
        
        std::cout << "[" << ROLE << "] My final share of u'_" << user_idx << ": ";
        for(size_t i = 0; i < new_u_i_share.size(); ++i) { std::cout << new_u_i_share.data[i] << (i == new_u_i_share.size() - 1 ? "" : ","); }
        std::cout << std::endl;

        
        #if defined(ROLE_p0)
            SharedVector final_share_other = co_await recv_vector(peer_socket, new_u_i_share.size());
            co_await send_vector(peer_socket, new_u_i_share);
            SharedVector final_u_i = secure_add(new_u_i_share, final_share_other);
            std::cout << "[" << ROLE << "] Reconstructed u'_" << user_idx << " for query " << q+1 << ": ";
            for(size_t i = 0; i < final_u_i.size(); ++i) { std::cout << final_u_i.data[i] << (i == final_u_i.size() - 1 ? "" : ","); }
            std::cout << std::endl;
        #else
            co_await send_vector(peer_socket, new_u_i_share);
            SharedVector final_share_other = co_await recv_vector(peer_socket, new_u_i_share.size());
            SharedVector final_u_i = secure_add(new_u_i_share, final_share_other);
            std::cout << "[" << ROLE << "] Reconstructed u'_" << user_idx << " for query " << q+1 << ": ";
            for(size_t i = 0; i < final_u_i.size(); ++i) { std::cout << final_u_i.data[i] << (i == final_u_i.size() - 1 ? "" : ","); }
            std::cout << std::endl;
        #endif
    }
}