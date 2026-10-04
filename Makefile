export CC      := clang
export CXX     := clang++

run: debug
	./build/Debug/bin/text-editor

debug:
	cmake \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX=install \
		-B build/Debug && cmake --build build/Debug

release:
	cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=install \
		-B build/Release && cmake --build build/Release

check: debug
	cmake --build build/Debug -t check

install: debug
	cmake --install build/Debug --prefix install 
	cmake --install build/Debug --prefix install

licenses:
	@rm -rf install/share/licenses
	@git submodule foreach --quiet 'echo $$sm_path' | while read -r path; do \
		found=$$(ls "$$path"/LICENSE* "$$path"/LICENCE* "$$path"/COPYING* "$$path"/UNLICENSE* 2>/dev/null | head -1); \
		if [ -n "$$found" ]; then \
			mkdir -p "install/share/licenses/$$(basename $$path)"; \
			cp "$$found" "install/share/licenses/$$(basename $$path)/"; \
		else \
			echo "no licence found in $$path"; \
		fi; \
	done

package: clean release licenses
	cmake --install build/Release --prefix install
	tar -czvf roots.tar.gz -C install .

perf: debug
	perf record \
		--debuginfod \
		--call-graph dwarf,2048 \
		./build/editor

	@[ -f `which hotspot` ] && hotspot

reformat:
	find \
		modules/* \
		projects/* \
		-iname '*.h' -o \
		-iname '*.c' -o \
		-iname '*.vert' -o \
		-iname '*.frag' | xargs clang-format -i

clean:
	rm -rf build
	rm -rf install
	rm -rf perf*
	rm -rf roots.tar.gz
	rm -rf tags
	rm -rf .clangd

update:
	git submodule update --remote --merge

init:
	git submodule update --init --recursive
