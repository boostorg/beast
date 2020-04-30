FROM madmongo1/ubuntu-xenial-base:latest

# boost as of today (2020-04-29)
ENV boost_commit develop
WORKDIR /tmp
RUN git clone https://github.com/boostorg/boost.git boost-root \
    && cd boost-root \
    && git checkout ${boost_commit} \
    && git submodule update --init --recursive
