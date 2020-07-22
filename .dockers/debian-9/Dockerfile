FROM debian:9

RUN apt-get update
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install tzdata
RUN apt-get install -y build-essential
RUN apt-get install -y git cmake libssl-dev clang libc++-dev libc++abi-dev libbz2-dev zlib1g-dev mlocate

WORKDIR /devel
RUN git clone -b develop https://github.com/boostorg/boost.git
COPY .dockers/debian-9/user-config.jam /devel/boost/
COPY .dockers/debian-9/tests.sh /devel/boost/

WORKDIR /devel/boost
RUN git submodule update --init --recursive
COPY . /devel/boost/libs/beast/

RUN ./bootstrap.sh
RUN ./b2 toolset=gcc variant=release cxxstd=latest headers
RUN ./tests.sh || true
