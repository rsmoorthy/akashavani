# Partition schemes

The build scripts use stock ESP32 core partition tables so that a checkout
builds without modifying the installed toolchain:

| Flash | Scheme | App slots | Notes |
| --- | --- | --- | --- |
| 4 MB | `min_spiffs` | 1.9 MB x2 + 190 KB SPIFFS | default; keeps OTA |
| 4 MB | `no_fs` | 2.0 MB x2 | alternative if no internal FS is needed |
| 16 MB | `app3M_fat9M_16MB` | 3 MB x2 + 9.9 MB FATFS | `AKASHVANI_FLASH_SIZE=16M` |

Select with `AKASHVANI_FLASH_SIZE=16M make fw-build`, or override the full FQBN
with `AKASHVANI_FQBN`.

## Using a custom table

The ESP32 core resolves `build.partitions` from its own `tools/partitions`
directory. To test a custom CSV, copy it into that directory (or the variant
directory as `build.custom_partitions=custom`) and pass the matching option:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom" \
  --build-property "build.custom_partitions=akashvani-16mb" \
  ...
```

Keep the production CSV under version control here before relying on it. OTA
requires two app slots and `otadata`; do not disable OTA to gain application
space without an explicit decision record.
