## Build
To create the docker container
```bash
docker build --no-cache -t scalable-mpc:latest -f Dockerfile .
```

Connecting to the container
```bash
docker run -it --name scalable-mpc-container -v "$(pwd):/home/mpc-user" scalable-mpc:latest
```
