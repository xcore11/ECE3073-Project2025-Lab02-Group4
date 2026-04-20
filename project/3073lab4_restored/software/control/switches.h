/*
 * switches.h
 *
 *  Created on: Apr 19, 2026
 *      Author: User
 */

#ifndef SWITCHES_H_
#define SWITCHES_H_

void HEX_enable (int state);
void handle_switch2 (int state, const char * message);
void handle_switch3 (int state);
void handle_switch4 (int state);
int translator (char a);

#define FPGA_SPI_MAX_MSG_LEN 32
#define PB1_MASK 0x02
#define SPI_TX_MESSAGE "ECE3073"
#define SPI_TX_LENGTH 7

void fpga_spi_init(void);
int fpga_spi_transfer_message(const char *tx_buf, char *rx_buf, int len);
int fpga_spi_transfer_string(const char *tx_msg, char *rx_msg);

#endif /* SWITCHES_H_ */
