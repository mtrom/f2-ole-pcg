FROM ubuntu:focal

# set environment variables for tzdata
ARG TZ=America/New_York
ENV TZ=${TZ}

# set higher timeouts to get through the build
RUN echo "Acquire::http::Timeout \"10\";" > /etc/apt/apt.conf.d/99timeout
RUN echo "Acquire::ftp::Timeout \"10\";" >> /etc/apt/apt.conf.d/99timeout
RUN echo "Acquire::Retries \"3\";" >> /etc/apt/apt.conf.d/99retry

# include manual pages and documentation
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update &&\
  yes | unminimize

# install GCC-related packages
RUN apt-get -y install\
 binutils-doc\
 cpp-doc\
 gcc-doc\
 g++\
 g++-multilib\
 gdb\
 gdb-doc\
 glibc-doc\
 libblas-dev\
 liblapack-dev\
 liblapack-doc\
 libstdc++-10-doc\
 make\
 make-doc

# install clang-related packages
RUN apt-get -y install\
 clang\
 clang-10-doc\
 lldb\
 clang-format

# install cmake dependencies
RUN apt-get -y install\
  cmake\
  ninja-build\
  libssl-dev\
  libboost-all-dev\
  doctest-dev\
  doxygen\
  libcrypto++-dev\
  libcrypto++-doc\
  libcrypto++-utils\
  libsqlite3-dev\
  sqlite3

# install programs used for system exploration
RUN apt-get -y install\
 blktrace\
 linux-tools-generic\
 strace\
 tcpdump\
 silversearcher-ag

# install interactive programs (emacs, vim, nano, man, sudo, etc.)
RUN apt-get -y install\
 bc\
 curl\
 dc\
 git\
 git-doc\
 man\
 micro\
 nano\
 psmisc\
 python3\
 python\
 sudo\
 wget\
 zip\
 unzip\
 tar

# set up libraries
RUN apt-get -y install\
 libreadline-dev\
 locales\
 wamerican\
 libssl-dev\
 libgmp3-dev

# install programs used for networking
RUN apt-get -y install\
 dnsutils\
 inetutils-ping\
 iproute2\
 net-tools\
 netcat\
 telnet\
 time\
 traceroute\
 libgmp3-dev

# set up default locale
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8

# remove unneeded .deb files
RUN rm -r /var/lib/apt/lists/*

# set up passwordless sudo for user mpc-user
RUN useradd -m -s /bin/bash mpc-user && \
  echo "mpc-user ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/mpc-init

# create binary reporting version of dockerfile
RUN (echo '#\!/bin/sh'; echo 'echo 1') > /usr/bin/mpc-docker-version; chmod ugo+rx,u+w,go-w /usr/bin/mpc-docker-version

# git build arguments
ARG USER=MPC\ User
ARG EMAIL=nobody@example.com

# configure your environment
USER mpc-user
RUN rm -f ~/.bash_logout

WORKDIR /home/mpc-user
CMD ["/bin/bash", "-l"]

# purely cosmetic
ENV TERM xterm-256color

# ensures project build can find shared libraries
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
