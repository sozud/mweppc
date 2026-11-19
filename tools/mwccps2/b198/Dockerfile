# Builds the native mwccps2 3.0.1 build 198.
#
#   docker build -f tools/mwccps2/b198/Dockerfile -t mwccps2-b198 .
#   docker run --rm -v "$PWD/build:/out" mwccps2-b198
FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates clang lld python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
# the original, for the glue's data: decomp.me's public image, checked by digest
COPY tools/mwccps2/b198/get_orig.py tools/mwccps2/b198/get_orig.py
RUN python3 tools/mwccps2/b198/get_orig.py && test -f orig/PS2_3_0_1_b198/mwccps2.exe

COPY config config
COPY include include
COPY src src
COPY tools/mwccpsp tools/mwccpsp
COPY tools/mwccps2 tools/mwccps2
ARG JOBS
RUN python3 tools/mwccps2/b198/native_b198.py -j "${JOBS:-$(nproc)}" -o /mwccps2

CMD ["cp", "/mwccps2", "/out/mwccps2"]
