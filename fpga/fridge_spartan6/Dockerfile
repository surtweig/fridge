# Xilinx ISE 14.7 (WebPACK) for Spartan-6 development.
# Build:  docker build -t ise:14.7 .
# Requires the ISE 14.7 split installer files in ./ISE/ (see README.md).
#
# Multi-stage: the ~8 GB installer payload lives only in the builder stage, so
# the final image contains just /opt/Xilinx.

FROM ubuntu:16.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8

# Note: xenial is still served by archive.ubuntu.com (no sources.list rewrite).
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        expect locales xz-utils \
        libglib2.0-0 libsm6 libice6 libxi6 libxrender1 libxrandr2 libxext6 \
        libx11-6 libfreetype6 libfontconfig1 \
    && rm -rf /var/lib/apt/lists/* \
    && echo "en_US.UTF-8 UTF-8" > /etc/locale.gen \
    && locale-gen en_US.UTF-8

COPY ISE/ /tmp/ise-media/
COPY docker/ise-install-config /tmp/ise-install-config
COPY docker/setup.exp /tmp/setup.exp

# Silent ISE WebPACK install (batchxsetup driven by expect)
RUN mkdir -p /tmp/install \
    && cp /tmp/ise-install-config /tmp/install/config \
    && tar -xf /tmp/ise-media/Xilinx_ISE_DS_14.7_1015_1-1.tar -C /tmp/install \
    && cp /tmp/ise-media/Xilinx_ISE_DS_14.7_1015_1-*.zip.xz /tmp/ \
    && cd /tmp/install \
    && TERM=xterm expect /tmp/setup.exp \
    && test -x /opt/Xilinx/14.7/ISE_DS/ISE/bin/lin64/xst

FROM ubuntu:16.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        make gcc g++ nano usbutils locales \
        libglib2.0-0 libsm6 libice6 libxi6 libxrender1 libxrandr2 libxext6 \
        libx11-6 libfreetype6 libfontconfig1 \
    && rm -rf /var/lib/apt/lists/* \
    && echo "en_US.UTF-8 UTF-8" > /etc/locale.gen \
    && locale-gen en_US.UTF-8

COPY --from=builder /opt/Xilinx /opt/Xilinx
RUN chmod -R a+rX /opt/Xilinx

COPY docker/ise-exec /usr/local/bin/ise-exec
RUN chmod +x /usr/local/bin/ise-exec

WORKDIR /work
ENTRYPOINT ["/usr/local/bin/ise-exec"]
CMD ["/bin/bash"]
