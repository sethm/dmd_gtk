#ifndef __TELNET_H__
#define __TELNET_H__

int telnet_open();
int telnet_close();
ssize_t telnet_read(uint8_t *buf, size_t len);
ssize_t telnet_send(uint8_t *buf, size_t len);

#endif
