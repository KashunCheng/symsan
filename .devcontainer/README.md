# SymSan Dev Container

This Dev Container configuration is based on the project's Dockerfile and provides a development environment for the SymSan project.

## Features

- Based on Ubuntu Noble
- LLVM/Clang 14
- Pre-installed AFLplusplus v4.31c
- All necessary development dependencies included
- python packages included
- Configured with C/C++ development tool extensions

## Usage

1. Ensure Docker and VS Code Dev Containers extension are installed
2. Open the project in VS Code
3. Press F1 and select "Dev Containers: Reopen in Container"
4. Wait for the container to build

## Building the Project

just open it.

## Environment Variables

- `KO_CC=clang-14`
- `KO_CXX=clang++-14`
- `KO_USE_FASTGEN=1`

## Important Notes

⚠️ **Do not upgrade dependency versions!** This project depends on specific versions of toolchains (LLVM 14, etc.). Upgrading may cause unpredictable issues.
