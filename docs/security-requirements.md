# Security Requirements: linux-rpi

Record required by ISOP-11 SD-02 (ISO/IEC 27001:2022 A.8.26). Created and amended by
pull request; the PR approval is the record.

**Reviewed:** commit `dee4e46` (watter-apollo-6.12, kernel release
`6.15.2-watter-6.12-rpi-2712`), 2026-09-30, prompt v2. Previous record: none.

**Scope:** WATTER's kernel for the "apollo" board, the Raspberry Pi 5 controller of the
WATTER water heater. It is built by `watter-build apollo` into the Debian packages
`linux-image-apollo` and `linux-headers-apollo` (`scripts/package/mkdebian:155`, `:212`,
`:235`). The package installs the kernel, modules and DTBs, plus the overlays under
`/usr/lib/linux-image-<release>/` (`scripts/package/builddeb:35-36`). In scope are the
74 non-merge commits by `@watter.com` authors since merge-base `fc85704c` (Linux 6.15.2):
73 by bcollins and 1 by tjsmith (`watter-build` cross-compile). They cover:
- the new `pcf8593-ctr` pulse-counter driver;
- `mcp9600` driver updates (MCP9601 support, filter controls);
- the IIO `status` and `filter` info types and `IIO_VAL_EMPTY`;
- small `emc2305` and 1-wire naming changes;
- the `watter-apollo-rev1`, `watter-squid`, `ad7998` and `mcp23017` overlays;
- the `watter-configs/apollo` kernel config;
- `watter-build` and changes to `scripts/package/`.

Everything else in the tree is vendored and was not reviewed: upstream Linux 6.15.2, the
Raspberry Pi downstream patches (rebased onto 6.15.2), and the upstream `.github/`
workflows. The upstream `ad799x`, `tmp117` and `emc2305` drivers are used unmodified,
apart from the `emc2305` change above. The code that reads the sensors and drives the
actuators is in watter-collector; its record covers the controller's user space.

**Zone:** WATTER-Guard, profile Residential by evidence: this is the controller that
watter-collector runs on. UNKNOWN — product owner: is the apollo kernel also deployed in
the MEGAWATT profile? This is the same question as in the watter-collector record.
**CUI:** none.

The repository is public. Facts from outside the code are from benmcollins,
2026-09-30, during the SEC-93 rollout.

## 1. Data handled and classification

| Data | Persists / transits | Where (path, mode, retention / protocol, destination) | Class |
|---|---|---|---|
| Sensor readings: thermocouple temperatures (`shroud-ambient`, `water-loop`, `coolant`), cold-junction and board-ambient temperatures, hot-water flow pulse count, 1-wire temperatures | Transit only; the kernel keeps no history | IIO and hwmon sysfs attributes, read on demand over I2C and 1-wire (`watter-apollo-rev1-overlay.dts:56-110`). Read attributes are mode 0444 (`drivers/iio/industrialio-core.c:1135-1137`). The flow counter is 6 BCD digits and is reset at probe (`drivers/iio/adc/pcf8593-ctr.c:33`, `:205`) | Confidential (flow reveals occupancy; tied to a customer site) |
| Sensor configuration: thermocouple type, EMA filter on or off and its coefficient, alert thresholds, labels, counter scale | Persists in the overlay; runtime changes last until reboot | The overlay sets type, label and scale at boot (`watter-apollo-rev1-overlay.dts:72-110`, `ad7998-overlay.dts:22-29`). Runtime writes go to the sensor chip only (`mcp9600.c:149-167`) | Internal |
| Actuator and status GPIO lines: `heat-element`, `pc-contactor`, `pc-power-button`, `pc-reset`, `valve-actuate`, pumps, `valve-opened` and `valve-closed` | Transit only | Named in device tree only; no kernel driver claims them (`watter-apollo-rev1-overlay.dts:151-168` on the MCP23017 expander, `watter-squid-overlay.dts:12-40` on RP1). They are read and set through the GPIO character device and sysfs (`watter-configs/apollo:2604-2605`) | Confidential (appliance state at a customer site) |
| Sensor hardware identifiers: 1-wire ROM ids, I2C chip ids | Transit only | 1-wire slave names in sysfs, now prefixed with the bus master name (`drivers/w1/slaves/w1_therm.c:1032-1041`). The MCP960x id appears in a kernel warning only on a mismatch (`mcp9600.c:660-667`) | Internal |
| Kernel log | Persists | printk ring buffer in RAM. By default any local user can read it (`watter-configs/apollo:5452`, `SECURITY_DMESG_RESTRICT` not set), unless a sysctl on the image restricts it. On the RPi it goes on to journald, with the retention in the watter-collector record. pstore keeps kmsg and console output after a crash only if pstore/blk is given a device (`watter-configs/apollo:5369-5381`, `PSTORE_BLK_BLKDEV=""`) | Internal |
| Kernel config, kernel image, modules, DTBs and overlays | Persists | `watter-configs/apollo`. The `.deb` is built on the engineer's host in `build-apollo/` (`watter-build:36`); nothing in this repo signs it or publishes it | Internal (build metadata) |
| Source code | Persists | Public GitHub fork `WATTER-Technology-Solutions/linux-rpi`. ISWI-08-01 classes source code as Confidential. UNKNOWN — product owner: is the public visibility intended and approved? | Public today |

