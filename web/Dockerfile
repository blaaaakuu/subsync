FROM emscripten/emsdk:6.0.4

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake \
        git \
        make \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*
