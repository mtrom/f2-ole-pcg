## Overview

This repository implements the programmable pseudorandom correlation generator (PCG) for oblivious linear evaluations
(OLE) over the binary field. It includes both the interactive seed exchange protocol and seed expansion.


## Running Evaluations

This project has been containerized with Docker so it suffices to pull the pre-build image and run it directly --- i.e.,
```
docker pull ghcr.io/mtrom/f2-ole-pcg:latest
```
and
```
docker run --rm --network host ghcr.io/mtrom/f2-ole-pcg:latest [ARGS]
```
Use `--help` to see the available arguments.


> [!NOTE]
> The `--network host` flag is necessary if you will be running the protocol between multiple hosts.

## Building the Project

In order to make development easier, there is a `dev` docker target that does not automatically compile the project.
This can be build by specifying the `--target`:
```bash
docker build -t f2-ole-pcg:dev --target dev -f Dockerfile .
```
This will create a Docker image. Now you create a container from that image and attach the repository directory as it's
main directory.
```bash
docker run -it --name pcg-container -v "$(pwd):/home/pcg-user" --network host f2-ole-pcg:dev
```
At this point you will be connected to the container and can build the project. Start with the `libOTe` dependency,
```bash
(cd thirdparty/libOTe; python3 build.py --all --boost --sodium --relic)
```
and then the project
```bash
mkdir build && (cd build; cmake ..) && (cd build; make -j$(nproc))
```
This will compile two binaries: `unit_tests` and `protocol`. Use `--help` for the protocol arguments.
