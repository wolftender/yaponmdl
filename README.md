# yaponmdl
A Patapon graphics file format viewer written in C++. This application natively supports file formats used by Patapon franchise on the PSP. Written in wxWidgets and OpenGL 4.0.

## Features
This application supports following features:
 - Load and parse native Patapon file formats (SCE GMO models and Patapon GXX models).
 - OpenGL-based hardware accelerated rendering of textures, models and animations.
 - Automatic matching of GXT textures (includes also PNG/JPG fallback using stb_image library).

## Research and documentation
Currently, most of the documentation exists only as comments within the code. This is, of course, a subject to change. You can view the current documentation in the [docs directory](./docs/README.md).

## Acknowledgements
This software was made possible thanks to:
 - owocek from Patapon Modding community - provided various research samples necessary to implement the file format parsers, as well as [GXTEdit](https://github.com/owodzeg/GXTEdit) that allowed to kick off the research on GXP file format.
 - GXXTool from Patapon Modding community - very helpful tool for GXX research

## Open source libraries used
Libraries used for development:
 - [glm](https://github.com/g-truc/glm) - used for all kinds of linear algebra
 - [stb](https://github.com/nothings/stb) - absolutely amazing single-header library suite used for PNG/JPEG support, TTF rendering and font atlash packing
 - [wxWidgets](https://github.com/wxWidgets/wxWidgets) - a C++ GUI library providing very pleasing results with minimal clutter and easy static linking on Windows
 - [glad](https://github.com/dav1dde/glad) - used to obtain OpenGL function pointers
 - [fmt](https://github.com/fmtlib/fmt) - used for string formatting across the codebase

## Build instructions
This project uses CMake as the build system. If you are familiar with CMake you can easily figure out how to build this project.

### Windows build instructions
The recommended compiler that this software was developed with is clang. You can obtain your copy of clang on Windows using ``winget``:
```
winget install --id LLVM.LLVM --source winget
```

You will also need need a copy of Microsoft Build Tools and Windows SDK from [the official Microsoft website](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/).

Additionally, you need a way to compile the wxWidgets library. In this setup guide we will use [vcpkg](https://github.com/microsoft/vcpkg) package manager to install the library.

Install wxWidgets using this command
```
vcpkg install wxwidgets:x64-windows-static
```

Now in the root of the project directory create a ``CMakeUserPresets.json`` file to point CMake to your vcpkg installation:
```json
{
    "version": 2,
    "configurePresets": [
        {
            "name": "default",
            "inherits": "win-clang-x64-debug",
            "environment": {
                "VCPKG_ROOT": "C:\\my\\vcpkg\\path"
            }
        },
        {
            "name": "default-release",
            "inherits": "win-clang-x64-release",
            "environment": {
                "VCPKG_ROOT": "C:\\my\\vcpkg\\path"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "default-release",
            "configurePreset": "default-release"
        }
    ]
}
```

Now you can just run configure and build
```
cmake --preset default-release
cmake --build --preset default-release
```
