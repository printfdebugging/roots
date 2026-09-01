export CC      := clang
export CXX     := clang++

run: debug
	./build/bin/editor

debug:
	cmake \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX=install \
		-B build && cmake --build build

release:
	cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=install \
		-B build && cmake --build build

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

clean:
	rm -rf build
	rm -rf install
	rm -rf perf*
	rm -rf roots.tar.gz
	rm -rf tags

update:
	git submodule update --remote --merge
