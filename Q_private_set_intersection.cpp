#include <cassert>
#include <cstdint>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <set>
#include <unistd.h>
#include <asio.hpp>
#include <NTL/lzz_p.h>
#include <NTL/lzz_pX.h>
#include "common.hpp"

using namespace std;
using namespace NTL;
using asio::ip::tcp;

int main(int argc, char *argv[])
{
    std::vector<string> IPaddrs;
    if (argc == 2)
    {
        ifstream file(argv[1]);
        string line;
        if (file.is_open())
        {
            while (getline(file, line)) {
                IPaddrs.push_back(line);
            }
            file.close();
        }
    }
    else
    {
        IPaddrs.push_back("127.0.0.1");
    }

    asio::io_context io_context;

    tcp::socket socket(io_context);
    bool connectionsuccess = false;
    size_t length;
    char *networkdata;
    char *parameters;
    char *vectorzzp;

    while (connectionsuccess == false)
    {
        try
        {
            socket = establishtcpconnection(IPaddrs[0].c_str(), "9864", io_context);

            networkdata = reinterpret_cast<char *>(malloc(1));
            networkdata[0] = 0;
            sendnetworkmessage(socket, networkdata, 1);
            free(networkdata);

            parameters = receivenetworkmessage(socket, &length);
            vectorzzp = receivenetworkmessage(socket, &length);
            socket.close();

            connectionsuccess = true;
        }
        catch (...)
        {
            sleep(0.1);
        }
    }

    unsigned long n, s, p, b;
    memcpy(&n, parameters + 0 * sizeof(unsigned long), sizeof(unsigned long));
    memcpy(&s, parameters + 1 * sizeof(unsigned long), sizeof(unsigned long));
    memcpy(&p, parameters + 2 * sizeof(unsigned long), sizeof(unsigned long));
    memcpy(&b, parameters + 3 * sizeof(unsigned long), sizeof(unsigned long));
    zz_p::init(p);
    free(parameters);

    if (argc != 2)
    {
        IPaddrs.push_back("127.0.0.1");
        IPaddrs.push_back("127.0.0.1");
    }

    vec_zz_p setintvec = deserializeveczzp(vectorzzp);
    free(vectorzzp);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<tcp::socket> sockets;
    for (int party = 1; party <= 2; party++)
    {
        connectionsuccess = false;
        while (connectionsuccess == false)
        {
            try
            {
                sockets.push_back(establishtcpconnection(IPaddrs[party].c_str(), to_string(9865 + party).c_str(), io_context));
                connectionsuccess = true;
            }
            catch(...)
            {
                sleep(0.1);
            }
        }
    }

    auto waiting_time_start = std::chrono::high_resolution_clock::now();

    char *T3_data = receivenetworkmessage(sockets[0], &length);
    auto waiting_time_stop = std::chrono::high_resolution_clock::now();

    uint64_t num_bins = length / 8;

    vector<unsigned long> T3_items(num_bins);
    vector<unsigned long> T3_hashes(num_bins);
    for (uint64_t i = 0; i < num_bins; i++)
    {
        T3_items[i] = static_cast<unsigned char>(T3_data[i * 8 + 0])
            | (static_cast<unsigned long>(static_cast<unsigned char>(T3_data[i * 8 + 1])) << 8)
            | (static_cast<unsigned long>(static_cast<unsigned char>(T3_data[i * 8 + 2])) << 16)
            | (static_cast<unsigned long>(static_cast<unsigned char>(T3_data[i * 8 + 3])) << 24);
        T3_hashes[i] = static_cast<unsigned char>(T3_data[i * 8 + 4])
            | (static_cast<unsigned long>(static_cast<unsigned char>(T3_data[i * 8 + 5])) << 8)
            | (static_cast<unsigned long>(static_cast<unsigned char>(T3_data[i * 8 + 6])) << 16)
            | (static_cast<unsigned long>(static_cast<unsigned char>(T3_data[i * 8 + 7])) << 24);
    }
    free(T3_data);

    char *T4_data = receivenetworkmessage(sockets[1], &length);
    sockets[0].close();
    sockets[1].close();

    uint64_t T4_bins = static_cast<unsigned char>(T4_data[0])
        | (static_cast<unsigned long>(static_cast<unsigned char>(T4_data[1])) << 8)
        | (static_cast<unsigned long>(static_cast<unsigned char>(T4_data[2])) << 16)
        | (static_cast<unsigned long>(static_cast<unsigned char>(T4_data[3])) << 24);

    assert(T4_bins == num_bins);

    uint64_t hash_len = 32;
    uint64_t offset = 4;

    auto decode_start = std::chrono::high_resolution_clock::now();

    vec_zz_p intersection;
    unsigned long universe_size = round(pow(2, b));

    for (uint64_t i = 0; i < num_bins; i++)
    {
        uint64_t bin_sz = static_cast<unsigned char>(T4_data[offset])
            | (static_cast<unsigned long>(static_cast<unsigned char>(T4_data[offset + 1])) << 8)
            | (static_cast<unsigned long>(static_cast<unsigned char>(T4_data[offset + 2])) << 16)
            | (static_cast<unsigned long>(static_cast<unsigned char>(T4_data[offset + 3])) << 24);
        offset += 4;

        for (uint64_t j = 0; j < bin_sz; j++)
        {
            unsigned char *oprf_val = reinterpret_cast<unsigned char *>(T4_data + offset);
            offset += hash_len;

            unsigned long decoded_item = T3_items[i] ^ (*(reinterpret_cast<uint64_t*>(oprf_val)) & 0xFFFFFFFF);
            unsigned long decoded_hash = T3_hashes[i] ^ ((*(reinterpret_cast<uint64_t*>(oprf_val + 8))) & 0xFFFFFFFF);

            unsigned long expected_hash = (decoded_item * 0x9e3779b97f4a7c15ULL) & 0xFFFFFFFF;

            if (decoded_hash == expected_hash && decoded_item < universe_size)
            {
                intersection.append(zz_p(decoded_item));
            }
        }
    }
    free(T4_data);

    auto decode_stop = std::chrono::high_resolution_clock::now();

    auto stop = std::chrono::high_resolution_clock::now();

    std::cout << "[Q] Checking correctness of result... " << std::endl;
    if (setintvec.length() != intersection.length())
    {
        std::cout << "[Q] Result is incorrect! Expected " << setintvec.length() << " items, got " << intersection.length() << endl;
        exit(1);
    }

    std::set<unsigned long> setint;
    for (int i = 0; i < setintvec.length(); i++)
    {
        setint.insert(conv<long>(setintvec[i]));
    }
    for (int i = 0; i < intersection.length(); i++)
    {
        if (setint.find(conv<long>(intersection[i])) == setint.end())
        {
            std::cout << "[Q] Result is incorrect! Unexpected item found." << endl;
            exit(1);
        }
    }

    std::cout << "[Q] Result is correct :)" << std::endl;
    std::cout << std::endl;

    std::cout << "[Q] n: " << n << std::endl;
    std::cout << "[Q] Intersection size: " << setintvec.length() << std::endl;
    std::cout << std::endl;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    auto waiting_duration = std::chrono::duration_cast<std::chrono::milliseconds>(waiting_time_stop - waiting_time_start);
    auto decode_duration = std::chrono::duration_cast<std::chrono::milliseconds>(decode_stop - decode_start);

    cout << "[Q] Total time: " << duration.count() << "ms" << endl;
    cout << "[Q] Waiting for other parties: " << waiting_duration.count() << "ms" << endl;
    cout << "[Q] Decoding: " << decode_duration.count() << "ms" << endl;

    return 0;
}
