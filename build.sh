#! /bin/bash
set -e

# build relic
(cd relic/; cmake -DMULTI=PTHREAD . && make && sudo make install)

# build project
(cd primal-dual-pcg/build; cmake .. && make)
