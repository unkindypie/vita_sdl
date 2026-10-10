# Build with --target linux for the native Linux toolchain. The resulting
# executable is written to the bind-mounted build/linux directory by Make.
# Pin a release so the host-runnable Linux binary has a stable ABI baseline.
FROM fedora:42@sha256:99e203b80b1c3d8f7e161ec10a68fd02b081ef83a3963553e513c82846b97814 AS linux

RUN dnf --assumeyes --quiet --setopt=install_weak_deps=False install \
      cmake \
      gcc \
      gcc-c++ \
      libasan \
      make \
      SDL2-devel \
    >/dev/null \
    && dnf clean all

WORKDIR /workspace

CMD ["sh"]

# Build with --target vita for VitaSDK's cross-compiler, SDL2 port, and VPK
# packaging tools. This is the maintained VitaSDK Docker image.
FROM vitasdk/vitasdk@sha256:fd82d88ff456d118eef16533ebf9a3a6ae0e292684fb8a3a35f15f0707b4692d AS vita

# The SDK image bundles SDL2's vitaGL variant. Use upstream's native GXM
# renderer to avoid runtime shader-compiler dependencies for this 2D presenter.
ADD --checksum=sha256:5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165 https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz /tmp/SDL2.tar.gz
RUN tar -xzf /tmp/SDL2.tar.gz -C /tmp \
    && cmake -S /tmp/SDL2-2.32.10 -B /tmp/sdl2-build \
      -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
      -DCMAKE_INSTALL_PREFIX=/opt/sdl2-vita -DCMAKE_BUILD_TYPE=Release \
      -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF \
      -DVIDEO_VITA_PIB=OFF -DVIDEO_VITA_PVR=OFF \
    && cmake --build /tmp/sdl2-build --parallel 4 \
    && cmake --install /tmp/sdl2-build

WORKDIR /workspace

CMD ["sh"]
