## Gif on Display

to enable gif support, you need to install and then enable it. Use `idf.py add-dependency "lvgl/lvgl^8"` to install it and run `idf.py menuconfig`.

```shell
Component config
└── LVGL configuration
    └── LVGL 3rd Party Libraries
        ├── [*] PNG decoder
        ├── [*] GIF decoder
        └── [*] File system on top of stdio API
```