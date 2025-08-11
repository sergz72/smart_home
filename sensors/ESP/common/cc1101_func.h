//
// Created by sergi on 14.12.2022.
//

#ifndef MAIN_CC1101_FUNC_H
#define MAIN_CC1101_FUNC_H

unsigned int cc1101Init(void);
unsigned char * cc1101Receive(void);
unsigned int cc1101ReceiveStart(void);
unsigned int cc1101ReceiveStop(void);

#endif //MAIN_CC1101_FUNC_H
