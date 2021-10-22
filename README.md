# intel-dimm -  Linux kernel driver for DIMM temperature monitoring on Intel Xeon platforms

intel-dimm is a Linux kernel driver based on the hwmon kernel API.
It provides temperature readings for the DIMM modules, which can be read with lm-sensors:

```
intel_dimm-pci-7ff0
Adapter: PCI adapter
MC_0_CH_A:    +29.0 C
MC_0_CH_B:    +27.0 C
MC_0_CH_C:    +29.0 C
MC_0_CH_D:    +28.0 C
```

The above example is from a Xeon E5-2620 v3, with all 4 memory channels populated.

# Compatibility

This driver is compatible with the following Intel platforms :

* Sandy Bridge LGA 2011 / Xeon E5 and i7 Extreme 3xxx
* Ivy Bridge LGA 2011 / Xeon E5/E7 v2 and i7 Extreme 4xxx
* Haswell LGA 2011-v3 / Xeon E5/E7 v3 and i7 Extreme 5xxx
* Broadwell LGA 2011-v3 / Xeon E5/E7 v4, Xeon D-15xx and i7 Extreme 6xxx
* Skylake LGA 2066 and 3647 / 1st gen Xeon Scalable, Xeon W-21xx and i9 7xxx
* Cascade Lake LGA 2066 and 3647 / 2nd gen Xeon Scalable, Xeon W-22xx and i9 10xxx

Some other platforms using one of the mentionned sockets are also probably compatible, but have not been tested.
This driver is based on the CPU Power Control Unit (PCU) registers, which are not available on the lower end Xeon (E3) and Core platforms.
