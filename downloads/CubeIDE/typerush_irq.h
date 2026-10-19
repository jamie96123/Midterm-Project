#ifndef TYPERUSH_IRQ_H
#define TYPERUSH_IRQ_H
#ifdef __cplusplus
extern "C" {
#endif
/* Call exactly once from the matching handler in stm32g4xx_it.c. */
void TypeRush_TIM2_IRQHandler(void);
void TypeRush_USART2_IRQHandler(void);
void TypeRush_EXTI4_IRQHandler(void);
void TypeRush_EXTI9_5_IRQHandler(void);
#ifdef __cplusplus
}
#endif
#endif
