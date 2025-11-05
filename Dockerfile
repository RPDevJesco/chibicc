# Dockerfile for building chibicc with EventChains integration
# This allows building on Windows, macOS, and Linux without installing dependencies

FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    binutils \
    libc6-dev \
    file \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /chibicc

# Copy source files
COPY . .

# Build chibicc
RUN make clean && make

# Default command: run tests
CMD ["make", "test"]

# Usage:
# Build: docker build -t chibicc .
# Run tests: docker run --rm chibicc
# Compile a file: docker run --rm -v $(pwd):/work chibicc ./chibicc /work/input.c -o /work/output.s
