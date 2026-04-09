# NeuronOS Build + Run Environment
# Uses Ubuntu with cross-compilation tools and QEMU
# Build:  docker build -t neuronos .
# Run:    docker run --rm -it neuronos

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ─── Install toolchain + QEMU ─────────────────────────────────────────────────
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    gcc-multilib \
    binutils \
    nasm \
    qemu-system-x86 \
    xorriso \
    grub-pc-bin \
    grub-common \
    mtools \
    python3 \
    && apt-get clean

WORKDIR /neuronos
COPY . .

# ─── Build ────────────────────────────────────────────────────────────────────
RUN make clean && make all

# ─── Default: run in QEMU (no display — serial output only) ──────────────────
CMD ["make", "run-serial"]
