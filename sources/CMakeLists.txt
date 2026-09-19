Require(GTest VERSION ">=1.17.0" TEST CONAN gtest COMPONENTS gtest gtest_main gmock gmock_main)
Require(benchmark VERSION ">=1.9.0" BENCH CONAN benchmark)

Require(oxbox VERSION ">=0.28.1" CONAN oxbox)
Require(xxHash VERSION ">=0.8.3" CONAN xxhash)
Require(libzip VERSION ">=1.11.4" CONAN libzip)
Require(OpenCV VERSION ">=4.14.0" CONAN opencv)
Require(ffmpeg VERSION ">=9.0.1" CONAN ffmpeg)
Require(SQLite3 VERSION ">=3.53.4" CONAN sqlite3)

# The core the tests dlopen and `tash run` loads from beside the binary.
Require(libretro-cores VERSION ">=2026.9.12" CONAN libretro-cores)

# The scenario interpreter, embedded in the cli.
Require(Python3 VERSION ">=3.14" SYSTEM
        COMPONENTS Interpreter Development.Embed)
Require(pybind11 VERSION ">=3.0" SYSTEM)

Scan_subdirectories()
