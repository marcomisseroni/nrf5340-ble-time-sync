/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <SEGGER_RTT.h>
#include "conn_time_sync.h"

int main(void)
{
	char role;

	printk("Starting connection time sync sample.\n");
	printk("Choose device role - type c (central) or p (peripheral): ");

	do {

		role = SEGGER_RTT_GetKey();

		switch (role) {
		case 'p':
			printk("\nPeripheral. Starting advertising\n");
			peripheral_start();
			break;
		case 'c':
			printk("\nCentral. Starting scanning\n");
			central_start();
			break;
		default:
			k_msleep(100);
			break;
		}
	} while (role != 'c' && role != 'p');
}
