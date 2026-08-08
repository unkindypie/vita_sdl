PROJECT := vita_sdl2
BUILD_DIR := build
LINUX_BUILD_DIR := $(BUILD_DIR)/linux
VITA_BUILD_DIR := $(BUILD_DIR)/vita
DOCKER_IMAGE := vita-sdl2-dev
LINUX_DOCKER_IMAGE := vita-sdl2-linux-build
HOST_UID := $(shell id -u)
HOST_GID := $(shell id -g)

.PHONY: help linux run vita docker-image linux-docker-image prepare-linux-build prepare-vita-build linux-in-container vita-in-container clean

help:
	@printf '%s\n' \
	  'make linux        Build the Linux test executable in Docker' \
	  'make run          Build in Docker and run the executable on the host' \
	  'make docker-image Build the local VitaSDK development image' \
	  'make linux-docker-image Build the local Linux build image' \
	  'make vita         Build the PS Vita VPK in Docker' \
	  'make clean        Remove all generated files'


linux-docker-image:
	docker build --target linux --tag $(LINUX_DOCKER_IMAGE) .

# Compile in Docker but retain a host-owned executable for local execution.
linux: linux-docker-image prepare-linux-build
	docker run --rm --user "$(HOST_UID):$(HOST_GID)" --volume "$(CURDIR):/workspace" \
	  --workdir /workspace $(LINUX_DOCKER_IMAGE) make linux-in-container

# Docker previously created build/ as root. Ensure the mounted output
# directories are writable before compiling as the host user.
prepare-linux-build:
	docker run --rm --user 0 --volume "$(CURDIR):/workspace" $(LINUX_DOCKER_IMAGE) \
	  sh -c 'mkdir -p /workspace/$(LINUX_BUILD_DIR) && chown -R $(HOST_UID):$(HOST_GID) /workspace/$(LINUX_BUILD_DIR)'

linux-in-container:
	cmake -S . -B $(LINUX_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(LINUX_BUILD_DIR) --parallel

run: linux
	$(LINUX_BUILD_DIR)/$(PROJECT)

docker-image:
	docker build --target vita --tag $(DOCKER_IMAGE) .

# The bind mount means build/vita is retained on the host after Docker exits.
vita: docker-image prepare-vita-build
	docker run --rm --user "$(HOST_UID):$(HOST_GID)" --volume "$(CURDIR):/workspace" --workdir /workspace $(DOCKER_IMAGE) \
	  make vita-in-container

prepare-vita-build:
	docker run --rm --user 0 --volume "$(CURDIR):/workspace" $(DOCKER_IMAGE) \
	  sh -c 'mkdir -p /workspace/$(VITA_BUILD_DIR) && chown -R $(HOST_UID):$(HOST_GID) /workspace/$(VITA_BUILD_DIR)'

vita-in-container:
	cmake -S . -B $(VITA_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_TOOLCHAIN_FILE="$$VITASDK/share/vita.toolchain.cmake"
	cmake --build $(VITA_BUILD_DIR) --parallel

clean:
	$(RM) -r $(BUILD_DIR)
