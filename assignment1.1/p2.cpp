#include "common.hpp"
#include "mpc_ops.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <random>
#include <thread>
#include <filesystem>

using boost::asio::ip::tcp;

std::string read_line(tcp::socket &sock) {
    boost::asio::streambuf buf;
    boost::asio::read_until(sock, buf, '\n');
    std::istream is(&buf);
    std::string line;
    std::getline(is, line);
    return line;
}

void write_line(tcp::socket &sock, const std::string &s) {
    std::string out = s + "\n";
    boost::asio::write(sock, boost::asio::buffer(out));
}

int main(int argc, char* argv[]) {
    // ./p2 num_users num_items dim
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " num_users num_items dim\n";
        return 1;
    }
    int dim = std::stoi(argv[3]);

    std::filesystem::create_directory("/workspace/output");

    boost::asio::io_context ctx;
    tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 9002));

    std::cout << "Dealer listening on port 9002...\n";

    tcp::socket sock1(ctx), sock2(ctx);
    acceptor.accept(sock1);
    std::string reg1 = read_line(sock1);
    std::cout << "Got: " << reg1 << "\n";
    acceptor.accept(sock2);
    std::string reg2 = read_line(sock2);
    std::cout << "Got: " << reg2 << "\n";

    // map roles to sockets
    tcp::socket *sockets[2] = {nullptr, nullptr};
    auto assign_role = [&](const std::string &reg, tcp::socket &sock){
        if (reg.rfind("ROLE ",0) == 0) {
            int r = std::stoi(reg.substr(5));
            if (r >=0 && r <=1) sockets[r] = &sock;
        }
    };
    assign_role(reg1, sock1);
    assign_role(reg2, sock2);

    if (!sockets[0] || !sockets[1]) {
        std::cerr << "Both roles not connected correctly\n";
        return 1;
    }


    std::mt19937_64 rng((unsigned)98765);
    std::uniform_int_distribution<int64> dist(-5,5);

    // DOT triples (vector a, vector b, scalar c = dot(a,b))
    std::vector<int64> a(dim), b(dim);
    for (int i = 0; i < dim; ++i) { a[i] = dist(rng); b[i] = dist(rng); }
    int64 c = dot(a,b);

    // split into shares
    std::vector<int64> a0(dim), a1(dim), b0(dim), b1(dim), c0_vec(1), c1_vec(1);
    for (int i = 0; i < dim; ++i) {
        a0[i] = dist(rng);
        a1[i] = a[i] - a0[i];
        b0[i] = dist(rng);
        b1[i] = b[i] - b0[i];
    }
    int64 c0 = dist(rng);
    int64 c1 = c - c0;

    // send to party 0
    write_line(*sockets[0], std::string("DOT_A ") + join_vec(a0));
    write_line(*sockets[0], std::string("DOT_B ") + join_vec(b0));
    write_line(*sockets[0], std::string("DOT_C ") + std::to_string(c0));

    // send to party 1
    write_line(*sockets[1], std::string("DOT_A ") + join_vec(a1));
    write_line(*sockets[1], std::string("DOT_B ") + join_vec(b1));
    write_line(*sockets[1], std::string("DOT_C ") + std::to_string(c1));

    // receive ALPHABETA from both parties, then forward
    auto l0 = read_line(*sockets[0]);
    auto l1 = read_line(*sockets[1]);
    // forward
    write_line(*sockets[0], l1);
    write_line(*sockets[1], l0);

    // Now prepare vector-scalar triples for M = v * delta
    std::vector<int64> a2(dim);
    for (int i = 0; i < dim; ++i) a2[i] = dist(rng);
    int64 b2 = dist(rng); // scalar
    std::vector<int64> c2(dim);
    for (int i = 0; i < dim; ++i) c2[i] = a2[i] * b2;

    // split shares for a2 and c2
    std::vector<int64> a20(dim), a21(dim), c20(dim), c21(dim);
    for (int i = 0; i < dim; ++i) {
        a20[i] = dist(rng);
        a21[i] = a2[i] - a20[i];
        c20[i] = dist(rng);
        c21[i] = c2[i] - c20[i];
    }
    int64 b20 = dist(rng);
    int64 b21 = b2 - b20;

    write_line(*sockets[0], std::string("VSA_A ") + join_vec(a20));
    write_line(*sockets[0], std::string("VSA_B ") + std::to_string(b20));
    write_line(*sockets[0], std::string("VSA_C ") + join_vec(c20));

    write_line(*sockets[1], std::string("VSA_A ") + join_vec(a21));
    write_line(*sockets[1], std::string("VSA_B ") + std::to_string(b21));
    write_line(*sockets[1], std::string("VSA_C ") + join_vec(c21));

    auto v0 = read_line(*sockets[0]);
    auto v1 = read_line(*sockets[1]);
    write_line(*sockets[0], v1);
    write_line(*sockets[1], v0);

    std::cout << "Dealer finished one query and forwarded messages.\n";

    return 0;
}
