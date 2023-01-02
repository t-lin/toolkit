#include "stm32f4xx.h"
#include <stdbool.h>

void turnLEDsOn() {
  GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_7 | GPIO_Pin_14);
  return;
}

void turnLEDsOff() {
  GPIO_ResetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_7 | GPIO_Pin_14);
  return;
}

void pause() {
  for (uint64_t i = 0; i < SystemCoreClock / 30; i++) {
    __NOP();
  }
}

bool isButtonPressed() {
  if (GPIO_ReadInputData(GPIOC) & GPIO_Pin_13) {
    return true;
  }

  return false;
}

int main(void)
{
  // Enable different GPIO groups
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

  GPIO_InitTypeDef gpio;
  GPIO_StructInit(&gpio);

  // Configure LED
  gpio.GPIO_Pin  = GPIO_Pin_0 | GPIO_Pin_7 | GPIO_Pin_14;
  gpio.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_Init(GPIOB, &gpio);

  // Configure push button
  gpio.GPIO_Pin  = GPIO_Pin_13;
  gpio.GPIO_Mode = GPIO_Mode_IN;
  GPIO_Init(GPIOC, &gpio);

  while(1)
  {
    if (isButtonPressed() == true) {
      turnLEDsOn();
      //pause();
      continue;
    }

    turnLEDsOff();
    //pause();
  }

  return 0;
}
