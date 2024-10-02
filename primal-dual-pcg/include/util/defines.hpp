#pragma once

#include <thread>
#include <chrono>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

using tcp = boost::asio::ip::tcp;

#define COMM_SLEEP 500
#define COMM_TIMEOUT 5000

class TCP {
public:
  TCP(boost::asio::io_service& ios, boost::asio::ip::address host, int inport, int outport = -1)
    : client_ios(ios), server_ios(ios), acceptor(ios, tcp::endpoint(tcp::v4(), inport)),
      client(ios), server(ios), host_(host), port(outport == -1 ? inport : outport) { }

	void join() {
    int slept = 0;
    bool accepted = false, connected = false;

    while (!connected || !accepted) {
      // try connecting over the client socket to the other host
      try {
        if (!connected) {
          tcp::resolver resolver(this->client_ios);
          tcp::resolver::query query(this->host_.to_string(), std::to_string(this->port));
          tcp::resolver::iterator iterator = resolver.resolve(query);
          boost::asio::connect(this->client, iterator);
          connected = true;
        }
      }
      catch (const boost::system::system_error& ex) {
        if (slept > COMM_TIMEOUT) {
          std::cerr << "[TCP] failed to connect to host " << this->host_.to_string() << std::endl;
          throw ex;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(COMM_SLEEP));
        slept += COMM_SLEEP;
      }

      // try accepting the other host's connection over our server socket
      if (!accepted) {
        boost::system::error_code ec;
        this->acceptor.accept(this->server, ec);
        accepted = true;
      }
    }

    // set options for our sockets
    boost::asio::ip::tcp::no_delay option(true);
    this->client.set_option(option);
    this->server.set_option(option);
  }

  size_t write(const uint8_t* data, int size) {
    boost::system::error_code ec;
    this->upload_ += size;
    return boost::asio::write(
      this->client, boost::asio::buffer(data, size), boost::asio::transfer_all(), ec
    );
  }

	size_t read(uint8_t* data, int size) {
    this->download_ += size;
		return boost::asio::read(this->server, boost::asio::buffer(data, size));
  }

  size_t upload() { return upload_; }
  size_t download() { return download_; }
  boost::asio::ip::address host() { return host_; }
protected:
  boost::asio::io_service& client_ios;
  boost::asio::io_service& server_ios;
  tcp::acceptor acceptor;
  tcp::socket client;
  tcp::socket server;

  size_t upload_, download_;
  boost::asio::ip::address host_;
  int port;
};

// for ease of use
typedef std::shared_ptr<TCP> Channel;

// security parameter
#define LAMBDA 128