The WATTER changes handle no credentials, keys or tokens. The kernel config does not set
`SYSTEM_TRUSTED_KEYS` or a module signing key (`watter-configs/apollo:5744`, `:768`).

## 2. Who may access the data, and how that is enforced

| Path | Mechanism in the code today |
|---|---|
| Sensor readings (sysfs, read) | File mode 0444: any local user and any process. `mcp9600` hot-junction reads busy-wait up to 1 s for the chip to be ready (`mcp9600.c:274-283`). |
| Sensor configuration (sysfs, write) | File mode 0200, owned by root. udev rules on the image could change this (outside this repo; UNKNOWN below). The writable attributes and their effects are: <br>- `mcp9600` `filter_type` (`none` or `ema`) and `in_temp_filter_low_pass_3db_frequency` (one of 7 listed values) reconfigure the chip (`mcp9600.c:205-237`, `:354-383`). <br>- `pcf8593-ctr` `in_count0_raw`: any write stops and zeroes the hot-water flow counter; the value written is ignored (`pcf8593-ctr.c:81-94`, `:155-175`). <br>- Upstream `tmp117` `calibbias` shifts the ambient reading that drives fan control through the `watter-thermal` zone (`watter-apollo-rev1-overlay.dts:114-148`). With `THERMAL_EMULATION=y` (`watter-configs/apollo:2993`), root can also set a fake zone temperature. <br>- `mcp9600` alert threshold and hysteresis attributes exist only when alert interrupts are declared in DT; the apollo overlay declares none. <br>The thermocouple type cannot be written through sysfs; it comes from DT only (`mcp9600.c:315-317`, `:678-688`). |
| Actuator GPIOs | The GPIO character device and sysfs GPIO are built in (`watter-configs/apollo:2604-2605`). The lines are not claimed by any driver, so any process that can open the GPIO chip can switch the heat element, contactor, pumps, valve and PC power. The kernel applies no interlock. The device-node owner and mode come from udev rules on the image (outside this repo). |
| Raw hardware buses and memory | `/dev/i2c-*` is built in (`watter-configs/apollo:2401`) and gives direct access to every sensor and to the MCP23017 expander, bypassing the drivers. `/dev/mem` is enabled without `STRICT_DEVMEM` (`:2373`, `:6116`), so root can read and write all physical memory. |
| Kernel integrity | Module signing is off (`:768`) and the lockdown LSM is off (`:5475`), so root can load any module. AppArmor is built in (`:5465`), but `CONFIG_LSM=""` (`:5485`) leaves it inactive unless the boot command line enables it. The boot command line comes from the bootloader (`:552-553`); KGDB is not built (`:5961`). Magic SysRq is enabled, including over the serial console (`:5955-5957`). UNKNOWN — IT: does the production boot command line set `lsm=` or `security=` to enable AppArmor, and does it enable a serial console? |
| Boot partition and overlays | Whoever can write the boot files chooses the overlays, sensor types, labels and GPIO names. Runtime overlays are possible through configfs (`:1798`). Both paths require root. |
| Network | The WATTER changes add no listeners. The config builds the TUN module (`:1939`). |
| Local users | Non-root users can read every sensor value and the kernel log. Root can change sensor configuration, reset the flow counter, and drive the actuators. Membership of groups that own the GPIO, I2C and IIO device nodes is set on the image, not here. UNKNOWN — IT: on production images, which users and groups can open `/dev/gpiochip*`, `/sys/class/gpio`, `/dev/i2c-*` and `/dev/iio:device*`, and do any udev rules change the owner or mode of IIO sysfs attributes? |
| Build and distribution | `watter-build` runs as the engineer, in a single-user container or VM destroyed after use (known fact). It reuses an existing `build-apollo/.config` in place of `watter-configs/apollo` if one is present (`watter-build:46-49`). Nothing here signs the `.deb`. UNKNOWN — infra owner: how does `linux-image-apollo` reach devices (a package archive, an image bake, or something else), and is it signed on that path? |
| CI | The upstream workflows `kernel-build`, `dtoverlaycheck` and `kunit` run only on `rpi-*` branches (`.github/workflows/kernel-build.yml:3-11`), so they never build `watter-rpi-6.15.y`. Only the advisory `checkpatch` runs on pull requests. The repository has no Actions secrets, variables or environments, and no deploy credentials. |
| Source code | Public repository: anyone can read it. Write access: 2 admins (benmcollins, tjsmith3) and 2 with write. `watter-rpi-6.15.y` has no branch protection and no rulesets, so SD-02 and SD-05 review before merge rest on convention, not enforcement. |

