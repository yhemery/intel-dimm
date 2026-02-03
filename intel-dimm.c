#include <linux/module.h>
#include <linux/pci.h>
#include <linux/hwmon.h>
#include <linux/err.h>

#define PCI_DEVICE_ID_INTEL_SANDY_BRIDGE_PCU_F0	0x3cc0
#define PCI_DEVICE_ID_INTEL_IVY_BRIDGE_PCU_F0	0x0ec0
#define PCI_DEVICE_ID_INTEL_SKYLAKE_PCU_F0		0x2080
#define PCI_DEVICE_ID_INTEL_ICE_LAKE_PCU_F2		0x345a
#define PCI_DEVICE_ID_INTEL_SAPPHIRE_RAPIDS_F2	0x325a

/* Haswell and Broadwell might have one or two IMC per socket :
 * One IMC  : All 4 channels available in dev 30 func 0. Dev 30 func 4
 *            will still be available, but return all zeroes
 * Two IMCs : 2 channels available in dev 30 func 0, the two others in
 *            dev 30 func 4
 */
#define PCI_DEVICE_ID_INTEL_HASWELL_PCU_F0		0x2f98
#define PCI_DEVICE_ID_INTEL_HASWELL_PCU_F4		0x2f9c
#define PCI_DEVICE_ID_INTEL_BROADWELL_PCU_F0	0x6f98
#define PCI_DEVICE_ID_INTEL_BROADWELL_PCU_F4	0x6f9c

struct dimm_channel {
	const char *label;  /* Sensor label */
	u8 pci_reg_offset;  /* Temperature offset in PCU register */
};

struct intel_dimm_pcu_capas {
	u16 pcu_dev_id;		/* PCU device id */
	const struct dimm_channel *dimm_channels;
};

/* 4 channels CPUs starting from Sandy Bridge, up to Broadwell */
static const struct dimm_channel DIMM_CHANNELS_SANDY_BRIDGE_MC0[] = {
	{"MC_0_CH_A", 0x60}, {"MC_0_CH_B", 0x61}, /* Always present */
	{"MC_0_CH_C", 0x62}, {"MC_0_CH_D", 0x63}, /* Disabled for SKUs with two IMCs */
	{NULL, 0},
};

/* Broadwell / Haswell specific */
static const struct dimm_channel DIMM_CHANNELS_HASWELL_MC1[] = {
	{"MC_1_CH_C", 0x60}, {"MC_1_CH_D", 0x61}, /* Only used for SKUs with two IMCs */
	{NULL, 0},
};

/* 6 channels CPUs starting from Skylake, up to Cascade Lake.
 * 2 IMCs with 3 channels each */
static const struct dimm_channel DIMM_CHANNELS_SKYLAKE[] = {
	{"MC_0_CH_A", 0x94}, {"MC_0_CH_B", 0x95}, {"MC_0_CH_C", 0x96},
	{"MC_1_CH_D", 0x98}, {"MC_1_CH_E", 0x99}, {"MC_1_CH_F", 0x9a},
	{NULL, 0},
};

/* 8 channels CPUs starting from Ice Lake, up to Emerald Rapids.
 * 4 IMc with 2 channels each */
static const struct dimm_channel DIMM_CHANNELS_ICE_LAKE[] = {
	{"MC_0_CH_A", 0xc8}, {"MC_0_CH_B", 0xc9},
	{"MC_1_CH_C", 0xcc}, {"MC_1_CH_D", 0xcd},
	{"MC_2_CH_E", 0xd0}, {"MC_2_CH_F", 0xd1},
	{"MC_3_CH_G", 0xd4}, {"MC_3_CH_H", 0xd5},
	{NULL, 0},
};

static const struct intel_dimm_pcu_capas intel_dimm_pcu_capas_table[] = {
	{PCI_DEVICE_ID_INTEL_SANDY_BRIDGE_PCU_F0,	DIMM_CHANNELS_SANDY_BRIDGE_MC0},
	{PCI_DEVICE_ID_INTEL_IVY_BRIDGE_PCU_F0,		DIMM_CHANNELS_SANDY_BRIDGE_MC0},
	{PCI_DEVICE_ID_INTEL_HASWELL_PCU_F0,		DIMM_CHANNELS_SANDY_BRIDGE_MC0},
	{PCI_DEVICE_ID_INTEL_BROADWELL_PCU_F0,		DIMM_CHANNELS_SANDY_BRIDGE_MC0},
	{PCI_DEVICE_ID_INTEL_HASWELL_PCU_F4,		DIMM_CHANNELS_HASWELL_MC1},
	{PCI_DEVICE_ID_INTEL_BROADWELL_PCU_F4,		DIMM_CHANNELS_HASWELL_MC1},
	{PCI_DEVICE_ID_INTEL_SKYLAKE_PCU_F0,		DIMM_CHANNELS_SKYLAKE},
	{PCI_DEVICE_ID_INTEL_ICE_LAKE_PCU_F2,		DIMM_CHANNELS_ICE_LAKE},
	{PCI_DEVICE_ID_INTEL_SAPPHIRE_RAPIDS_F2,	DIMM_CHANNELS_ICE_LAKE},
};

