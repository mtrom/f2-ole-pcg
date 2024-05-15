#! /bin/bash
set -e

# install specific version of cmake
wget https://cmake.org/files/v3.18/cmake-3.18.6.tar.gz
tar -xvzf cmake-3.18.6.tar.gz
(cd cmake-3.18.6; ./configure && make && sudo make install)
rm cmake-3.18.6.tar.gz

# retrieve and install log4cpp (required by libscapi)
wget https://sourceforge.net/projects/log4cpp/files/log4cpp-1.1.x%20%28new%29/log4cpp-1.1/log4cpp-1.1.4.tar.gz
tar -xvzf log4cpp-1.1.4.tar.gz
(cd log4cpp/; ./configure)
(cd log4cpp/; make && make check && sudo make install)
rm log4cpp-1.1.4.tar.gz

# build libscapi
(cd libscapi/; make)
