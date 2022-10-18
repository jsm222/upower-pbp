/*-
 * Copyright (c) 2020, 2021 Soren Schmidt <sos@DeepCore.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Driver for CellWise CW2015 battery fuel gauge
 */

#include <sys/cdefs.h>
__FBSDID("$Id$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "iicbus_if.h"

static struct ofw_compat_data compat_data[] = {
	{ "cellwise,cw2015",			1 },
	{ NULL,					0 }
};

#define	CW_VERSION	0x00
#define CW_CELVOLT	0x02
#define CW_CHARGE	0x04
#define CW_AL_RRT	0x06
#define CW_CONFIG	0x08
#define CW_MODE		0x0a
#define CW_STATUS	0x0c

static int
cw2015_read(device_t dev, uint8_t reg, void *data, uint8_t size)
{
	return iicdev_readfrom(dev, reg, data, size, IIC_INTRWAIT);
}

static int
cw2015_write(device_t dev, uint8_t reg, uint8_t val)
{
	return iicdev_writeto(dev, reg, &val, 1, IIC_INTRWAIT);
}

static int
cw2015_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return ENXIO;

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return ENXIO;

	device_set_desc(dev, "CellWise CW2015 fuel-gauge");

	return BUS_PROBE_DEFAULT;
}

static int
cw2015_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	int type = (uint8_t)arg2;
	int16_t word;
	int error, temp;

	switch (type) {
	case CW_CELVOLT:
		cw2015_read(dev, CW_CELVOLT, &word, sizeof(word));
		temp = be16toh(word) * 312 / 1024;
		error = sysctl_handle_int(oidp, &temp, 0, req);
        	if (error || req->newptr == NULL)
                	return error;
		break;
	case CW_CHARGE:
		cw2015_read(dev, CW_CHARGE, &word, sizeof(word));
		temp = be16toh(word) >> 8;
		error = sysctl_handle_int(oidp, &temp, 0, req);
        	if (error || req->newptr == NULL)
                	return error;
		break;
	case CW_AL_RRT:
		cw2015_read(dev, CW_AL_RRT, &word, sizeof(word));
		temp = be16toh(word) & 0x1fff;
		if (temp == 0x1fff)
			temp = 0;
		error = sysctl_handle_int(oidp, &temp, 0, req);
        	if (error || req->newptr == NULL)
                	return error;
		break;
	case CW_STATUS:
		cw2015_read(dev, CW_AL_RRT, &word, sizeof(word));
		temp = (be16toh(word) & 0x7fff) == 0x1fff ? 1 : 0;
		error = sysctl_handle_int(oidp, &temp, 0, req);
        	if (error || req->newptr == NULL)
                	return error;
		break;
	default:
		return -1;
	}
	return 0;
}

static int
cw2015_attach(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;
	int8_t version = 0, mode = 0;
	int16_t vcell, soc, rrt;
	int millivolt, remaining;

	// if mode == sleep wakeup chip ??
	cw2015_read(dev, CW_MODE, &mode, sizeof(mode));
	if (mode) {
		cw2015_write(dev, CW_MODE, 0xc0);
		DELAY(10000);
		cw2015_write(dev, CW_MODE, 0x00);
	}
	cw2015_read(dev, CW_MODE, &mode, sizeof(mode));
	cw2015_read(dev, CW_VERSION, &version, sizeof(version));
	cw2015_read(dev, CW_CELVOLT, &vcell, sizeof(vcell));
	cw2015_read(dev, CW_CHARGE, &soc, sizeof(soc));
	cw2015_read(dev, CW_AL_RRT, &rrt, sizeof(rrt));

	// should we average over a set of samples ??
	millivolt = be16toh(vcell) * 312 / 1024;
	remaining = be16toh(rrt) & 0x7fff;
	device_printf(dev, "cell %dmV charge %d%% %s\n",
		      millivolt, (soc & 0xff),
		      remaining == 0x1fff ? "charging" : "discharging");

	// register sysctls etc...
	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "millivolt",
            CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, CW_CELVOLT,
            cw2015_sysctl, "IU", "battery voltage in millivolt");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "chargepct",
            CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, CW_CHARGE,
            cw2015_sysctl, "IU", "remaining charge in percent");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "remaining",
            CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, CW_AL_RRT,
            cw2015_sysctl, "IU", "remaining run time in minutes");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "charging",
            CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, CW_STATUS,
            cw2015_sysctl, "IU", "charging status");

	return 0;
}

static int
cw2015_detach(device_t dev)
{
	return 0;
}


static device_method_t cw2015_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cw2015_probe),
	DEVMETHOD(device_attach,	cw2015_attach),
	DEVMETHOD(device_detach,	cw2015_detach),

	DEVMETHOD_END
};

static driver_t cw2015_driver = {
	"cwfg", cw2015_methods, 0,
};


EARLY_DRIVER_MODULE(cw2015, iicbus, cw2015_driver, 0, 0,
    BUS_PASS_RESOURCE);
MODULE_VERSION(cw2015, 1);
MODULE_DEPEND(cw2015, iicbus, 1, 1, 1);
IICBUS_FDT_PNP_INFO(compat_data);
