#include <cassert>
#include <vector>
#include <numeric>
#include <random>
#include <set>
#include <asio.hpp>
#include <NTL/lzz_p.h>
#include <NTL/vec_lzz_p.h>
#include "common.hpp"

using namespace std;
using namespace NTL;
using asio::ip::tcp;

void generaterandomelement(zz_p &randomelement, unsigned long p, unsigned long b)
{
    unsigned long universe_size = round(pow(2, b));

    random_device truerandomnumbergenerator;
    mt19937 generator(truerandomnumbergenerator());
    uniform_int_distribution<unsigned long> distribution(0, universe_size - 1);

    randomelement = distribution(generator);
}

int main(int argc, char *argv[])
{
    unsigned long n, s;
    if (argc != 3 || atoi(argv[1]) <= 0 || atoi(argv[2]) < 0 || atoi(argv[1]) < atoi(argv[2]))
    {
        n = 500;
        s = 250;
    }
    else
    {
        n = atoi(argv[1]);
        s = atoi(argv[2]);
    }

    unsigned long p = 180143985094819841;
    unsigned long b = 32;
    unsigned long lambda = 40;

    zz_p::init(p);

    unsigned long min_required_size = round(pow(2, lambda));
    assert(p >= min_required_size);

    cout << "[G] Generating random sets (n=" << n << ", s=" << s << ")..." << endl;

    vec_zz_p X, Y;
    X.SetLength(n);
    Y.SetLength(n);

    zz_p randomelement;
    std::set<unsigned long> currentelements;

    for (int i = 0; i < n; i++)
    {
        generaterandomelement(randomelement, p, b);
        while (currentelements.find(conv<long>(randomelement)) != currentelements.end())
        {
            generaterandomelement(randomelement, p, b);
        }
        X[i] = randomelement;
        currentelements.insert(conv<long>(randomelement));
    }

    std::vector<int> X_indices(n);
    std::iota(std::begin(X_indices), std::end(X_indices), 0);
    std::vector<int> Y_indices(X_indices);
    std::random_shuffle(X_indices.begin(), X_indices.end());
    std::random_shuffle(Y_indices.begin(), Y_indices.end());

    currentelements.clear();
    for (int i = 0; i < n; i++)
    {
        if (i < s)
        {
            Y[Y_indices[i]] = X[X_indices[i]];
            currentelements.insert(conv<long>(X[X_indices[i]]));
        }
        else
        {
            generaterandomelement(randomelement, p, b);
            while (currentelements.find(conv<long>(randomelement)) != currentelements.end())
            {
                generaterandomelement(randomelement, p, b);
            }
            Y[Y_indices[i]] = randomelement;
            currentelements.insert(conv<long>(randomelement));
        }
    }

    cout << "[G] Random set generation complete" << endl;

    std::set<unsigned long> setX, setY;
    for (int i = 0; i < n; i++)
    {
        setX.insert(conv<long>(X[i]));
        setY.insert(conv<long>(Y[i]));
    }

    vec_zz_p setint;
    for (int i = 0; i < n; i++)
    {
        if (setY.find(conv<long>(X[i])) != setY.end())
        {
            setint.append(X[i]);
        }
    }

    asio::io_context io_context;

    cout << "[G] Sending sets to P1 and P2..." << endl;

    for (int party = 1; party <= 2; party++)
    {
        tcp::socket socket = waitfortcpconnection(9865, io_context);

        size_t length;
        char *networkdata = receivenetworkmessage(socket, &length);
        assert(length == 1);
        char partyid = networkdata[0];
        assert(partyid == 1 || partyid == 2);
        free(networkdata);

        networkdata = reinterpret_cast<char *>(malloc(4 * sizeof(unsigned long)));
        memcpy(networkdata + 0 * sizeof(unsigned long), &n, sizeof(unsigned long));
        memcpy(networkdata + 1 * sizeof(unsigned long), &s, sizeof(unsigned long));
        memcpy(networkdata + 2 * sizeof(unsigned long), &p, sizeof(unsigned long));
        memcpy(networkdata + 3 * sizeof(unsigned long), &b, sizeof(unsigned long));
        sendnetworkmessage(socket, networkdata, 4 * sizeof(unsigned long));
        free(networkdata);

        if (partyid == 1)
        {
            networkdata = serializeveczzp(X, length);
        }
        else
        {
            networkdata = serializeveczzp(Y, length);
        }
        sendnetworkmessage(socket, networkdata, length);
        free(networkdata);
    }

    tcp::socket socket = waitfortcpconnection(9864, io_context);

    size_t length;
    char *networkdata = receivenetworkmessage(socket, &length);
    assert(length == 1);
    char partyid = networkdata[0];
    assert(partyid == 0);
    free(networkdata);

    networkdata = reinterpret_cast<char *>(malloc(4 * sizeof(unsigned long)));
    memcpy(networkdata + 0 * sizeof(unsigned long), &n, sizeof(unsigned long));
    memcpy(networkdata + 1 * sizeof(unsigned long), &s, sizeof(unsigned long));
    memcpy(networkdata + 2 * sizeof(unsigned long), &p, sizeof(unsigned long));
    memcpy(networkdata + 3 * sizeof(unsigned long), &b, sizeof(unsigned long));
    sendnetworkmessage(socket, networkdata, 4 * sizeof(unsigned long));
    free(networkdata);

    networkdata = serializeveczzp(setint, length);
    sendnetworkmessage(socket, networkdata, length);
    free(networkdata);

    cout << "[G] Sets sent. Intersection size: " << setint.length() << endl;

    return 0;
}
