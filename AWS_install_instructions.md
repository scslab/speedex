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
libgsldev
liblmdb-dev
libsodium-dev
pkg-config
libtool
cython
libtbb-dev
libglpk-dev
libfyaml, via git clone git@github.com:pantoniou/libfyaml.git
./bootstrap.sh && ./configure && make && sudo make install

