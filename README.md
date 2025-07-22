# SMS

Since I failed successfully installing the `esp-idf` tools to my NixOS, this is the manual approach of using the cli tool directly via the `nix-direnv`.

## Allow the direnv for this repo

```shell
direnv allow
```

## to setup a new project

```shell
export NAME=
idf.py create-project $NAME
chmod -R +rw $NAME
cd $NAME
idf.py set-target esp32s3
```

## Build, Flash, and Monitor

To get the esp32 in boot mode, hold down the boot button while connecting the USB cable.

```shell
idf.py build flash monitor
```

## Install idf packages

```shell
idf.py add-dependency "espressif/mpu6050^1.2.0"
```
