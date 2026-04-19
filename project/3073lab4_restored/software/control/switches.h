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

#endif /* SWITCHES_H_ */
