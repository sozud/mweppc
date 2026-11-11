# Builds the native mwccpsp (PSP 3.0.1 build 219).
#
#   docker build -f tools/mwccpsp/Dockerfile -t mwccpsp .
#   docker run --rm -v "$PWD/build:/out" mwccpsp
FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates clang curl lld python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
RUN mkdir -p orig/PSP_3_0_1 \
    && curl -fsSL -o mwccpsp_219.tar.gz \
        https://github.com/Xeeynamo/sotn-decomp/releases/download/cc1-psx-26/mwccpsp_219.tar.gz \
    && echo "62ec4faa807ad5afaefa7c92de6423505072c321fad13c071c2f640e9820b5c8  mwccpsp_219.tar.gz" | sha256sum -c \
    && tar -xzf mwccpsp_219.tar.gz -C orig/PSP_3_0_1 \
    && rm mwccpsp_219.tar.gz \
    && test -f orig/PSP_3_0_1/mwccpsp.exe

COPY config config
COPY include include
COPY src src
COPY tools/mwccpsp tools/mwccpsp
ARG JOBS
RUN python3 tools/mwccpsp/native_build.py -j "${JOBS:-$(nproc)}" -o /mwccpsp

CMD ["cp", "/mwccpsp", "/out/mwccpsp"]
