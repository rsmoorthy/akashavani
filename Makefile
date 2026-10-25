SHELL := /usr/bin/env bash

BOARD ?= receiver
PORT ?= /dev/ttyACM0
BAUD ?= 115200
PYTHON ?= python3

.DEFAULT_GOAL := help

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------
.PHONY: help
help: ## Show this help
	@awk 'BEGIN {FS = ":.*##"; printf "Usage: make <target> [BOARD=receiver|transmitter_sim] [PORT=/dev/ttyACM0]\n\nTargets:\n"} \
		/^[a-zA-Z0-9_.-]+:.*##/ {printf "  \033[36m%-18s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------
.PHONY: setup
setup: fw-setup server-setup ## Install firmware toolchain and Python environment

.PHONY: fw-setup
fw-setup: ## Install the pinned ESP32 core and external Arduino libraries
	./tools/fw-setup.sh

.PHONY: server-setup
server-setup: ## Create .venv and install server + docs dependencies
	./tools/server-setup.sh

# ---------------------------------------------------------------------------
# Firmware
# ---------------------------------------------------------------------------
.PHONY: fw-build
fw-build: ## Build one board (BOARD=receiver|transmitter_sim)
	./tools/fw-build.sh $(BOARD)

.PHONY: fw-build-all
fw-build-all: ## Build receiver and transmitter simulator
	./tools/fw-build.sh receiver
	./tools/fw-build.sh transmitter_sim

.PHONY: fw-flash
fw-flash: ## Build and flash a board (requires PORT)
	./tools/fw-flash.sh $(BOARD) $(PORT)

.PHONY: fw-monitor
fw-monitor: ## Open the serial monitor
	./tools/fw-monitor.sh $(PORT) $(BAUD)

.PHONY: fw-test
fw-test: ## Run native C++ tests (no hardware needed)
	./tools/fw-test-native.sh

.PHONY: fw-test-hw
fw-test-hw: ## Build the on-target bring-up sketch
	./tools/fw-build.sh bringup

.PHONY: fw-export
fw-export: ## Export receiver OTA image + checksum
	./tools/fw-export-ota.sh

.PHONY: fw-clean
fw-clean: ## Remove firmware build artifacts
	rm -rf firmware/build firmware/.cache firmware/tests/native/build

# ---------------------------------------------------------------------------
# Server
# ---------------------------------------------------------------------------
.PHONY: server-dev
server-dev: ## Run the API locally with autoreload
	./tools/server-dev.sh

.PHONY: server-test
server-test: ## Run API + protocol-schema tests
	./tools/server-test.sh

.PHONY: server-lint
server-lint: ## Ruff + mypy
	./tools/server-lint.sh

.PHONY: server-format
server-format: ## Auto-format Python
	./tools/server-lint.sh --fix

.PHONY: docker-build
docker-build: ## Build the server image
	docker compose -f server/docker-compose.yml build

.PHONY: docker-up
docker-up: ## Start the Dockerised API
	docker compose -f server/docker-compose.yml up -d

.PHONY: docker-down
docker-down: ## Stop the Dockerised API
	docker compose -f server/docker-compose.yml down

.PHONY: docker-logs
docker-logs: ## Follow API container logs
	docker compose -f server/docker-compose.yml logs -f api

# ---------------------------------------------------------------------------
# Cross-cutting
# ---------------------------------------------------------------------------
.PHONY: test
test: fw-test server-test ## Run all host-side tests

.PHONY: lint
lint: server-lint ## Run linters

.PHONY: check
check: test lint ## Test + lint

.PHONY: docs-serve
docs-serve: ## Serve documentation at http://127.0.0.1:8001
	.venv/bin/mkdocs serve -a 127.0.0.1:8001

.PHONY: docs-build
docs-build: ## Build the documentation site
	.venv/bin/mkdocs build --strict

.PHONY: clean
clean: fw-clean ## Remove generated artifacts
	rm -rf site server/data/*.db server/data/firmware .pytest_cache .ruff_cache .mypy_cache
