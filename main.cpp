#include <boost/asio.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <pthread.h>
#include "spdlog/sinks/basic_file_sink.h"
std::shared_ptr<spdlog::logger> my_logger = nullptr;


namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

void fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

Strand* g_ioc = nullptr;
int g_enter = 0;
class session : public std::enable_shared_from_this<session>
{
    public:
        net::io_context& m_ioc;
        net::strand<net::io_context::executor_type> m_strand;
        session(net::io_context& ioc, tcp::socket socket): 
            m_ioc(ioc),
            m_socket(std::move(socket)), 
            m_strand(net::make_strand(ioc)) {
                my_logger->info("+session()");
            }

        void start() {
            net::post(m_socket.get_executor(), boost::beast::bind_front_handler(&session::onPost, shared_from_this()));
            do_read();
        }
        ~session() {
            my_logger->info("~session()");
        }

    private:
        int m_check = 0;
        void onPost() {
            m_check++;
            if(m_check != 1) {
                my_logger->info("post shit!!1");
            }
            my_logger->info("onPost");
            net::post(m_socket.get_executor(), boost::beast::bind_front_handler(&session::onPost, shared_from_this()));
            m_check--;
        }
        void do_read() {
            auto self(shared_from_this());
            m_socket.async_read_some(boost::asio::buffer(m_data, max_length),
                                    // net::bind_executor(m_strand, 
                                    [this, self](boost::system::error_code ec, std::size_t length) {

                                        my_logger->info("{{{on read");
                                        g_enter++;
                                        if(g_enter != 1) {
                                            my_logger->info("g_enter shit!!1");
                                        }
                                        m_check++;
                                        if(m_check != 1) {
                                            my_logger->info("shit!!1");
                                        }
                                        if (ec) {
                                            return;
                                        }
                                        do_write(length);
                                        m_check--;
                                        g_enter--;
                                        my_logger->info("on read}}}");
                                    }
                                   );
        }

        void do_write(std::size_t length) {
            auto self(shared_from_this());
            net::async_write(m_socket, boost::asio::buffer(m_data, length),
                             [this, self](boost::system::error_code ec, std::size_t) {
                                 my_logger->info("{{{on write");
                                 g_enter++;
                                 if(g_enter != 1) {
                                     my_logger->info("g_enter!!1");
                                 }
                                 m_check++;
                                 if(m_check != 1) {
                                     my_logger->info("shit!!2");
                                 }
                                 if (!ec) {
                                     do_read();
                                 }
                                 m_check--;
                                 g_enter--;
                                 my_logger->info("on write}}}");
                             });
        }

        tcp::socket m_socket;
        enum { max_length = 1024 };
        char m_data[max_length];
};

class listener : public std::enable_shared_from_this<listener> { 
    public:
        net::io_context& m_ioc;
        tcp::acceptor m_acceptor;
        listener( net::io_context& ioc, tcp::endpoint endpoint)
            : m_ioc(ioc) , m_acceptor(ioc) {
                boost::system::error_code ec; 
                m_acceptor.open(endpoint.protocol(), ec);
                if(ec) {
                    fail(ec, "open");
                    return;
                }
                m_acceptor.set_option(net::socket_base::reuse_address(true), ec);
                if(ec) {
                    fail(ec, "set_option");
                    return;
                }
                m_acceptor.bind(endpoint, ec);
                if(ec) {
                    fail(ec, "bind");
                    return;
                }
                m_acceptor.listen(net::socket_base::max_listen_connections, ec);
                if(ec) {
                    fail(ec, "listen");
                    return;
                }
            }
        void run() {
            do_accept();
        }

    private:
        void do_accept() {
            m_acceptor.async_accept( net::make_strand(m_ioc),
                                   boost::beast::bind_front_handler( &listener::on_accept, shared_from_this() 
                                            ));
            // m_acceptor.async_accept( m_ioc,
            // boost::bind_front_handler( &listener::on_accept, shared_from_this()));
        }

        void on_accept(boost::system::error_code ec, tcp::socket socket) {
            if(ec) {
                fail(ec, "accept");
            } else {
                auto t = socket.remote_endpoint().address().to_string();
                boost::asio::ip::tcp::no_delay option(true);
                socket.set_option(option);

                std::make_shared<session>(m_ioc, std::move(socket))->start();
            }
            do_accept();
        }
};


int main(int argc, char* argv[]) { 
    if(argc < 2) {
        std::cout << "neo-bridge-cpp [thread number]\n";
        return -1;
    }
    my_logger = spdlog::basic_logger_mt("basic_logger", "basic-log.txt");
    my_logger->flush_on(spdlog::level::debug);
    my_logger->set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");

    printf("start program\n");

    my_logger->info("start program");
    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(10010); 
    auto const sslPort = static_cast<unsigned short>(10011); 
    auto const threads = atoi(argv[1]); 

    net::io_context ioc{threads};
    auto globalStrand = net::make_strand(ioc);
    g_ioc = &globalStrand;
    std::make_shared<listener>(ioc, tcp::endpoint{address, port})->run();
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
            [&ioc]
            {
                ioc.run();
            });
    ioc.run();

    return EXIT_SUCCESS;
}
