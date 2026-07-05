# Project 06 — Important Notes

## Project Origin

This project was **imported from Project 05** (standalone HID keyboard) and modified manually to add CDC and Vendor interfaces.
It was NOT created fresh from CubeMX — the USB Middleware configuration was edited by hand, not through the wizard.

---

## Warning: Do NOT fully trust the .ioc file

The `.ioc` file only reflects the CubeMX configuration at the time the original project (Project 05) was created.
All USB descriptor, endpoint, and interface changes were made **manually afterward** and are **unknown to CubeMX**.

If you click **"Generate Code"** again from CubeMX, the following files will be **overwritten and all manual changes lost**:

| File overwritten | Manual content that will be lost |
|------------------|----------------------------------|
| `USB_Device/App/usbd_composite.c` | Full composite descriptor (4 interfaces, IAD, endpoint map), DataIn/DataOut/Setup callbacks, EP4 bulk pipeline, Device Qualifier descriptor with class `0xEF/0x02/0x01` |
| `USB_Device/App/usbd_composite.h` | Interface number defines, endpoint address defines, extern declarations |
| `USB_Device/Target/usbd_conf.c` | EP4 open/close, PMA allocation for all 5 endpoints |
| `USB_Device/Target/usbd_conf.h` | `USBD_MAX_NUM_INTERFACES`, `USBD_MAX_NUM_CONFIGURATION` |
| `USB_Device/App/usbd_desc.c` | Product string, `bDeviceClass=0xEF`, `bDeviceSubClass=0x02`, `bDeviceProtocol=0x01` |

All files under `Core/Src/` and `Core/Inc/` that were created manually may also be **removed from the project** if CubeMX regenerates the project tree and does not include them.

---

## Checklist if regeneration is unavoidable

1. **Device descriptor class triple**: must be `0xEF / 0x02 / 0x01` — CubeMX generates `0x00 / 0x00 / 0x00`
2. **Product string**: `"STM32 USB Composite Keypad+CDC+Bulk"` — CubeMX may reset it to the default
3. **PMA buffer allocation** in `usbd_conf.c`: all 5 endpoints must be allocated correctly, not left at the HID-only default (2 endpoints)
4. **`USBD_MAX_NUM_INTERFACES = 4`** in `usbd_conf.h` — CubeMX generates `1`
5. **IAD descriptor** in the configuration descriptor — CubeMX does not generate IAD for this composite setup
6. **EP4 IN open** in `USBD_LL_SetupStage` / `USBD_LL_Reset` — must be opened manually

---

## Safe workflow for clock or GPIO changes

Use CubeMX only for changes **unrelated to USB** (clock, GPIO, timer, etc.):

1. Open the `.ioc` file and make the required changes
2. Before generating: **back up** the entire `USB_Device/` folder and all `Core/Src/usbd_*` files
3. Generate code
4. Diff the output and **manually restore** the backed-up USB files
5. Re-verify `usbd_conf.h` and `usbd_desc.c`