## 3. Events that must be logged

The kernel writes to the printk ring buffer. On the RPi that goes to journald, with the
retention given in the watter-collector record. A crash dump survives a reboot only if
pstore/blk is configured with a device (UNKNOWN — IT: is pstore/blk given a block device
on production images, or is `PSTORE_BLK_BLKDEV` left empty as in `watter-configs/apollo:5377`?).

| Event | Required | Today |
|---|---|---|
| Authentication success | Not applicable: the WATTER kernel changes implement no authentication. | Not applicable. Kernel audit is built (`watter-configs/apollo:45-47`); whether auditd records logins is set on the image. |
| Authentication failure | Not applicable (as above). | Not applicable. |
| Privilege change | Not applicable to the WATTER code. | Kernel audit available (`:45-47`); rules are outside this repo. |
| Configuration change | Every runtime sensor reconfiguration, with attribute, old value, new value and requester: filter type and coefficient, flow-counter reset, `calibbias`, thermal emulation. Also module loads. | Not logged. `mcp9600` filter writes, `pcf8593-ctr` counter resets and upstream `calibbias` writes print nothing. Only boot-time configuration is logged: `pcf8593-ctr.c:209` and `emc2305.c:326`, `:389`, `:398`. |
| Actuator changes (heat element, contactor, valve, pumps, PC power) | Line, new value and requester | Not logged by the kernel; the GPIO character device records nothing. User space (watter-collector) is the only record. |
| Sensor faults | I2C read failures, conversion timeouts, thermocouple open or short circuit | `mcp9600.c:282` logs a timeout without rate limiting (and without a trailing newline), and the read then continues. `mcp9600.c:162` logs a failure to configure. `pcf8593-ctr.c:145-146` and `:168-169` return `-EIO` without logging. The MCP9601 open-circuit and short-circuit status is not reported: the IIO `status` info type is defined (`include/linux/iio/types.h:73`), but no driver uses it. |

**Must not be logged:** credentials, keys and sensor payload history. No violation found:
the WATTER drivers log only chip ids, I2C probe results and fixed messages. By default the kernel
log is readable by every local user (`SECURITY_DMESG_RESTRICT` not set), and any local
user can trigger the unthrottled `mcp9600` timeout message by reading the sensor.
