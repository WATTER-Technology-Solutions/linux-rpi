// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Based on:
 *    drivers/rtc/rtc-pcf8583.c
 *
 *  Copyright (C) 2000 Russell King
 *  Copyright (C) 2008 Wolfram Sang & Juergen Beisert, Pengutronix
 *  Copyright (C) 2025 Ben Collins
 *
 *  Driver for PCF8593 Chip in counter mode
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/bcd.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

struct pcf8593 {
	struct i2c_client *client;
	unsigned int scale, scale_micro;
};

#define CTRL_STOP	0xa0
#define CTRL_START	0x20

#define REG_CONTROL	0x00
#define REG_COUNTER	0x01

/* Max counter is 6 digits of BCD */
#define MAX_COUNTER	999999

static const struct iio_chan_spec pcf8593_trigger_channel = {
	.type = IIO_COUNT,
	.channel = 0,
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
			      BIT(IIO_CHAN_INFO_PEAK) |
			      BIT(IIO_CHAN_INFO_SCALE),
	.indexed = 1
};

static int pcf8593_get_counter(struct i2c_client *client);
static int pcf8593_reset_counter(struct i2c_client *client);

static int pcf8593_counter_read_raw(struct iio_dev *indio_dev,
				    struct iio_chan_spec const *chan,
				    int *val, int *val2, long mask)
{
        struct pcf8593 *pcf8593 = iio_priv(indio_dev);
        int last_count;
	long delta;
	unsigned long msec, last_read, count;

        switch (mask) {
        case IIO_CHAN_INFO_RAW:
		/* Provide raw count since last reset */
		count = pcf8593_get_counter(pcf8593->client);
		if (count < 0)
			return count;

		*val = count;

		return IIO_VAL_INT;

	case IIO_CHAN_INFO_PEAK:
		*val = MAX_COUNTER;

		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = pcf8593->scale;
		*val2 = pcf8593->scale_micro;

		return IIO_VAL_INT_PLUS_MICRO;
        }

        return -EINVAL;
}

static int pcf8593_counter_write_raw(struct iio_dev *indio_dev,
				     struct iio_chan_spec const *chan,
				     int val, int val2,
				     long mask)
{
	struct pcf8593 *pcf8593 = iio_priv(indio_dev);

        switch (mask) {
        case IIO_CHAN_INFO_RAW:
		return pcf8593_reset_counter(pcf8593->client);
	}

	return -EINVAL;
}

static const struct iio_info pcf8593_trigger_info = {
	.read_raw = pcf8593_counter_read_raw,
	.write_raw = pcf8593_counter_write_raw,
};

static struct pcf8593 *pcf8593_setup_counter_device(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct pcf8593));
	if (!indio_dev)
		return ERR_PTR(-ENOMEM);

	indio_dev->name = id->name;
	indio_dev->info = &pcf8593_trigger_info;
	indio_dev->modes = INDIO_HARDWARE_TRIGGERED;
	indio_dev->num_channels = 1;
	indio_dev->channels = &pcf8593_trigger_channel;

	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret)
		return ERR_PTR(ret);

	return iio_priv(indio_dev);
}

static int pcf8593_get_counter(struct i2c_client *client)
{
	unsigned char buf[3], addr = REG_COUNTER;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &addr,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 3,
			.buf = buf,
		}
	};
	int ret, count = 0;

	memset(buf, 0, sizeof(buf));

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret != 2)
		return -EIO;

	count  = bcd2bin(buf[0]);
	count += bcd2bin(buf[1]) * 100;
	count += bcd2bin(buf[2]) * 10000;

	return count;
}

static int pcf8593_reset_counter(struct i2c_client *client)
{
	struct pcf8593 *pcf8593 = i2c_get_clientdata(client);
	unsigned char buf[5];
	int ret;

	/* Stop counter, then zero the counter values */
	buf[0] = REG_CONTROL;
	buf[1] = CTRL_STOP;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;

	ret = i2c_master_send(client, (char *)buf, sizeof(buf));
	if (ret != sizeof(buf))
		return -EIO;

	buf[1] = CTRL_START;
	ret = i2c_master_send(client, (char *)buf, 2);

	return ret == 2 ? 0 : -EIO;
}

static void pcf8593_probe_scale(struct pcf8593 *pcf8593)
{
	struct device_node *np = dev_of_node(&pcf8593->client->dev);

	of_property_read_u32_index(np, "scale", 0,
				   &pcf8593->scale);
	of_property_read_u32_index(np, "scale", 1,
				   &pcf8593->scale_micro);
}

static int pcf8593_probe(struct i2c_client *client)
{
	struct pcf8593 *pcf8593;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	pcf8593 = pcf8593_setup_counter_device(client);
	if (IS_ERR(pcf8593))
		return PTR_ERR(pcf8593);

	pcf8593->client = client;

	pcf8593_probe_scale(pcf8593);

	i2c_set_clientdata(client, pcf8593);

	ret = pcf8593_reset_counter(client);
	if (ret)
		return ret;

	dev_info(&client->dev, "registered pulse counter device\n");

	return 0;
}

static const struct i2c_device_id pcf8593_id[] = {
	{ "pcf8593-ctr" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcf8593_id);

static struct i2c_driver pcf8593_driver = {
	.driver = {
		.name	= "pcf8593-ctr",
	},
	.probe		= pcf8593_probe,
	.id_table	= pcf8593_id,
};

module_i2c_driver(pcf8593_driver);

MODULE_AUTHOR("Ben Collins");
MODULE_DESCRIPTION("PCF8593 Chip in counter mode");
MODULE_LICENSE("GPL");
