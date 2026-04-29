#ifndef _PLATFORM_USB_H_
#define _PLATFORM_USB_H_

const char *fastboot_get_serialno_string(void);
void platform_prepare_reboot(void);
void platform_do_reboot(const char *cmd_buf);

#endif /* _PLATFORM_USB_H_ */
