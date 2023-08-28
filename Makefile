default: build/lookup build/neon_ranges build/neon_lookup input.txt
	time build/lookup input.txt
	time build/neon_ranges input.txt
	time build/neon_lookup input.txt

build:
	mkdir -p build

build/lookup: build main.c
	cc main.c -o build/lookup -O3

build/neon_ranges: build main.c
	cc main.c -o build/neon_ranges -O3 -DNEON_RANGES

build/neon_lookup: build main.c
	cc main.c -o build/neon_lookup -O3 -DNEON_LOOKUP

input.txt: generate.rb
	ruby generate.rb

clean: 
	rm -rf build input.txt

.PHONY: default clean
