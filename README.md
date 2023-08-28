# yarp-identifiers

In the [https://github.com/ruby/yarp](yarp) project, we need to parse identifiers in Ruby source. Frequently our parsing of identifiers shows up in our performance profiles (because identifiers are very common in Ruby code). In order to parse them, you start at some starting point (a letter or an underscores) and then read as many letters, digits, and underscores as you can. In Ruby this is actually encoding dependent, as you can encode your source in dozens of encodings. We want to only bother attempting to optimize the default path here (`UTF-8`).

In YARP, we use a lookup table to determine if a character is a letter, digit, or underscore. This is a 256 byte table, and we index into it with the character's byte value.

This repository is exploring other approaches, like using SIMD instructions to do the lookup or using SIMD instructions to check against ranges of characters. At the moment it is only attempting to do this with ASCII.

To compile and run the code, run `make`.
