# Bootloader

Thin A/B bootloader for the STM32F407. Reads a boot descriptor from flash, picks the active slot, and jumps to it.

## Flash Layout

| Region | Address | Size | Contents |
|--------|---------|------|---------|
| Sectors 0-2 | `0x08000000` | 48 KB | Bootloader |
| Sector 3 | `0x0800C000` | 16 KB | Boot descriptor |
| Sector 4 | `0x08010000` | 64 KB | Unused |
| Sector 5 | `0x08020000` | 128 KB | Slot A |
| Sector 6 | `0x08040000` | 128 KB | Slot B |

## Boot Flow

```
Read descriptor
  BLANK   -> write default descriptor, jump to slot A
  VALID   -> slot_confirmed == 1       : jump to active slot
             boot_try_count > 0        : decrement, write, jump to active slot
             boot_try_count == 0       : rollback to slot A
  INVALID -> jump to slot A
```

Slots alternate on each OTA update. If the app is running from slot A, the next OTA downloads into slot B and vice versa. The app determines the download target by reading `active_slot` from the boot descriptor.

## Build and Flash

Open in STM32CubeIDE, build, and flash via ST-LINK to `0x08000000`. The linker script limits the bootloader to 48 KB.
