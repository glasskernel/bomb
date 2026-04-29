/*
 * Copyright (c) 2024 Igor Belwon <igor.belwon@mentallysanemainliners.org>
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 *
 */

#include <platform/gpio.h>
#include "include/lk3rd/keys.h"

void setup_key(struct exynos_gpio_bank *bank, int gpio)
{
	exynos_gpio_set_pull(bank, gpio, GPIO_PULL_UP);
	exynos_gpio_cfg_pin(bank, gpio, GPIO_INPUT);
}
