// SPDX-License-Identifier: GPL-2.0-only
/*
 * RTL8733BU WiFi/BT power control driver
 *
 * Controls the power enable GPIO of the RTL8733BU combo chip via rfkill.
 * Registers both RFKILL_TYPE_WLAN and RFKILL_TYPE_BLUETOOTH so the stack
 * (wifictl, bluetooth settings) can control WiFi and BT independently.  Power
 * is kept on if either WiFi or BT is unblocked; power is cut only when both
 * are blocked (maximum battery when both are off).  Uses the same GPIO as
 * the original vcc_wifi regulator (enable-active-low).
 *
 * Module load ordering is handled via modprobe.d softdeps + MODULE_SOFTDEP:
 *   - rtl8733bu_power loads at boot (modules-load.d)
 *   - MODULE_SOFTDEP pulls in 8733bu after this driver probes
 *   - softdep in modprobe.d ensures 8733bu loads before btusb
 *
 * The combo BT dependency (BT needs WiFi firmware alive) is solved by
 * disabling IPS in the 8733bu modprobe options (rtw_ips_mode=0) so the
 * combo block stays active even when WiFi is rfkill-blocked.  GPIO power
 * off (both blocked) still fully cuts the chip.
 *
 * After turning power on (and on resume), we wait 100 ms before returning
 * so the chip is stable before USB enumerates.
 *
 * Power for suspend is dropped by the driver rather than left to userspace;
 * see the comment above rtl8733bu_power_suspend_late() for why it has to be
 * the late phase.
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/rfkill.h>

#define RTL8733BU_POWER_STABLE_MS	100

struct rtl8733bu_power {
	struct gpio_desc *enable_gpio;
	struct rfkill *rfkill_wlan;
	struct rfkill *rfkill_bt;
	bool wlan_blocked;
	bool bt_blocked;
};

static void rtl8733bu_power_update_gpio(struct rtl8733bu_power *power)
{
	bool on = !power->wlan_blocked || !power->bt_blocked;

	gpiod_set_value_cansleep(power->enable_gpio, on ? 1 : 0);

	if (on)
		msleep(RTL8733BU_POWER_STABLE_MS);
}

static int rtl8733bu_power_set_block_wlan(void *data, bool blocked)
{
	struct rtl8733bu_power *power = data;

	power->wlan_blocked = blocked;
	rtl8733bu_power_update_gpio(power);
	return 0;
}

static int rtl8733bu_power_set_block_bt(void *data, bool blocked)
{
	struct rtl8733bu_power *power = data;

	power->bt_blocked = blocked;
	rtl8733bu_power_update_gpio(power);
	return 0;
}

static const struct rfkill_ops rtl8733bu_power_rfkill_ops_wlan = {
	.set_block = rtl8733bu_power_set_block_wlan,
};

static const struct rfkill_ops rtl8733bu_power_rfkill_ops_bt = {
	.set_block = rtl8733bu_power_set_block_bt,
};

static int rtl8733bu_power_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtl8733bu_power *power;
	int ret;

	power = devm_kzalloc(dev, sizeof(*power), GFP_KERNEL);
	if (!power)
		return -ENOMEM;

	power->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(power->enable_gpio)) {
		ret = PTR_ERR(power->enable_gpio);
		dev_err(dev, "failed to get enable GPIO: %d\n", ret);
		return ret;
	}

	power->wlan_blocked = false;
	power->bt_blocked = false;
	rtl8733bu_power_update_gpio(power);

	power->rfkill_wlan = rfkill_alloc("rtl8733bu wifi", dev, RFKILL_TYPE_WLAN,
					  &rtl8733bu_power_rfkill_ops_wlan, power);
	if (!power->rfkill_wlan)
		return -ENOMEM;
	rfkill_set_states(power->rfkill_wlan, false, false);
	ret = rfkill_register(power->rfkill_wlan);
	if (ret) {
		dev_err(dev, "failed to register rfkill wlan: %d\n", ret);
		rfkill_destroy(power->rfkill_wlan);
		return ret;
	}

	power->rfkill_bt = rfkill_alloc("rtl8733bu bluetooth", dev, RFKILL_TYPE_BLUETOOTH,
					&rtl8733bu_power_rfkill_ops_bt, power);
	if (!power->rfkill_bt) {
		ret = -ENOMEM;
		goto err_unregister_wlan;
	}
	rfkill_set_states(power->rfkill_bt, false, false);
	ret = rfkill_register(power->rfkill_bt);
	if (ret) {
		dev_err(dev, "failed to register rfkill bt: %d\n", ret);
		rfkill_destroy(power->rfkill_bt);
		goto err_unregister_wlan;
	}

	platform_set_drvdata(pdev, power);
	dev_info(dev, "rtl8733bu-power: WiFi/BT rfkill — GPIO power control\n");
	return 0;

err_unregister_wlan:
	rfkill_unregister(power->rfkill_wlan);
	rfkill_destroy(power->rfkill_wlan);
	return ret;
}

static void rtl8733bu_power_remove(struct platform_device *pdev)
{
	struct rtl8733bu_power *power = platform_get_drvdata(pdev);

	if (!power)
		return;
	if (power->rfkill_bt) {
		rfkill_unregister(power->rfkill_bt);
		rfkill_destroy(power->rfkill_bt);
	}
	if (power->rfkill_wlan) {
		rfkill_unregister(power->rfkill_wlan);
		rfkill_destroy(power->rfkill_wlan);
	}
}

static void rtl8733bu_power_shutdown(struct platform_device *pdev)
{
	struct rtl8733bu_power *power = platform_get_drvdata(pdev);

	if (!power)
		return;
	gpiod_set_value_cansleep(power->enable_gpio, 0);
}

/*
 * Cut chip power for suspend, so the userspace sleep hook is not the only
 * thing doing it.
 *
 * This has to be the late phase.  Two earlier points look plausible and are
 * both wrong:
 *
 *   PM_SUSPEND_PREPARE, via a PM notifier, matches when the userspace hook
 *   runs but not what it does.  The hook powers the chip down *through*
 *   rfkill, so the BT stack closes the HCI device while the chip is still
 *   alive.  Dropping the GPIO directly at PREPARE instead kills the chip
 *   underneath that close: hci_dev_close_sync() then sits in drain_workqueue()
 *   waiting on work the dead device will never complete, the rfkill task never
 *   freezes, and the suspend aborts with "Freezing user space processes failed
 *   ... tasks refusing to freeze".  It is a race, so it survives some cycles.
 *
 *   dev_pm_ops.suspend fires inside dpm_suspend() and this platform device has
 *   no ordering relationship to the USB controller, so it can yank a USB
 *   device mid-suspend.
 *
 * By .suspend_late, user space is frozen (nothing can issue a new rfkill or
 * HCI request) and every .suspend callback has run, including those of the USB
 * devices on this chip.  Nothing is left waiting on it, so cutting power is
 * safe.  No settle delay is needed either: the disconnect is not processed
 * until resume, when the chip re-enumerates anyway.
 *
 * The cut is unconditional, unlike update_gpio(), which keeps power on while
 * either rfkill is unblocked.  That is fine because .resume restores the GPIO
 * from the rfkill state, which this path does not modify.
 */
