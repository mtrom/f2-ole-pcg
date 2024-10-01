FROM ubuntu:focal

# set environment variables for tzdata
ARG TZ=America/New_York
ENV TZ=${TZ}
ENV LANG en_US.UTF-8
ENV TERM xterm-256color

# ensures project build can find shared libraries
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# set higher timeouts to get through the build
RUN echo "Acquire::http::Timeout \"10\";" > /etc/apt/apt.conf.d/99timeout
RUN echo "Acquire::ftp::Timeout \"10\";" >> /etc/apt/apt.conf.d/99timeout
RUN echo "Acquire::Retries \"3\";" >> /etc/apt/apt.conf.d/99retry

ARG DEBIAN_FRONTEND=noninteractive

# install gcc-related packages
RUN apt-get update && \
    apt-get -y install \
      binutils-doc \
      cpp-doc \
      gcc-doc \
      g++ \
      g++-multilib \
      gdb \
      gdb-doc \
      glibc-doc \
      libblas-dev \
      liblapack-dev \
      liblapack-doc \
      libstdc++-10-doc \
      make \
      make-doc

# install clang-related packages
RUN apt-get -y install \
      clang \
      clang-10-doc \
      lldb \
      clang-format

# install cmake dependencies
RUN apt-get -y install \
      cmake \
      ninja-build \
      libssl-dev \
      libboost-all-dev \
      doctest-dev \
      doxygen \
      libcrypto++-dev \
      libcrypto++-doc \
      libcrypto++-utils \
      libsqlite3-dev \
      sqlite3

# install programs used for system exploration
RUN apt-get -y install \
      blktrace \
      linux-tools-generic \
      strace \
      tcpdump \
      silversearcher-ag

# install interactive programs (emacs, vim, nano, man, sudo, etc.)
RUN apt-get -y install \
      bc \
      curl \
      dc \
      git \
      git-doc \
      man \
      micro \
      nano \
      psmisc \
      python3 \
      python \
      sudo \
      wget \
      zip \
      unzip \
      tar

# set up libraries
RUN apt-get -y install \
      libreadline-dev \
      locales \
      wamerican \
      libssl-dev \
      libgmp3-dev

# install programs used for networking
RUN apt-get -y install \
      dnsutils \
      inetutils-ping \
      iproute2 \
      net-tools \
      netcat \
      telnet \
      time \
      traceroute \
      libgmp3-dev

# remove unneeded .deb files
RUN rm -r /var/lib/apt/lists/*

# set up passwordless sudo for user pcg-user
RUN useradd -m -s /bin/bash pcg-user &&  \
  echo "pcg-user ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/pcg-init

# git build arguments
ARG USER=PCG\ User
ARG EMAIL=nobody@example.com

# configure your environment
USER pcg-user
RUN rm -f ~/.bash_logout
WORKDIR /home/pcg-user
CMD ["/bin/bash", "-l"]
