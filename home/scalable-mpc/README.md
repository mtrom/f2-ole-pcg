# Brown MPC Library

This is a library for MPC at Brown University, created by Nick Young in collaboration with Peihan Miao, Akshayaram Srinivasan, Max Tromanhauser, and Alice Nancy Lulu Murphy. This package builds off of `libscapi`, as well as a few other packages including Boost, CryptoPP, and more.

## Running and Building

To build this code, first run `git submodule update --init --recursive` to ensure that all submodules are initialized and pulled, then run `cmake ..` from the `build/` directory. To run it, run `make` from the `build/` directory then run the appropriate binary. You may have to update `CMakeLists.txt` if you want to add more binaries or source files.

You should run everything in the [devenv made for this repo](https://github.com/BrownAppliedCryptography/devenv/tree/libscapi).
