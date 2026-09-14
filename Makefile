export CC      := clang
export CXX     := clang++

run: debug
	./build/Debug/bin/editor

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

install: clean debug
	cmake --install build --prefix install

package: clean release
	cmake --install build --prefix install
	tar -czvf roots.tar.gz -C install .

perf: debug
	perf record \
		--debuginfod \
		--call-graph dwarf,2048 \
		./build/editor

	@[ -f `which hotspot` ] && hotspot

reformat:
	find \
		source/* \
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
