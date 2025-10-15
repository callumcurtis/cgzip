# Paths that require cleaning
install-dir = $(shell pwd)/install
build-dir = $(shell pwd)/build

.DEFAULT_GOAL := install

data.tar:
	tar -cf data.tar data

data.tar.%.gz: data.tar
	gzip -$* -c data.tar > $@

.PHONY: test
test: install data.tar data.tar.9.gz data.tar.6.gz data.tar.5.gz data.tar.1.gz
	cmake -S . -B $(build-dir)
	cmake --build $(build-dir) --target test
	$(build-dir)/test/test
	$(install-dir)/bin/cgzip < data.tar > data.tar.gz
	gzip -cd data.tar.gz > data.d.tar
	diff data.d.tar data.tar -s
	@echo "original file size: $$(stat -c%s data.tar)"
	@echo "compressed file size: $$(stat -c%s data.tar.gz)"
	@echo "gzip -9 file size: $$(stat -c%s data.tar.9.gz)"
	@echo "gzip -6 file size: $$(stat -c%s data.tar.6.gz)"
	@echo "gzip -5 file size: $$(stat -c%s data.tar.5.gz)"
	@echo "gzip -1 file size: $$(stat -c%s data.tar.1.gz)"

.PHONE: profile
profile: install data.tar
	$(install-dir)/bin/cgzip < data.tar > data.tar.gz & pid=$$! && flamegraph $$pid > flamegraph.svg && kill $$pi

install:
	cmake -S . -B $(build-dir) -DCMAKE_INSTALL_PREFIX=$(install-dir)
	cmake --build $(build-dir) --target cgzip
	cmake --install $(build-dir)

.PHONY: format
format:
	find app include src test -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs --no-run-if-empty clang-format -i

.PHONY: lint
lint:
	find app include src -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs --no-run-if-empty -n 1 -P 8 clang-tidy -fix -p $(build-dir)

.PHONY: fix
fix: lint format

.PHONY: verify
verify:
	find app include src test -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs --no-run-if-empty clang-format --dry-run --Werror
	git diff --name-only --diff-filter=d HEAD | grep -E 'include/.*\.hpp$$|src/.*\.cpp$$|src/.*\.hpp$$|app/.*\.cpp$$|app/.*\.hpp$$' | xargs --no-run-if-empty -n 1 -P 8 clang-tidy -p $(build-dir)

.PHONY: setup-pre-commit
setup-pre-commit:
	echo "make verify" > .git/hooks/pre-commit
	chmod +x .git/hooks/pre-commit

.PHONY: validate
validate: install
	./scripts/validate

.PHONY: clean
clean:
	rm -rf $(build-dir) \
		$(install-dir)
