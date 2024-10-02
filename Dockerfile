FROM ubuntu:jammy

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
      lldb \
      clang-format

# install cmake dependencies
RUN apt-get -y install \
      cmake \
      libssl-dev

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
      telnet \
      time \
      traceroute \
      libgmp3-dev

# install the specific version of boost that libOTe wants
RUN wget https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.gz && \
    tar -xzf boost_1_86_0.tar.gz && \
    (cd ./boost_1_86_0; ./bootstrap.sh) && \
    (cd ./boost_1_86_0/; ./b2 install --with-program_options --with-thread --with-system --with-filesystem) && \
    rm -r boost_1_86_0 boost_1_86_0.tar.gz

# configure our user
WORKDIR /home/pcg-user/

# copy repository to the image
COPY . .

# build and install libOTe
RUN (cd libOTe; python3 build.py --all --boost --sodium --relic --install)

# build our project
RUN (cd primal-dual-pcg/build; git clean -xfd) && \
    (cd primal-dual-pcg/build; cmake ..) && \
    (cd primal-dual-pcg/build; make)

# remove unneeded .deb files
RUN rm -r /var/lib/apt/lists/*

# set up passwordless sudo for user pcg-user
RUN useradd -m -s /bin/bash pcg-user &&  \
  echo "pcg-user ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/pcg-init

# git build arguments
ARG USER=PCG\ User
ARG EMAIL=nobody@example.com

USER pcg-user
RUN rm -f ~/.bash_logout

# configure the endpoints to reach
CMD ["/bin/bash", "-l"]
CMD ["./primal-dual-pcg/build/protocol"]
CMD ["./primal-dual-pcg/build/unit_tests"]
