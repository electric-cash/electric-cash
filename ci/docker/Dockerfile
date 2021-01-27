# --- Base Image ---
FROM ubuntu:18.04 as base

ENV DOCKERIZE_VERSION v0.6.1

RUN apt-get update && \
    apt-get -y install --no-install-recommends --no-install-suggests \
        curl \
        libboost-all-dev \
        libevent-dev \
        libssl-dev \
        libzmq3-dev \
        git \
        bsdmainutils \
        software-properties-common \
        build-essential \
        pkg-config \
        autoconf \
        automake \
        libtool \
        autotools-dev \
    && add-apt-repository ppa:bitcoin/bitcoin \
    && apt-get update \
    && apt-get -y install libdb4.8-dev libdb4.8++-dev \
    && curl -L -k -f https://github.com/jwilder/dockerize/releases/download/$DOCKERIZE_VERSION/dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz -o dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz \
    && tar -C /usr/local/bin -xzvf dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz \
    && rm dockerize-linux-amd64-$DOCKERIZE_VERSION.tar.gz \
    && rm -fr /source \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean


# --- Compilation ---
FROM base AS builder
ARG CONFIGURE_PARAMS="--disable-bench --disable-wallet --with-gui=no --disable-tests"
WORKDIR /code
COPY . .
RUN env CFLAGS=-O2 CXXFLAGS=-O2 ./autogen.sh \
    && ./configure ${CONFIGURE_PARAMS} \
    && make -j`nproc` \
    && make install

# --- Release ----
FROM builder AS release
WORKDIR /elcash

ARG GIT_COMMIT=unspecified
LABEL revision=$GIT_COMMIT

ENV ELCASHD_SERVER 1
ENV ELCASHD_TESTNET_ENABLED 0
ENV ELCASHD_NET_BAN_SCORE 100000
ENV ELCASHD_NET_BAN_TIME 60
ENV ELCASHD_COMMIT $GIT_COMMIT

COPY --from=builder /usr/local/bin/elcashd /usr/local/bin/elcashd
# https://github.com/moby/moby/issues/37965
RUN true
COPY ci/docker/logrotate-elcash /etc/logrotate.d/elcash
COPY ci/docker/*.tmpl /templates/
COPY ci/docker/entrypoint.sh /elcash/

VOLUME /elcash/blockchain

ENTRYPOINT ["sh", "/elcash/entrypoint.sh"]
CMD ["elcashd","-conf=/elcash/elcash.conf"]
