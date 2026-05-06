#include <cassert>
#include <cmath>
#include <chrono>
#include <fstream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <asio.hpp>
#include <NTL/lzz_p.h>
#include <NTL/lzz_pX.h>
#include "common.hpp"
#include "simple_hashing.hpp"
#include "oprf/oprf.hpp"

using namespace std;
using namespace NTL;
using asio::ip::tcp;

void zzptoblock(block &blk, const zz_p &item)
{
    memset(&blk, 0, sizeof(block));
    unsigned long val = conv<unsigned long>(item);
    memcpy(&blk, &val, sizeof(unsigned long));
}

int main(int argc, char *argv[])
{
    unsigned long k = 2;

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
            socket = establishtcpconnection(IPaddrs[0].c_str(), "9865", io_context);

            networkdata = reinterpret_cast<char *>(malloc(1));
            networkdata[0] = k;
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

    vec_zz_p Y = deserializeveczzp(vectorzzp);
    free(vectorzzp);

    socket = waitfortcpconnection(9865 + k, io_context);

    auto start = std::chrono::high_resolution_clock::now();

    double epsilon = 1.2;
    u64 gamma = 3;
    u64 num_bins = static_cast<u64>(epsilon * n);
    if (num_bins < 1) num_bins = 1;

    SimpleHashing simple(num_bins, gamma);
    for (u64 i = 0; i < n; i++)
    {
        simple.insert(Y[i]);
    }

    zz_p dummy_item = zz_p(round(pow(2, b)) + 2);
    simple.pad_empty_bins(dummy_item);

    u64 b_bins = simple.num_bins();

    u64 hashLengthInBytes = 32;

    u64 max_bin_size = 0;
    for (u64 i = 0; i < b_bins; i++)
    {
        if (simple.bin_size(i) > max_bin_size)
            max_bin_size = simple.bin_size(i);
    }

    u8 *sender_outputs = reinterpret_cast<u8 *>(calloc(b_bins * max_bin_size, hashLengthInBytes));
    u64 *sender_data_totals = reinterpret_cast<u64 *>(malloc(b_bins * sizeof(u64)));
    std::vector<std::thread> sender_threads;

    auto oprf_start = std::chrono::high_resolution_clock::now();

    for (u64 i = 0; i < b_bins; i++)
    {
        u64 bin_sz = simple.bin_size(i);
        vector<block> sender_set(bin_sz);
        for (u64 j = 0; j < bin_sz; j++)
        {
            zzptoblock(sender_set[j], simple.get_bin(i)[j]);
        }

        u64 senderSize = bin_sz;
        u64 receiverSize = 1;

        sender_threads.push_back(std::thread(runSender,
            IPaddrs[1] + ":" + std::to_string(8236 + i),
            "oprf" + std::to_string(8136 + i),
            senderSize, receiverSize,
            sender_set, hashLengthInBytes,
            sender_outputs + i * max_bin_size * hashLengthInBytes,
            &sender_data_totals[i],
            1000 + i, 2000 + i));
    }

    for (u64 i = 0; i < b_bins; i++)
    {
        sender_threads[i].join();
    }

    auto oprf_stop = std::chrono::high_resolution_clock::now();

    u64 sender_data_total = 0;
    for (u64 i = 0; i < b_bins; i++)
    {
        sender_data_total += sender_data_totals[i];
    }

    vector<unsigned char> T4_serialized;
    T4_serialized.push_back(static_cast<unsigned char>(b_bins & 0xFF));
    T4_serialized.push_back(static_cast<unsigned char>((b_bins >> 8) & 0xFF));
    T4_serialized.push_back(static_cast<unsigned char>((b_bins >> 16) & 0xFF));
    T4_serialized.push_back(static_cast<unsigned char>((b_bins >> 24) & 0xFF));

    for (u64 i = 0; i < b_bins; i++)
    {
        u64 bin_sz = simple.bin_size(i);
        T4_serialized.push_back(static_cast<unsigned char>(bin_sz & 0xFF));
        T4_serialized.push_back(static_cast<unsigned char>((bin_sz >> 8) & 0xFF));
        T4_serialized.push_back(static_cast<unsigned char>((bin_sz >> 16) & 0xFF));
        T4_serialized.push_back(static_cast<unsigned char>((bin_sz >> 24) & 0xFF));

        for (u64 j = 0; j < bin_sz; j++)
        {
            u8 *oprf_out = sender_outputs + i * max_bin_size * hashLengthInBytes + j * hashLengthInBytes;
            for (u64 byte_idx = 0; byte_idx < hashLengthInBytes; byte_idx++)
            {
                T4_serialized.push_back(oprf_out[byte_idx]);
            }
        }
    }
    free(sender_outputs);
    free(sender_data_totals);

    sendnetworkmessage(socket, reinterpret_cast<const char *>(T4_serialized.data()), T4_serialized.size());
    socket.close();

    auto stop = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    auto oprf_duration = std::chrono::duration_cast<std::chrono::milliseconds>(oprf_stop - oprf_start);

    cout << "[P2] Total time: " << duration.count() << "ms" << endl;
    cout << "[P2] OPRF evaluation: " << oprf_duration.count() << "ms" << endl;
    cout << "[P2] Communication as OPRF sender: " << sender_data_total / pow(2.0, 20) << "MB" << endl;
    cout << "[P2] Simple hash bins: " << b_bins << ", max bin size: " << max_bin_size << endl;

    return 0;
}
