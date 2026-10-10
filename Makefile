PROJECT := vita_sdl2
BUILD_DIR := build
LINUX_BUILD_DIR := $(BUILD_DIR)/linux
VITA_BUILD_DIR := $(BUILD_DIR)/vita
DOCKER_IMAGE := vita-sdl2-dev
LINUX_DOCKER_IMAGE := vita-sdl2-linux-build
HOST_UID := $(shell id -u)
HOST_GID := $(shell id -g)
ARGS ?=
CMAKE_ARGS ?=
VITA3K ?= $(CURDIR)/build/tools/vita3k/Vita3K
VITA3K_DIR ?= build/vita3k
# Vita3K separates guest arguments with comma + space, not ordinary spaces.
VITA3K_ARGS ?=

.PHONY: help linux run test test-in-container test-asan smoke verify-vpk vita vita3k-setup vita3k vita3k-smoke docker-image linux-docker-image prepare-linux-build prepare-vita-build linux-in-container vita-in-container clean

help:
	@printf '%s\n' \
	  'make linux        Build the Linux test executable in Docker' \
	  'make run          Build in Docker and run the executable on the host' \
	  'make test         Headless unit, integration, and launch tests in Docker' \
	  'make test-asan    Run headless tests with AddressSanitizer' \
	  'make smoke        Launch on the host, exercise movement/camera, save a BMP' \
	  'make docker-image Build the local VitaSDK development image' \
	  'make linux-docker-image Build the local Linux build image' \
	  'make vita         Build the PS Vita VPK in Docker' \
	  'make verify-vpk   Build Vita and verify packaged assets' \
	  'make vita3k-setup Download/check the pinned Linux x86_64 emulator' \
	  'make vita3k       Set up Vita3K, build and launch the Vita app' \
	  'make vita3k-smoke Run 600-frame guest movement/camera smoke in Vita3K' \
	  'make clean        Remove all generated files'


linux-docker-image:
	docker build --target linux --tag $(LINUX_DOCKER_IMAGE) .

# Compile in Docker but retain a host-owned executable for local execution.
linux: linux-docker-image prepare-linux-build
	docker run --rm --user "$(HOST_UID):$(HOST_GID)" --volume "$(CURDIR):/workspace" \
	  --workdir /workspace $(LINUX_DOCKER_IMAGE) make linux-in-container LINUX_BUILD_DIR=$(LINUX_BUILD_DIR) CMAKE_ARGS="$(CMAKE_ARGS)"

# Docker previously created build/ as root. Ensure the mounted output
# directories are writable before compiling as the host user.
prepare-linux-build:
	docker run --rm --user 0 --volume "$(CURDIR):/workspace" $(LINUX_DOCKER_IMAGE) \
	  sh -c 'mkdir -p /workspace/$(LINUX_BUILD_DIR) && chown -R $(HOST_UID):$(HOST_GID) /workspace/$(LINUX_BUILD_DIR)'

linux-in-container:
	cmake -S . -B $(LINUX_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug $(CMAKE_ARGS)
	cmake --build $(LINUX_BUILD_DIR) --parallel

run: linux
	$(LINUX_BUILD_DIR)/$(PROJECT) $(ARGS)

test: linux
	docker run --rm --user "$(HOST_UID):$(HOST_GID)" --volume "$(CURDIR):/workspace" \
	  --workdir /workspace $(LINUX_DOCKER_IMAGE) make test-in-container LINUX_BUILD_DIR=$(LINUX_BUILD_DIR)

test-asan:
	$(MAKE) test LINUX_BUILD_DIR=build/linux-asan CMAKE_ARGS=-DPORT_SANITIZE=ON

test-in-container:
	ctest --test-dir $(LINUX_BUILD_DIR) --output-on-failure

smoke: linux
	$(LINUX_BUILD_DIR)/$(PROJECT) --smoke --frames 240 --screenshot $(LINUX_BUILD_DIR)/smoke.bmp

docker-image:
	docker build --target vita --tag $(DOCKER_IMAGE) .

# The bind mount means build/vita is retained on the host after Docker exits.
vita: docker-image prepare-vita-build
	docker run --rm --user "$(HOST_UID):$(HOST_GID)" --volume "$(CURDIR):/workspace" --workdir /workspace $(DOCKER_IMAGE) \
	  make vita-in-container VITA_BUILD_DIR=$(VITA_BUILD_DIR) CMAKE_ARGS="$(CMAKE_ARGS)"

prepare-vita-build:
	docker run --rm --user 0 --volume "$(CURDIR):/workspace" $(DOCKER_IMAGE) \
	  sh -c 'mkdir -p /workspace/$(VITA_BUILD_DIR) && chown -R $(HOST_UID):$(HOST_GID) /workspace/$(VITA_BUILD_DIR)'

vita-in-container:
	cmake -S . -B $(VITA_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_TOOLCHAIN_FILE="$$VITASDK/share/vita.toolchain.cmake" \
	  -DSDL2_DIR=/opt/sdl2-vita/lib/cmake/SDL2 $(CMAKE_ARGS)
	cmake --build $(VITA_BUILD_DIR) --parallel

verify-vpk: vita
	docker run --rm --user "$(HOST_UID):$(HOST_GID)" --volume "$(CURDIR):/workspace" \
	  --workdir /workspace $(DOCKER_IMAGE) cmake -DVPK=$(VITA_BUILD_DIR)/$(PROJECT).vpk \
	  -DMANIFEST=$(VITA_BUILD_DIR)/asset_manifest.txt -P cmake/verify_vpk.cmake

vita3k-setup: docker-image
	docker run --rm --user 0 --volume "$(CURDIR):/workspace" $(DOCKER_IMAGE) \
	  sh -c 'mkdir -p /workspace/build/tools && chown -R $(HOST_UID):$(HOST_GID) /workspace/build/tools'
	sh tools/setup_vita3k.sh "$(VITA3K)" "$(CURDIR)/build/tools/vita3k/Vita3K"

vita3k: vita3k-setup
	$(MAKE) vita
	docker run --rm --user 0 --volume "$(CURDIR):/workspace" $(DOCKER_IMAGE) \
	  sh -c 'mkdir -p /workspace/$(VITA3K_DIR) && chown -R $(HOST_UID):$(HOST_GID) /workspace/$(VITA3K_DIR)'
	docker run --rm --user "$(HOST_UID):$(HOST_GID)" --volume "$(CURDIR):/workspace" \
	  --workdir /workspace $(DOCKER_IMAGE) cmake \
	  -DOUTPUT=/workspace/$(VITA3K_DIR)/config.yml \
	  -DPREF_PATH="$(CURDIR)/$(VITA3K_DIR)/storage/" -P cmake/prepare_vita3k.cmake
	"$(VITA3K)" --frontend sdl --console --load-config \
	  --config-location "$(CURDIR)/$(VITA3K_DIR)/config.yml" --keep-config \
	  --app-args '$(VITA3K_ARGS)' "$(CURDIR)/$(VITA_BUILD_DIR)/$(PROJECT).vpk"

vita3k-smoke:
	$(MAKE) vita3k VITA3K_ARGS='--smoke, --frames, 600, --verify-gameplay, --screenshot, ux0:data/asciicker/vita3k.bmp'

clean:
	$(RM) -r $(BUILD_DIR)
