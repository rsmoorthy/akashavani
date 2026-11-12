# Partition schemes

The receiver sketch carries its own 4 MB table, which the ESP32 core copies into
the build directory before compiling:

| File | Flash | App slots | Notes |
| --- | --- | --- | --- |
| `firmware/sketches/receiver/partitions.csv` | 4 MB | `0x1E0000` x2 (1,966,080 B = 1.875 MiB each) | default; 128 KiB SPIFFS + 64 KiB coredump |
| stock `min_spiffs` | 4 MB | 1,966,080 B x2 | identical layout, used when the sketch has no `partitions.csv` |
| stock `no_fs` | 4 MB | 2.0 MB x2 | alternative if no internal FS is needed |
| stock `app3M_fat9M_16MB` | 16 MB | 3 MB x2 + 9.9 MB FATFS | `AKASHVANI_FLASH_SIZE=16M` |

`arduino-cli` pre-build hooks look for `partitions.csv` in this order: sketch
source directory, variant directory, core `tools/partitions`. That is why the
receiver table does not have to be copied into the installed toolchain.

## Layout

```text
nvs       0x9000   0x5000
otadata   0xE000   0x2000
app0      0x10000  0x1E0000   OTA slot 0
app1      0x1F0000 0x1E0000   OTA slot 1
spiffs    0x3D0000 0x20000
coredump  0x3F0000 0x10000
```

The receiver image is currently about 1.23 MB, so the 1.875 MiB slot leaves
roughly 740 KB for future code. OTA requires two app slots and `otadata`; do not
disable OTA to gain application space without an explicit decision record.
