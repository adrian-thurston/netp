FROM ubuntu:16.04

RUN apt-get update && apt-get install --yes \
		libtool m4 autoconf automake git \
		automake autoconf libtool make g++ gcc \
        gcc g++ dpkg-dev debhelper git gitk valgrind gdb \
        libdbd-sqlite3-perl libssl-dev libc-ares-dev bridge-utils \
        asciidoc python-pygments libpcap-dev cmake uuid-dev \
        libboost-dev libboost-system-dev libboost-program-options-dev \
        libcap-dev libreadline6-dev flex bison pkgconf \
        zlib1g-dev libpq-dev libsqlite3-dev tesseract-ocr python-html5lib python-lxml \
		libbrotli-dev vim openvpn libcurl4-openssl-dev

#
# colm
#
RUN mkdir devel
RUN git clone https://github.com/adrian-thurston/colm.git /devel/colm
WORKDIR /devel/colm
RUN git checkout -B no-bare-send origin/no-bare-send
RUN ./autogen.sh
RUN ./configure --prefix=/pkgs/colm --disable-manual
RUN make -j4
RUN make install
RUN echo /pkgs/colm/lib > /etc/ld.so.conf.d/colm.conf
RUN ldconfig

#
# ragel
#
RUN git clone https://github.com/adrian-thurston/ragel.git /devel/ragel
WORKDIR /devel/ragel
RUN ./autogen.sh
RUN ./configure --prefix=/pkgs/ragel --with-colm=/pkgs/colm --disable-manual
RUN make -j4
RUN make install
RUN echo /pkgs/ragel/lib > /etc/ld.so.conf.d/ragel.conf
RUN ldconfig

#
# netp
#
COPY . /devel/netp
WORKDIR /devel/netp
RUN ./autogen.sh
RUN ./configure --with-colm=/pkgs/colm --with-ragel=/pkgs/ragel
RUN make -j4

