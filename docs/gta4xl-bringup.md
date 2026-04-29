# gta4xl Bring-Up Notes

Target: Samsung Galaxy Tab S6 Lite 2020 LTE/Wi-Fi family (`gta4xl`,
`gta4xlwifi`), Exynos 9611.

Reference source used for the initial target data:

- `LineageOS/android_kernel_samsung_gta4xl`, branch `lineage-23.2`, commit
  `8d1f7a68b0d0eef5aa6a12479429333e27482673`.
- Common board DTS:
  `arch/arm64/boot/dts/exynos/exynos9611-gta4xl_common.dtsi`.
- Base SoC DTS: `arch/arm64/boot/dts/exynos/exynos9610.dts`.
- Display DTS:
  `arch/arm64/boot/dts/samsung/display-lcd_gta4xl_common.dtsi`.

Current assumptions:

- Exynos 9611 is treated as register-compatible with the existing
  `maestro9610` platform code for first-stage lk3rd bring-up.
- Boot storage is UFS. The gta4xl DTBO bootargs use
  `androidboot.boot_devices=13520000.ufs`.
- Hardware keys are from the gta4xl `gpio-keys` node: volume-down is `gpa1-6`,
  volume-up is `gpa1-5`, and power is `gpa1-7`.
- USB DWC3 is at `0x13200000`; USB2 PHY is at `0x131D0000`; DWC3 IRQ is SPI
  `186`; the USB PHY PMU control offset is `0x704`.
- Graphical fastboot is preserved through the stock framebuffer handoff region
  at `0xCA000000`, with the DTS-provided 1200x2000 geometry.

Known blockers before real-device flashing:

- Panel bring-up is not ported. The DTBO identifies an HX83102E panel at
  1200x2000 with reset on `gpg1-3`, buck enable on `gpg1-2`, and LDO enable on
  `gpg2-4`; lk3rd currently has no HX83102E DSI command sequence, so drawing
  depends on a valid bootloader framebuffer.
- The boot image arguments are best-effort and should be verified against a
  stock `boot.img` from the exact SM-P610/SM-P615 firmware to avoid a loader
  address mismatch.
- USB PHY tuning is based on the existing Exynos 9610 fastboot path and the
  base DTS. Confirm enumeration on hardware before relying on fastboot.
- Board ID/revision matching for DTBO selection is still inherited from the
  9610 code and may need adjustment after dumping BL_SYS_INFO on hardware.