static int rtl8733bu_power_suspend_late(struct device *dev)
{
	struct rtl8733bu_power *power = dev_get_drvdata(dev);

	if (!power)
		return 0;

	gpiod_set_value_cansleep(power->enable_gpio, 0);
	return 0;
}

static int rtl8733bu_power_resume(struct device *dev)
{
	struct rtl8733bu_power *power = dev_get_drvdata(dev);

	if (!power)
		return 0;

	rtl8733bu_power_update_gpio(power);
	return 0;
}

static const struct dev_pm_ops rtl8733bu_power_pm_ops = {
	.suspend_late = rtl8733bu_power_suspend_late,
	.resume = rtl8733bu_power_resume,
};

static const struct of_device_id rtl8733bu_power_of_match[] = {
	{ .compatible = "rockchip,rtl8733bu-power" },
	{ }
};
MODULE_DEVICE_TABLE(of, rtl8733bu_power_of_match);

static struct platform_driver rtl8733bu_power_driver = {
	.probe    = rtl8733bu_power_probe,
	.remove   = rtl8733bu_power_remove,
	.shutdown = rtl8733bu_power_shutdown,
	.driver   = {
		.name = "rtl8733bu-power",
		.of_match_table = rtl8733bu_power_of_match,
		.pm = pm_sleep_ptr(&rtl8733bu_power_pm_ops),
	},
};
module_platform_driver(rtl8733bu_power_driver);

MODULE_SOFTDEP("post: 8733bu");
MODULE_DESCRIPTION("RTL8733BU WiFi/BT GPIO power via rfkill; power off when both blocked");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ROCKNIX");
