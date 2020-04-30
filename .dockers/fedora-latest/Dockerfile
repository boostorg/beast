FROM fedora:32

RUN dnf update -y
RUN dnf -y groupinstall "Development Tools"
RUN dnf install -y cmake openssl-devel clang libcxx-devel libcxxabi-devel bzip2-devel which zlib-devel

WORKDIR /devel
RUN git clone -b develop https://github.com/boostorg/boost.git
COPY .dockers/fedora-latest/user-config.jam /devel/boost/

WORKDIR /devel/boost
RUN git submodule update --init --recursive
COPY Jamfile /devel/boost/libs/beast/
COPY doc /devel/boost/libs/beast/doc/
COPY example /devel/boost/libs/beast/example/
COPY meta /devel/boost/libs/beast/meta/
COPY tools /devel/boost/libs/beast/tools/

#RUN cd libs/beast && git checkout develop && git pull origin develop
RUN ./bootstrap.sh
RUN ./b2 toolset=gcc variant=release cxxstd=2a headers
#RUN ./b2 toolset=gcc,clang variant=release cxxstd=2a,17,14,11 -j`grep processor /proc/cpuinfo | wc -l` libs/beast//test
