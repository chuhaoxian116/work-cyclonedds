# work-cyclonedds

CycloneDDS C++ demos. The CycloneDDS 0.10.5 development package is managed as
a Git submodule and is not rebuilt by this project.

The repository keeps reusable dependencies in `thirdparty/` and all runnable
examples in `samples/`:

```text
.
├── idl/
│   └── DemoMessage.idl
├── src/
│   └── idl/
│       └── CMakeLists.txt
├── samples/
│   └── hello_world/
│       ├── CMakeLists.txt
│       ├── publisher.cpp
│       └── subscriber.cpp
└── thirdparty/
    └── cyclonedds-0.10.5-prebuilt/
```

## Get the source

Clone with the prebuilt dependency:

```bash
git clone --recurse-submodules git@github.com:chuhaoxian116/work-cyclonedds.git
cd work-cyclonedds
```

For an existing clone:

```bash
git submodule update --init --recursive
```

The current prebuilt package contains GNU/Linux x86-64 binaries. A target with
a different operating system or CPU architecture must use a package built for
that platform.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

CMake locates the CycloneDDS dependencies directly inside:

```text
thirdparty/cyclonedds-0.10.5-prebuilt/
├── cyclonedds-install/
└── cyclonedds-cxx-install/
```

## Run

Each program sends or receives five samples by default. Pass a positive integer
to select a different count.

Run the C++ publisher and subscriber:

```bash
# Terminal 1
./build/bin/dds_cpp_subscriber 5

# Terminal 2
./build/bin/dds_cpp_publisher 5
```

The IDL source is stored in `idl/`. Generated C++ sources and headers are kept
in `build/src/idl/`, and the reusable shared library is written to
`build/lib/libdds_idl.so`. Other in-tree targets only need to link `dds_idl`.

Runtime network selection and library paths can be supplied later by the
project's unified startup script through `CYCLONEDDS_URI` and
`LD_LIBRARY_PATH`.
