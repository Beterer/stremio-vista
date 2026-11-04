FROM node:24.19.0-bookworm@sha256:4196d66a565c6f195728d9952f161f4adfe2ad753052a08b7ec7f1c5a6bda42b

RUN apt-get update && apt-get install -y --no-install-recommends \
    binutils clangd cmake ffmpeg gdb nasm ninja-build pkg-config \
    python3 shellcheck zlib1g-dev && rm -rf /var/lib/apt/lists/*

ARG ZIG_VERSION=0.17.0
ARG ZIG_SHA256=1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026
RUN curl -fsSL --retry 3 "https://ziglang.org/download/${ZIG_VERSION}/zig-x86_64-linux-${ZIG_VERSION}.tar.xz" -o /tmp/zig.tar.xz \
    && echo "${ZIG_SHA256}  /tmp/zig.tar.xz" | sha256sum -c - \
    && mkdir /opt/zig && tar -xJf /tmp/zig.tar.xz -C /opt/zig --strip-components=1 \
    && rm /tmp/zig.tar.xz

# Only the ARM TV sysroot is used; the archive's ARM64 host compiler is unused.
ARG SDK_RELEASE=webos-d7ed7ee.6
ARG SDK_SHA256=45a2d12ff557457d92cde4fddaa77a6f1090fca03adc43bb74397e5e0c379501
RUN test -n "$SDK_SHA256" \
    && curl -fsSL --retry 3 "https://github.com/webosbrew/native-toolchain/releases/download/${SDK_RELEASE}/arm-webos-linux-gnueabi_sdk-buildroot_linux-aarch64.tar.bz2" -o /tmp/sdk.tar.bz2 \
    && echo "${SDK_SHA256}  /tmp/sdk.tar.bz2" | sha256sum -c - \
    && mkdir /opt/webos-sdk && tar -xjf /tmp/sdk.tar.bz2 -C /opt/webos-sdk --strip-components=1 \
    && rm /tmp/sdk.tar.bz2

ENV RUSTUP_HOME=/usr/local/rustup CARGO_HOME=/usr/local/cargo
ENV PATH=/usr/local/cargo/bin:/opt/zig:$PATH
RUN curl -fsSL --retry 3 https://sh.rustup.rs -o /tmp/rustup.sh \
    && sh /tmp/rustup.sh -y --profile minimal --default-toolchain 1.90.0 \
    && rustup target add armv7-unknown-linux-gnueabi && rm /tmp/rustup.sh
RUN npm install -g @webos-tools/cli@3.2.6

ENV ORIGIN_BUILD=/opt/origin-build \
    ZIG=/opt/zig/zig \
    SDK=/opt/webos-sdk/arm-webos-linux-gnueabi/sysroot \
    ZIG_GLOBAL_CACHE_DIR=/opt/origin-build/zig-cache \
    CARGO_HOME=/opt/origin-build/cargo \
    CARGO_TARGET_DIR=/opt/origin-build/cargo-target
RUN git config --global --add safe.directory /workspace
WORKDIR /workspace
CMD ["bash"]
