# docker build --rm -t tiva-dfu:latest .

FROM ubuntu:18.04

LABEL maintainer="Jacob Miller (jake_son@yahoo.com)"

# RUN apt-get update && apt-get install -y -q \
#     #python3.7 \
# WORKDIR /root/tiva-dfu-prog
RUN mkdir -p /home/fuguru/git/tiva-dfu-prog-linux
COPY . /home/fuguru/git/tiva-dfu-prog-linux
WORKDIR /home/fuguru/git/tiva-dfu-prog-linux

RUN rm -rf bin

RUN apt update && apt -y install \ 
    autoconf \
    automake \
    autotools-dev \
    build-essential \
    curl \
    libtool \
    libusb-dev \
    libusb-1.0 \
    libglib2.0-dev \
    pkg-config \
    vim

RUN autoreconf -fis && \
    autoconf && \
    aclocal && \
    automake --add-missing --foreign && \
    ./configure && \
    make -j && make install && make clean

ENTRYPOINT ["/bin/bash"]
