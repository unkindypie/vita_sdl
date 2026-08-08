# Build with --target linux for the native Linux toolchain. The resulting
# executable is written to the bind-mounted build/linux directory by Make.
# Pin a release so the host-runnable Linux binary has a stable ABI baseline.
FROM fedora:42 AS linux

RUN dnf --assumeyes --quiet --setopt=install_weak_deps=False install \
      cmake \
      gcc \
      gcc-c++ \
      make \
      SDL2-devel \
    >/dev/null \
    && dnf clean all

WORKDIR /workspace

CMD ["sh"]

# Build with --target vita for VitaSDK's cross-compiler, SDL2 port, and VPK
# packaging tools. This is the maintained VitaSDK Docker image.
FROM vitasdk/vitasdk:latest AS vita

WORKDIR /workspace

CMD ["sh"]
