/*
 * Aquantia PHY drivers
 */
#include <common.h>
#include <phy.h>

/* Aquantia AQR105 */


static int aqr105_parse_status(struct phy_device *phydev)
{
/*
	int mii_reg;

	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_DP83865_LANR);

	switch (mii_reg & MIIM_DP83865_SPD_MASK) {

	case MIIM_DP83865_SPD_1000:
		phydev->speed = SPEED_1000;
		break;

	case MIIM_DP83865_SPD_100:
		phydev->speed = SPEED_100;
		break;

	default:
		phydev->speed = SPEED_10;
		break;

	}

	if (mii_reg & MIIM_DP83865_DPX_FULL)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;
*/
	return 0;
}


static int aqr105_config(struct phy_device *phydev)
{
	printf("ENTER aqr105_config(struct phy_device *phydev=%p)...\n", phydev);

	return 0;
}

static int aqr105_startup(struct phy_device *phydev)
{
	printf("ENTER aqr105_starup(struct phy_device *phydev=%p)...\n", phydev);
	genphy_update_link(phydev);
	aqr105_parse_status(phydev);

	return 0;
}

static int aqr105_shutdown(struct phy_device *phydev)
{
	printf("ENTER aqr105_shutdown(struct phy_device *phydev=%p)...\n", phydev);

	return 0;
}

static struct phy_driver AQR105_driver = {
	.name = "Aquantia AQR105",
	.uid = 0xffffffff,
	.mask = 0xffffffff,
	.features = PHY_10G_FEATURES,
	.config = &aqr105_config,
	.startup = &aqr105_startup,
	.shutdown = &aqr105_shutdown,
};

int aquantia_phy_init(void)
{
	printf("ENTER %s() ...\n", __func__);
	phy_register(&AQR105_driver);

	return 0;
}
