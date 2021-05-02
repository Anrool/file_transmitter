# What
TCP client|server applications for file transmission.

# Requirements
Boost.Asio

# CMake build workflow
```
mkdir <build_dir>
cd <build_dir>
cmake <source_dir> -DBOOST_ROOT=<boost_install_prefix>
cmake --build <build_dir>
```

# Test
Launch server application:
```
./server <port>
```
Launch as many client instances as you wish in the following fashion:
```
./client <address> <port> <path_to_file>
```

Several file examples are located in the "input" folder.
