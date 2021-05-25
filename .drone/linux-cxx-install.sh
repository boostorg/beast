#!/bin/bash

set -ex
echo ">>>>> APT: REPO.."
for i in {1..3}; do sudo -E apt-add-repository -y "ppa:ubuntu-toolchain-r/test" && break || sleep 2; done
if test -n "${LLVM_OS}" ; then
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    if test -n "${LLVM_VER}" ; then
	sudo -E apt-add-repository "deb http://apt.llvm.org/${LLVM_OS}/ llvm-toolchain-${LLVM_OS}-${LLVM_VER} main"
    else
        # Snapshot (i.e. trunk) build of clang
	sudo -E apt-add-repository "deb http://apt.llvm.org/${LLVM_OS}/ llvm-toolchain-${LLVM_OS} main"
    fi
elif [[ $CXX =~ ^clang || $TOOLSET =~ ^clang || $B2_TOOLSET =~ ^clang ]]; then
    # Default Travis Installation of clang-7
    LLVM_OS=$(awk -F= '$1=="VERSION_CODENAME" { print $2 ;}' /etc/os-release)
    LLVM_VER="7"
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    sudo -E apt-add-repository "deb http://apt.llvm.org/${LLVM_OS}/ llvm-toolchain-${LLVM_OS}-${LLVM_VER} main"
    sudo -E apt-get -o Acquire::Retries=3 update
    sudo -E apt-get -o Acquire::Retries=3 -y --no-install-suggests --no-install-recommends install libstdc++-8-dev clang-7 libc++-7-dev libc++-helpers libc++abi-7-dev
    ln -s /usr/bin/clang-7 /usr/bin/clang
    ln -s /usr/bin/clang /usr/bin/clang++
fi

echo ">>>>> APT: UPDATE.."
sudo -E apt-get -o Acquire::Retries=3 update
echo ">>>>> APT: INSTALL ${PACKAGES}.."
sudo -E apt-get -o Acquire::Retries=3 -y --no-install-suggests --no-install-recommends install ${PACKAGES}

MAJOR_VERSION=$(lsb_release -r -s | cut -c 1-2)
if [ "$MAJOR_VERSION" -lt "20" ]; then
    sudo -E apt-get -o Acquire::Retries=3 -y install python python-pip
fi

if [ "$MAJOR_VERSION" -gt "18" ]; then
    sudo -E apt-get -o Acquire::Retries=3 -y install python3 python3-pip
    ln -s /usr/bin/python3 /usr/bin/python || true
    ln -s /usr/bin/pip3 /usr/bin/pip || true
fi
