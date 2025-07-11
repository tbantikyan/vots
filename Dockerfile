FROM ubuntu:22.04

# Prevent interactive prompts during install
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \            
      curl \
      git \
      openssh-client && \
    apt-get install -y --no-install-recommends cmake && \
    rm -rf /var/lib/apt/lists/*

RUN curl -LO https://github.com/neovim/neovim/releases/latest/download/nvim-linux-arm64.appimage && \
    chmod u+x nvim-linux-arm64.appimage && \
    ./nvim-linux-arm64.appimage --appimage-extract && \
    ln -s /squashfs-root/AppRun /usr/bin/nvim

WORKDIR /workspace

CMD ["/bin/bash"]
