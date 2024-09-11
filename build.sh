#! /bin/bash
set -e

# retrieve and install log4cpp (required by libscapi)
wget https://sourceforge.net/projects/log4cpp/files/log4cpp-1.1.x%20%28new%29/log4cpp-1.1/log4cpp-1.1.4.tar.gz
tar -xvzf log4cpp-1.1.4.tar.gz
(cd log4cpp/; ./configure)
(cd log4cpp/; make && make check && sudo make install)
rm log4cpp-1.1.4.tar.gz

# build libscapi
(cd libscapi/; make libs libscapi)

# build relic
(cd relic/; cmake -DMULTI=PTHREAD . && make && sudo make install)

# build project
(cd scalable-mpc/build; cmake .. && make)
