FROM ubuntu:22.04

# Prevent interactive prompts during install
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \            
      curl \
      git \
      gnupg \
      llvm \
      lsb-release \
      openssh-client \
      ripgrep \
      software-properties-common \
      wget && \
    apt-get install -y --no-install-recommends cmake && \
    rm -rf /var/lib/apt/lists/*

RUN curl -LO https://github.com/neovim/neovim/releases/latest/download/nvim-linux-arm64.appimage && \
    chmod u+x nvim-linux-arm64.appimage && \
    ./nvim-linux-arm64.appimage --appimage-extract && \
    ln -s /squashfs-root/AppRun /usr/bin/nvim

RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 20 && \
    apt-get install -y clangd-20 && \
    update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-20 100 && \
    mkdir -p ~/.local/share/nvim/mason/packages/clangd/bin && \
    ln -s "$(which clangd-20)" ~/.local/share/nvim/mason/packages/clangd/bin/clangd && \
    rm llvm.sh

WORKDIR /workspace

CMD ["/bin/bash"]
