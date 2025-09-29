# Paths that require cleaning
install-dir = $(shell pwd)/install
build-dir = $(shell pwd)/build
# coverage-dir = $(shell pwd)/coverage

data.tar:
	tar -cf data.tar data

.PHONY: test
test: install data.tar
	cmake -S . -B $(build-dir)
	cmake --build $(build-dir) --target test
	$(build-dir)/test/test
	$(install-dir)/bin/gzip < data/calgary_corpus/book1 > book1.gz
	$(install-dir)/bin/gzip < data.tar > data.tar.gz
	gzip -cd data.tar.gz > data.d.tar
	diff data.d.tar data.tar

.PHONE: profile
profile: install data.tar
	$(install-dir)/bin/gzip < data.tar > data.tar.gz & pid=$$! && flamegraph $$pid > flamegraph.svg

# .PHONY: generate-coverage
# generate-coverage:
# 	cmake -S . -B $(build-dir) -DENABLE_COVERAGE=ON
# 	cmake --build $(build-dir) --target test
# 	$(build-dir)/test/test
# 	mkdir -p $(coverage-dir)
# 	lcov --capture --directory $(build-dir) --output-file $(coverage-dir)/coverage.info --ignore-errors inconsistent
# 	lcov --extract $(coverage-dir)/coverage.info $(this-dir)/src --output-file $(coverage-dir)/src-coverage.info --ignore-errors inconsistent
# 	genhtml $(coverage-dir)/src-coverage.info --output-directory $(coverage-dir) --ignore-errors inconsistent

# .PHONY: coverage
# coverage: generate-coverage
# 	xdg-open http://localhost:8080/index.html
# 	cd $(coverage-dir) && python3 -m http.server 8080

.PHONY: install
install:
	cmake -S . -B $(build-dir) -DCMAKE_INSTALL_PREFIX=$(install-dir)
	cmake --build $(build-dir) --target gzip
	cmake --install $(build-dir)

# .PHONY: format
# format:
# 	find include src test -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs --no-run-if-empty clang-format -i

# .PHONY: lint
# lint:
# 	find include src -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs --no-run-if-empty -n 1 -P 8 clang-tidy -fix -p $(build-dir)

# .PHONY: lint-changed
# lint-changed:
# 	git diff --name-only --diff-filter=d HEAD | grep -E 'include/.*\.hpp$$|src/.*\.cpp$$|src/.*\.hpp$$' | xargs --no-run-if-empty -n 1 -P 8 clang-tidy -fix -p $(build-dir)

# .PHONY: fix
# fix: lint-changed format

# .PHONY: verify
# verify:
# 	find include src test -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs --no-run-if-empty clang-format --dry-run --Werror
# 	$(MAKE) generate-coverage
# 	lcov --summary $(coverage-dir)/src-coverage.info --fail-under-lines 100 --ignore-errors inconsistent
# 	git diff --name-only --diff-filter=d HEAD | grep -E 'include/.*\.hpp$$|src/.*\.cpp$$|src/.*\.hpp$$' | xargs --no-run-if-empty -n 1 -P 8 clang-tidy -p $(build-dir)

# .PHONY: setup-pre-commit
# setup-pre-commit:
# 	echo "make verify" > .git/hooks/pre-commit
# 	chmod +x .git/hooks/pre-commit

.PHONY: clean
clean:
	rm -rf $(build-dir) \
		$(install-dir)
#   $(coverage-dir)

