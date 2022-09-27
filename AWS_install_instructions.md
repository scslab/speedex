sudo apt-get update:wq

install

g++[10? 11? whichever is latest and available easily]
autoconf
make
bison
flex
pip

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10

install
libgsl-dev
liblmdb-dev
libsodium-dev
pkg-config
libtool
cython (possibly with an update-alternatives if you install cython3)
libtbb-dev
libglpk-dev
cmake
catch2 (clone, mkdir build && cd build && cmake .. && make -j && sudo make install)
libfyaml, via git clone git@github.com:pantoniou/libfyaml.git
./bootstrap.sh && ./configure && make && sudo make install