struct intel_dimm_drv_data {
	struct pci_dev *pdev;

	const struct intel_dimm_pcu_capas *pcu_capas;
	u8 channels_enabled;
};

static int intel_dimm_get_pcu_capas(struct intel_dimm_drv_data *drv_data,
									unsigned short pci_dev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(intel_dimm_pcu_capas_table); i++) {
		const struct intel_dimm_pcu_capas *entry = &intel_dimm_pcu_capas_table[i];

		if (entry->pcu_dev_id == pci_dev) {
			drv_data->pcu_capas = entry;
			return 0;
		}
	}

	return -ENODEV;
}

static ssize_t intel_dimm_read_channel_temp(struct device *dev,
											unsigned int channel)
{
	struct intel_dimm_drv_data *drv_data = dev_get_drvdata(dev);
	const struct dimm_channel *dimm_channel = &drv_data->pcu_capas->dimm_channels[channel];
	u8 val;

	pci_read_config_byte(drv_data->pdev, dimm_channel->pci_reg_offset, &val);

	return val;
}

static int intel_dimm_get_channels_status(struct intel_dimm_drv_data *drv_data,
											struct device *dev)
{
	int i;
	ssize_t channel_status;

	drv_data->channels_enabled = 0;

	for (i = 0; drv_data->pcu_capas->dimm_channels[i].label; i++) {
		channel_status = intel_dimm_read_channel_temp(dev, i);
		if (channel_status != 0)
			drv_data->channels_enabled |= BIT(i);
	}

	if (drv_data->channels_enabled == 0)
		return -ENODEV;

	return 0;
}

static umode_t intel_dimm_hwmon_is_visible(const void *_data, enum hwmon_sensor_types type,
											u32 attr, int channel)
{
	const struct intel_dimm_drv_data *drv_data = _data;

	if (drv_data->channels_enabled & BIT(channel))
		return 0444;

	return 0;
}

static int intel_dimm_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
									u32 attr, int channel, long *val)
{
	*val = intel_dimm_read_channel_temp(dev, channel) * 1000;
	return 0;
}

static int intel_dimm_hwmon_labels(struct device *dev, enum hwmon_sensor_types type,
									u32 attr, int channel, const char **str)
{
	struct intel_dimm_drv_data *drv_data = dev_get_drvdata(dev);
	const struct dimm_channel *dimm_channel = &drv_data->pcu_capas->dimm_channels[channel];

	*str = dimm_channel->label;

	return 0;
}

static const struct hwmon_channel_info *intel_dimm_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp,
					   HWMON_T_INPUT | HWMON_T_LABEL,
					   HWMON_T_INPUT | HWMON_T_LABEL,
					   HWMON_T_INPUT | HWMON_T_LABEL,
					   HWMON_T_INPUT | HWMON_T_LABEL,
					   HWMON_T_INPUT | HWMON_T_LABEL,
					   HWMON_T_INPUT | HWMON_T_LABEL),
	NULL
};

static const struct hwmon_ops intel_dimm_hwmon_ops = {
	.is_visible = intel_dimm_hwmon_is_visible,
	.read = intel_dimm_hwmon_read,
	.read_string = intel_dimm_hwmon_labels,
};

static const struct hwmon_chip_info intel_dimm_hwmon_chip_info = {
	.ops = &intel_dimm_hwmon_ops,
	.info = intel_dimm_hwmon_info,
};

static int intel_dimm_probe(struct pci_dev *pdev,
							const struct pci_device_id *id)
{
	int res;
	struct intel_dimm_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	struct device *hwmon_dev;

	drv_data = devm_kzalloc(dev, sizeof(struct intel_dimm_drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	dev_set_drvdata(dev, drv_data);
	drv_data->pdev = pdev;

	res = intel_dimm_get_pcu_capas(drv_data, pdev->device);
	if (res)
		return res;

	res = intel_dimm_get_channels_status(drv_data, dev);
	if (res)
		return res;

	hwmon_dev = devm_hwmon_device_register_with_info(dev, "intel_dimm",
														drv_data, &intel_dimm_hwmon_chip_info, NULL);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static const struct pci_device_id intel_dimm_pcu_ids[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SANDY_BRIDGE_PCU_F0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_IVY_BRIDGE_PCU_F0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_HASWELL_PCU_F0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_HASWELL_PCU_F4)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BROADWELL_PCU_F0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BROADWELL_PCU_F4)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SKYLAKE_PCU_F0)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ICE_LAKE_PCU_F2)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SAPPHIRE_RAPIDS_F2)},
	{},
};

MODULE_DEVICE_TABLE(pci, intel_dimm_pcu_ids);

static struct pci_driver intel_dimm_driver = {
	.name = "intel-dimm",
	.id_table = intel_dimm_pcu_ids,
	.probe = intel_dimm_probe,
};

module_pci_driver(intel_dimm_driver);

MODULE_DESCRIPTION("Intel DIMM thermal sensor driver");
MODULE_AUTHOR("Yannick Hemery <yannick.hemery@ovhcloud.com>");
