#include <common.h>

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

namespace file_transmitter {

enum { argv_binary_index, argv_port_index, expected_argc };

using boost::asio::ip::tcp;

// reads header(file name|size) terminated by DELIMITER
// and creates the file with the identical name|content
// NOTE: ownership is passed to the current completion handler
// Error handling:
// in case of any errors no other completion handler will be called,
// so that connection will be automatically destroyed by the end of the function
class connection : public boost::enable_shared_from_this<connection> {
public:
  typedef boost::shared_ptr<connection> ptr;

  explicit connection(boost::asio::io_context &io_context)
      : socket_(io_context), bytes_to_read_(0) {}

  tcp::socket &socket() { return socket_; }

  void start() {
    boost::asio::async_read_until(
        socket_, header_, DELIMITER,
        boost::bind(&connection::handle_read_header, shared_from_this(),
                    boost::asio::placeholders::error));
  }

private:
  void handle_read_header(const boost::system::error_code &error) {
    if (error) {
      std::cerr << "Failed to read header, error " << error.message()
                << std::endl;
      return;
    }

    std::string file_name;
    std::istream is(&header_);
    is >> file_name >> bytes_to_read_;

    if (!is) {
      std::cerr << "Wrong header\n";
      return;
    }

    ofile_.open(file_name.c_str(), std::ios_base::binary);

    if (!ofile_) {
      std::cerr << "Failed to open " << file_name << std::endl;
      return;
    }

    // discard DELIMITER
    is.ignore(1);

    // async_read_until can read additional data beyond the delimiter
    if (header_.size() > 0) {
      bytes_to_read_ -= header_.size();
      ofile_ << is.rdbuf();
    }

    start_read_buf();
  }

  void start_read_buf() {
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(buf_, std::min(bytes_to_read_, MAX_BUFFER_LEN)),
        boost::bind(&connection::handle_read_buf, shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }

  void handle_read_buf(const boost::system::error_code &error,
                       std::size_t length) {
    if (length > 0) {
      bytes_to_read_ -= length;
      ofile_.write(buf_, length);

      start_read_buf();
      return;
    }

    if (error == boost::asio::error::eof) {
      std::cout << "EOF, "
                << (bytes_to_read_ == length ? "success\n" : "failure\n");
    } else if (error) {
      std::cout << "Failed to read buffer, error: " << error.message()
                << std::endl;
    }
  }

  tcp::socket socket_;

  std::ofstream ofile_;
  boost::asio::streambuf header_;
  char buf_[MAX_BUFFER_LEN];
  std::size_t bytes_to_read_;
};

// listens the specified port awaiting for incoming connections
// in case of successful connection launches it,
// otherwise connection is automatically destroyed by the end of the function
class server {
public:
  explicit server(unsigned short port)
      : acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)) {
    start_accept();
  }

  void run() { io_context_.run(); }

private:
  void start_accept() {
    connection::ptr new_connection(new connection(io_context_));
    acceptor_.async_accept(new_connection->socket(),
                           boost::bind(&server::handle_accept, this,
                                       boost::asio::placeholders::error,
                                       new_connection));
  }

  void handle_accept(const boost::system::error_code &error,
                     connection::ptr connection) {
    if (!error) {
      connection->start();
    }

    start_accept();
  }

  boost::asio::io_context io_context_;
  tcp::acceptor acceptor_;
};

} // namespace file_transmitter

int main(int argc, char *argv[]) {
  if (argc != file_transmitter::expected_argc) {
    std::cerr << "Usage: " << argv[file_transmitter::argv_binary_index]
              << " <port>\n";
    return EXIT_FAILURE;
  }

  const char *port_str = argv[file_transmitter::argv_port_index];
  char *endptr = NULL;
  errno = 0;
  const unsigned long port = std::strtoul(port_str, &endptr, 10);

  if (errno || *endptr != '\0' ||
      port > std::numeric_limits<unsigned short>::max()) {
    std::cerr << "Bad port: " << port_str << std::endl;
    return EXIT_FAILURE;
  }

  try {
    file_transmitter::server s(static_cast<unsigned short>(port));
    s.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
