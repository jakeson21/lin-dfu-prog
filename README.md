# lin-dfu-prog

Custom dfu-programmer for Linux for Arduino-Tiva

## Build instructions

Ensure you have the necessary tools

``` bash
apt update && apt -y install \
    autoconf \
    build-essential \
    libtool \
    libusb-dev \
    libusb-1.0 \
    libglib2.0-dev \
    pkg-config
```

Then build in the root folder. It will attempt to install into /usr/local/bin.

``` bash
autoreconf -fis && \
    autoconf && \
    aclocal && \
    automake --add-missing --foreign && \
    ./configure && \
    make -j && make install && make clean
```
