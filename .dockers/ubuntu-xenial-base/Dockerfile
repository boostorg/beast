FROM ubuntu:xenial

RUN apt-get update

# basic development tools
RUN apt-get install -y software-properties-common
RUN apt-get install -y libffi-dev
RUN apt-get install -y libstdc++6
RUN apt-get install -y binutils-gold
RUN apt-get install -y gdb

# for building docs
RUN apt-get install -y docbook
RUN apt-get install -y docbook-xml
RUN apt-get install -y docbook-xsl
RUN apt-get install -y xsltproc
RUN apt-get install -y libsaxonhe-java
RUN apt-get install -y default-jre-headless
RUN apt-get install -y build-essential
RUN apt-get install -y git
RUN apt-get install -y cmake
RUN apt-get install -y flex
RUN apt-get install -y bison
RUN apt-get install -y graphviz-dev
RUN apt-get install -y sudo
RUN apt-get install -y wget
RUN apt-get install -y unzip

# build doxygen
WORKDIR /tmp
RUN git clone -b 'Release_1_8_15' --depth 1 https://github.com/doxygen/doxygen.git
RUN cmake -Hdoxygen -Bbuild/doxygen -DCMAKE_BUILD_TYPE=Release
WORKDIR /tmp/build/doxygen
RUN cores=$(grep processor /proc/cpuinfo | wc -l) && make -j ${cores} && sudo make install

# update saxonhe
WORKDIR /tmp/saxonhe
RUN wget -O saxonhe.zip https://sourceforge.net/projects/saxon/files/Saxon-HE/9.9/SaxonHE9-9-1-4J.zip/download
RUN unzip saxonhe.zip
RUN sudo rm /usr/share/java/Saxon-HE.jar \
    && sudo cp saxon9he.jar /usr/share/java/Saxon-HE.jar

