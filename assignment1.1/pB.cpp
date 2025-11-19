#include "common.hpp"
#include "shares.hpp"
#include "mpc_ops.hpp"
#include <boost/asio.hpp>
#include <iostream>

// Build twice with -DROLE=0 and -DROLE=1 to produce p0 and p1
#ifndef ROLE
#error "ROLE must be defined as 0 or 1"
#endif

int main(int argc, char* argv[]) {
    // ./pX num_users num_items dim num_queries dealer_host dealer_port
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << " num_users num_items dim num_queries dealer_host dealer_port\n";
        return 1;
    }
    int dim = std::stoi(argv[3]);
    int num_queries = std::stoi(argv[4]);
    std::string dealer_host = argv[5];
    std::string dealer_port = argv[6];

    int role = ROLE; // 0 or 1

    std::string share_path = "/workspace/output/share_p" + std::to_string(role) + "_0.txt";
    std::vector<int64> u_share, v_share;
    if (!read_shares(share_path, u_share, v_share)) {
        std::cerr << "Failed to read shares from " << share_path << "\n";
        return 1;
    }

    boost::asio::ip::tcp::iostream stream;
    stream.connect(dealer_host, dealer_port);
    if (!stream) {
        std::cerr << "Failed to connect to dealer " << dealer_host << ":" << dealer_port << "\n";
        return 1;
    }

    stream << "ROLE " << role << "\n" << std::flush;

    // For each query, perform protocol steps
    for (int q = 0; q < num_queries; ++q) {
        std::string line;
        std::getline(stream, line);
        if (line.rfind("DOT_A ",0) != 0) { std::cerr << "Protocol error: expected DOT_A\n"; return 1; }
        auto a_share = parse_vec(line.substr(6));
        std::getline(stream, line);
        if (line.rfind("DOT_B ",0) != 0) { std::cerr << "Protocol error: expected DOT_B\n"; return 1; }
        auto b_share = parse_vec(line.substr(6));
        std::getline(stream, line);
        if (line.rfind("DOT_C ",0) != 0) { std::cerr << "Protocol error: expected DOT_C\n"; return 1; }
        int64 c_share = std::stoll(line.substr(6));

        // Compute alpha and beta using addition-based masking (alpha = u_share + a_share)
        auto alpha = add_vec(u_share, a_share);
        auto beta = add_vec(v_share, b_share);

        // send alpha and beta as one line
        stream << "ALPHABETA " << join_vec(alpha) << " | " << join_vec(beta) << "\n" << std::flush;

        // receive other's ALPHABETA
        std::getline(stream, line);
        if (line.rfind("ALPHABETA ",0) != 0) { std::cerr << "Protocol error: expected ALPHABETA resp\n"; return 1; }
        auto pos = line.find(" | ");
        std::string alpha_other_s = line.substr(10, pos - 10);
        std::string beta_other_s = line.substr(pos + 3);
        auto alpha_other = parse_vec(alpha_other_s);
        auto beta_other = parse_vec(beta_other_s);

        for (size_t i = 0; i < alpha.size(); ++i) alpha[i] += (i < alpha_other.size() ? alpha_other[i] : 0);
        for (size_t i = 0; i < beta.size(); ++i) beta[i] += (i < beta_other.size() ? beta_other[i] : 0);

       
        int64 full_alpha_dot_beta = dot(alpha, beta);

        int64 r = c_share;
        r -= dot(alpha, b_share);
        r -= dot(beta, a_share);
        if (role == 0) r += full_alpha_dot_beta; 

        int64 dot_share = r;

        int64 delta_share = ((role == 0) ? 1 : 0) - dot_share;

        // Now receive vector-scalar triples for M = v * delta
        std::getline(stream, line);
        if (line.rfind("VSA_A ",0) != 0) { std::cerr << "Protocol error: expected VSA_A\n"; return 1; }
        auto a2_share = parse_vec(line.substr(6));
        std::getline(stream, line);
        if (line.rfind("VSA_B ",0) != 0) { std::cerr << "Protocol error: expected VSA_B\n"; return 1; }
        int64 b2_share = std::stoll(line.substr(6));
        std::getline(stream, line);
        if (line.rfind("VSA_C ",0) != 0) { std::cerr << "Protocol error: expected VSA_C\n"; return 1; }
        auto c2_share = parse_vec(line.substr(6));

        auto alpha_v = add_vec(v_share, a2_share);
        int64 beta_delta = delta_share + b2_share;

        stream << "ALPHAVBD " << join_vec(alpha_v) << " | " << beta_delta << "\n" << std::flush;

        std::getline(stream, line);
        if (line.rfind("ALPHAVBD ",0) != 0) { std::cerr << "Protocol error: expected ALPHAVBD resp\n"; return 1; }
        pos = line.find(" | ");
        std::string alpha_v_other_s = line.substr(9, pos - 9);
        std::string beta_delta_other_s = line.substr(pos + 3);
        auto alpha_v_other = parse_vec(alpha_v_other_s);
        int64 beta_delta_other = std::stoll(beta_delta_other_s);

        // reconstruct full alpha_v and beta_delta
        for (size_t i = 0; i < alpha_v.size(); ++i) alpha_v[i] += (i < alpha_v_other.size() ? alpha_v_other[i] : 0);
        int64 full_beta_delta = beta_delta + beta_delta_other;

        std::vector<int64> M_share(dim);
        for (int i = 0; i < dim; ++i) {
            int64 term = c2_share[i];
            term -= alpha_v[i] * b2_share;
            term -= full_beta_delta * a2_share[i];
            if (role == 0) term += alpha_v[i] * full_beta_delta;
            M_share[i] = term;
        }

        for (int i = 0; i < dim; ++i) u_share[i] += M_share[i];

        std::string out_path = "/workspace/output/updated_share_p" + std::to_string(role) + "_0.txt";
        write_shares(out_path, u_share, v_share);
        std::cout << "Wrote " << out_path << "\n";
    }

    return 0;
}