FROM alpine:latest

COPY . /otserv/.

RUN echo http://nl.alpinelinux.org/alpine/edge/testing >> /etc/apk/repositories && \
    apk update && \
    apk upgrade && \
    apk add --no-cache autoconf build-base pkgconfig boost-dev gmp-dev libxml2-dev && \
    apk add --no-cache automake lua lua-dev mariadb-dev crypto++ ccache
    cd /otserv/source && \
    chmod +x autogen.sh && \
    ./autogen.sh && \
    ./configure --enable-mysql --enable-server-diag && \
    make
