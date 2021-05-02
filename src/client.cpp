#include <common.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

namespace file_transmitter {

enum {
  argv_binary_index,
  argv_address_index,
  argv_port_index,
  argv_path_index,
  expected_argc
};

using boost::asio::ip::tcp;

class client_exception : public std::runtime_error {
public:
  explicit client_exception(const std::string &msg) : std::runtime_error(msg) {}
};

// Workflow:
// client opens file,
// connects to the server,
// transfers file name|size in the header terminated by delimiter
// reads from the file into buffer and transfers through the socket
// Error handling:
// in case of any errors an exception is thrown
// and socket is automatically closed in the destructor
class client {
public:
  explicit client(char **argv) : socket_(io_context_) {
    const char *path = argv[argv_path_index];
    ifile_.open(path, std::ios_base::binary | std::ios_base::ate);

    if (!ifile_) {
      throw client_exception("Failed to open " + std::string(path));
    }

    const char *last_slash = std::strrchr(path, '/');
    const char *file_name = last_slash ? last_slash + 1 : path;
    const std::size_t file_size = ifile_.tellg();
    ifile_.seekg(SEEK_SET);

    std::ostream os(&header_);
    os << file_name << ' ' << file_size << DELIMITER;

    const tcp::resolver::results_type endpoints =
        tcp::resolver(io_context_)
            .resolve(argv[argv_address_index], argv[argv_port_index]);
    boost::asio::connect(socket_, endpoints);
    boost::asio::async_write(socket_, header_,
                             boost::bind(&client::handle_write_header, this,
                                         boost::asio::placeholders::error));
  }

  void run() { io_context_.run(); }

private:
  void handle_write_header(const boost::system::error_code &error) {
    if (error) {
      throw client_exception("Failed to write header");
    }

    start_write_buf();
  }

  void start_write_buf() {
    if (ifile_.eof()) {
      return;
    }

    ifile_.read(buf_, MAX_BUFFER_LEN);

    const std::streamsize bytes_read = ifile_.gcount();

    if (bytes_read > 0) {
      boost::asio::async_write(socket_, boost::asio::buffer(buf_, bytes_read),
                               boost::bind(&client::handle_write_buf, this,
                                           boost::asio::placeholders::error));
    } else {
      throw client_exception("Failed to read input buffer");
    }
  }

  void handle_write_buf(const boost::system::error_code &error) {
    if (error) {
      throw client_exception("Failed to write buf, error: " + error.message());
    }

    start_write_buf();
  }

  boost::asio::io_context io_context_;
  tcp::socket socket_;

  std::ifstream ifile_;
  boost::asio::streambuf header_;
  char buf_[MAX_BUFFER_LEN];
};

} // namespace file_transmitter

int main(int argc, char *argv[]) {
  if (argc != file_transmitter::expected_argc) {
    std::cerr << "Usage: " << argv[file_transmitter::argv_binary_index]
              << " <address> <port> <path>\n";
    return EXIT_FAILURE;
  }

  try {
    file_transmitter::client c(argv);
    c.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
