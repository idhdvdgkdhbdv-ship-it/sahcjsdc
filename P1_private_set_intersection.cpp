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
#include "cuckoo_hashing.hpp"
#include "oprf/oprf.hpp"

using namespace std;
using namespace NTL;
using asio::ip::tcp;

void zzptoblock(block &b, const zz_p &item)
{
    memset(&b, 0, sizeof(block));
    unsigned long val = conv<unsigned long>(item);
    memcpy(&b, &val, sizeof(unsigned long));
}

int main(int argc, char *argv[])
{
    unsigned long k = 1;

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

    vec_zz_p X = deserializeveczzp(vectorzzp);
    free(vectorzzp);

    socket = waitfortcpconnection(9865 + k, io_context);

    auto start = std::chrono::high_resolution_clock::now();

    double epsilon = 1.2;
    u64 gamma = 3;
    u64 num_bins = static_cast<u64>(epsilon * n);
    if (num_bins < 1) num_bins = 1;

    CuckooHashing cuckoo(num_bins, gamma);
    for (u64 i = 0; i < n; i++)
    {
        bool ok = cuckoo.insert(X[i]);
        assert(ok);
    }

    zz_p dummy_item = zz_p(round(pow(2, b)) + 1);
    cuckoo.pad_empty_bins(dummy_item);

    u64 b_bins = cuckoo.num_bins();

    u64 hashLengthInBytes = 32;

    u8 *receiver_outputs = reinterpret_cast<u8 *>(calloc(b_bins, hashLengthInBytes));
    u64 *receiver_data_totals = reinterpret_cast<u64 *>(malloc(b_bins * sizeof(u64)));
    std::vector<std::thread> receiver_threads;

    auto oprf_start = std::chrono::high_resolution_clock::now();

    for (u64 i = 0; i < b_bins; i++)
    {
        vector<block> receiver_set(1);
        zzptoblock(receiver_set[0], cuckoo.get_bin(i).item);

        u64 sender_bin_size = 1;
        u64 senderSize = sender_bin_size;
        u64 receiverSize = 1;

        receiver_threads.push_back(std::thread(runReceiver,
            IPaddrs[1] + ":" + std::to_string(8236 + i),
            "oprf" + std::to_string(8136 + i),
            senderSize, receiverSize,
            receiver_set, hashLengthInBytes,
            receiver_outputs + i * hashLengthInBytes,
            &receiver_data_totals[i],
            1000 + i, 2000 + i));
    }

    for (u64 i = 0; i < b_bins; i++)
    {
        receiver_threads[i].join();
    }

    auto oprf_stop = std::chrono::high_resolution_clock::now();

    u64 receiver_data_total = 0;
    for (u64 i = 0; i < b_bins; i++)
    {
        receiver_data_total += receiver_data_totals[i];
    }
    free(receiver_data_totals);

    vector<unsigned char> T3_serialized;
    for (u64 i = 0; i < b_bins; i++)
    {
        const CuckooHashEntry &entry = cuckoo.get_bin(i);
        unsigned long item_val = conv<unsigned long>(entry.item);

        unsigned long hash_val = (item_val * 0x9e3779b97f4a7c15ULL) & 0xFFFFFFFF;

        u8 *oprf_out = receiver_outputs + i * hashLengthInBytes;

        unsigned long encoded_item = item_val ^ (*(reinterpret_cast<u64*>(oprf_out)) & 0xFFFFFFFF);
        unsigned long encoded_hash = hash_val ^ ((*(reinterpret_cast<u64*>(oprf_out + 8))) & 0xFFFFFFFF);

        T3_serialized.push_back(static_cast<unsigned char>(encoded_item & 0xFF));
        T3_serialized.push_back(static_cast<unsigned char>((encoded_item >> 8) & 0xFF));
        T3_serialized.push_back(static_cast<unsigned char>((encoded_item >> 16) & 0xFF));
        T3_serialized.push_back(static_cast<unsigned char>((encoded_item >> 24) & 0xFF));
        T3_serialized.push_back(static_cast<unsigned char>(encoded_hash & 0xFF));
        T3_serialized.push_back(static_cast<unsigned char>((encoded_hash >> 8) & 0xFF));
        T3_serialized.push_back(static_cast<unsigned char>((encoded_hash >> 16) & 0xFF));
        T3_serialized.push_back(static_cast<unsigned char>((encoded_hash >> 24) & 0xFF));
    }
    free(receiver_outputs);

    sendnetworkmessage(socket, reinterpret_cast<const char *>(T3_serialized.data()), T3_serialized.size());
    socket.close();

    auto stop = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    auto oprf_duration = std::chrono::duration_cast<std::chrono::milliseconds>(oprf_stop - oprf_start);

    cout << "[P1] Total time: " << duration.count() << "ms" << endl;
    cout << "[P1] OPRF evaluation: " << oprf_duration.count() << "ms" << endl;
    cout << "[P1] Communication as OPRF receiver: " << receiver_data_total / pow(2.0, 20) << "MB" << endl;
    cout << "[P1] Cuckoo hash bins: " << b_bins << endl;

    return 0;
}
