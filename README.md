Since I failed successfully installing the `esp-idf` tools to my NixOS, this is the manual approach of using the cli tool directly via the `nix-direnv`.

### Allow the direnv for this repo:
```shell
direnv allow
```

### to setup a new project:
```shell
idf.py create-project speaker
cd speaker
idf.py set-target esp32s3
```

### Build, Flash, and Monitor
```shell
idf.py build flash monitor
```