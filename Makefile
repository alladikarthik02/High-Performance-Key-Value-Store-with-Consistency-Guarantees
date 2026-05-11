# Makefile (optional convenience)
IMAGE ?= kvstore:latest

.PHONY: docker-build up down logs seed cli

docker-build:
	docker build --build-arg BUILD_TESTS=OFF -t $(IMAGE) .

up:
	docker compose up --build -d

down:
	docker compose down -v

logs:
	docker compose logs -f --tail=200

seed:
	bash scripts/seed.sh kv1 kv1:50051 50

cli:
	@echo 'Examples:'
	@echo '  SVC=kv1 ADDR=kv1:50051 scripts/kvctl.sh put user:1 karthik'
	@echo '  SVC=kv1 ADDR=kv1:50051 scripts/kvctl.sh get user:1'
