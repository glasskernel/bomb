/*
 * Copyright (c) 2024 Igor Belwon <igor.belwon@mentallysanemainliners.org>
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef KEYS_H
#define KEYS_H

#include <target/board_info.h>

#ifndef VOLDOWN_GPIOCON
#define VOLDOWN_GPIOCON 0
#endif

#ifndef VOLDOWN_BIT
#define VOLDOWN_BIT 4
#endif

#ifndef VOLUP_GPIOCON
#define VOLUP_GPIOCON VOLDOWN_GPIOCON
#endif

#ifndef VOLUP_BIT
#define VOLUP_BIT (VOLDOWN_BIT - 1)
#endif

#ifndef POWER_GPIOCON
#ifdef EXYNOS9830_GPA2CON
#define POWER_GPIOCON EXYNOS9830_GPA2CON
#else
#define POWER_GPIOCON VOLDOWN_GPIOCON
#endif
#endif

#ifndef POWER_BIT
#define POWER_BIT VOLDOWN_BIT
#endif

void setup_key(struct exynos_gpio_bank *bank, int gpio);

#endif /* KEYS_H */
