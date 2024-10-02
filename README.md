## Build
To create the docker image
```bash
docker build --no-cache -t primal-dual-pcg:latest -f Dockerfile .
```

Connecting to the container run the following from within this directory:
```bash
docker run -it --name pcg-container -v "$(pwd):/home/pcg-user" --network host primal-dual-pcg:latest
```

Once connected to the container:
```
./build.sh
```

## Run
The `primal-dual-pcg/build/` directory will have two binaries: `unit_tests` and `protocols`, the latter of which has a
help message if given `--help`.
