#ifndef COMMON_H
#define COMMON_H

#include <asio.hpp>
#include <NTL/lzz_p.h>
#include <NTL/vec_lzz_p.h>

using namespace NTL;
using asio::ip::tcp;

class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
    public:
        typedef std::shared_ptr<tcp_connection> pointer;

        static pointer create(asio::io_context& io_context)
        {
            return pointer(new tcp_connection(io_context));
        }

        asio::ip::tcp::socket& socket()
        {
            return socket_;
        }

        void start()
        {
            asio::async_read(socket_, asio::buffer(this->message),
                bind(&tcp_connection::handle_read, shared_from_this(),
                  std::placeholders::_1,
                  std::placeholders::_2));
        }

        std::vector<char> message = std::vector<char>(10000000);
        size_t message_length;
        bool written = 0;

    private:
        tcp_connection(asio::io_context& io_context) : socket_(io_context) {}

        void handle_read(const asio::error_code& error, size_t bytes_transferred) {
            size_t data_length = 256 * 256 * 256 * static_cast<unsigned char>(this->message[0]) + 256 * 256 * static_cast<unsigned char>(this->message[1]) + 256 * static_cast<unsigned char>(this->message[2]) + static_cast<unsigned char>(this->message[3]);
            assert(bytes_transferred = data_length + 4);
            this->message_length = data_length;

            for (int i = 0; i < data_length; i++)
            {
                this->message[i] = this->message[i + 4];
            }
            this->message.resize(data_length);

            written = true;
        }

        asio::ip::tcp::socket socket_;
};

class tcp_server
{
    public:
        tcp_server(asio::io_context& io_context, unsigned int port) : port_(port), io_context_(io_context), acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
        {
            start_accept();
        }

        tcp_connection::pointer connection;

    private:
        void start_accept()
        {
            this->connection = tcp_connection::create(io_context_);

            acceptor_.async_accept(this->connection->socket(),
                bind(&tcp_server::handle_accept, this, this->connection,
                  std::placeholders::_1));
        }

        void handle_accept(tcp_connection::pointer new_connection, const asio::error_code& error)
        {
            if (!error)
            {
                new_connection->start();
            }
        }

        unsigned int port_;
        asio::io_context& io_context_;
        asio::ip::tcp::acceptor acceptor_;
};

tcp::socket waitfortcpconnection(unsigned int port, asio::io_context &asio_io_context)
{
    tcp::acceptor acceptor(asio_io_context, tcp::endpoint(tcp::v4(), port));
    tcp::socket socket(asio_io_context);
    acceptor.listen(0);
    acceptor.accept(socket);
    return socket;
}

tcp::socket establishtcpconnection(const char *server_ip, const char *port, asio::io_context &asio_io_context)
{
    tcp::resolver resolver(asio_io_context);
    tcp::resolver::query query(tcp::v4(), server_ip, port);
    tcp::resolver::results_type endpoints = resolver.resolve(query);
    tcp::socket socket(asio_io_context);
    asio::connect(socket, endpoints);
    return socket;
}

void sendnetworkmessage(tcp::socket &socket, const char *message, size_t length)
{
    assert(length < 256ull * 256ull * 256ull * 256ull);

    char *paddedmessage = reinterpret_cast<char *>(malloc(4 + length));
    paddedmessage[0] = length / 256 / 256 / 256;
    paddedmessage[1] = length / 256 / 256 % 256;
    paddedmessage[2] = length / 256 % 256;
    paddedmessage[3] = length % 256;
    if (message != nullptr)
    {
        memcpy(paddedmessage + 4, message, length);
    }

    std::string strmessage(paddedmessage, 4 + length);
    free(paddedmessage);

    asio::error_code ignored_error;
    asio::write(socket, asio::buffer(strmessage), ignored_error);
}

char *receivenetworkmessage(tcp::socket &socket, size_t *length)
{
    std::vector<char> buf(4);
    *length = read(socket, asio::buffer(buf));
    assert(*length == 4);

    size_t remaining_data_length = 256 * 256 * 256 * static_cast<unsigned char>(buf[0]) + 256 * 256 * static_cast<unsigned char>(buf[1]) + 256 * static_cast<unsigned char>(buf[2]) + static_cast<unsigned char>(buf[3]);

    buf.resize(remaining_data_length);
    *length = read(socket, asio::buffer(buf));
    assert(*length == remaining_data_length);

    char *data = reinterpret_cast<char *>(malloc(*length));
    if (*length != 0)
    {
        memcpy(data, buf.data(), *length);
    }

    return data;
}

char *serializeveczzp(vec_zz_p vectorzzp, size_t &length)
{
    size_t vectorlength = vectorzzp.length();
    length = sizeof(size_t) + vectorlength * sizeof(zz_p);
    char *serializeddata = reinterpret_cast<char *>(malloc(length));

    memcpy(serializeddata, &vectorlength, sizeof(size_t));
    for (int i = 0; i < vectorlength; i++)
    {
        memcpy(serializeddata + sizeof(size_t) + i * sizeof(zz_p), &vectorzzp[i], sizeof(zz_p));
    }

    return serializeddata;
}

vec_zz_p deserializeveczzp(char *serializeddata)
{
    vec_zz_p vectorzzp;

    size_t vectorlength;
    memcpy(&vectorlength, serializeddata, sizeof(size_t));
    vectorzzp.SetLength(vectorlength);

    for (int i = 0; i < vectorlength; i++)
    {
        memcpy(&vectorzzp[i], serializeddata + sizeof(size_t) + i * sizeof(zz_p), sizeof(zz_p));
    }

    return vectorzzp;
}

#endif
