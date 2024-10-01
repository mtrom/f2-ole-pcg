## Build
To create the docker container
```bash
docker build --no-cache -t primal-dual-pcg:latest -f Dockerfile .
```

Connecting to the container
```bash
docker run -it --name pcg-container -v "$(pwd):/home/mpc-user" --network bridge primal-dual-pcg:latest
```
