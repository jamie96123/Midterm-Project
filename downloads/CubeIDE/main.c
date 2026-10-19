/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* [TR-INCLUDES] Standard C only; peripheral access comes from main.h/CMSIS. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typerush_irq.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* [TR-TYPES] Sizes must precede the Game struct declaration. */
enum { GAME_ROUNDS = 10u, WORD_CAP = 24u, WORD_COUNT = 40u };
typedef enum { READY, RUNNING, PAUSED, FINISHED } GameState;
typedef enum { GREEN, YELLOW, RED, BLUE } LightColor;
typedef struct {
    GameState state;
    uint32_t score, correct, wrong, misses, attempts, hits;
    uint32_t question, remaining, duration, elapsed, last_tick, random;
    uint32_t last_award;
    uint8_t rate, pos, manual;
    LightColor display_color;
    char word[WORD_CAP];
    const char *result;
} Game;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* [TR-CONSTANTS] Interrupt-safe serial buffer capacities. */
#define RX_CAP 512u
#define TX_CAP 2048u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* [TR-STATE] No game code is needed in any other .c file. */
static Game game;
static uint32_t protocol_errors;
static char command_line[64];
static size_t command_length;
static bool command_discard;
static uint32_t last_report;
static uint32_t last_lights = UINT32_MAX;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
/* [TR-PROTOTYPES] Main-only helpers, except the four public IRQ bridges. */
static void game_init(Game *g, uint32_t now);
static void game_start(Game *g, uint32_t now);
static void game_update(Game *g, uint32_t now);
static void game_pause(Game *g, uint32_t now);
static void game_resume(Game *g, uint32_t now);
static void game_key(Game *g, uint32_t key, uint32_t now);
static void game_speed(Game *g, uint32_t now);
static void game_color(Game *g);
static LightColor game_tier(const Game *g);
static const char *game_state_name(GameState state);
static const char *game_color_name(LightColor color);
static void board_init(void);
static uint32_t board_millis(void);
static uint32_t board_buttons(void);
static int board_getchar(void);
static bool board_send(const char *data, size_t length);
static void board_lights(const Game *g);
static uint32_t board_rx_errors(void);
static uint32_t board_tx_drops(void);
static const char *game_command(Game *g, const char *line, uint32_t now);
static void TypeRush_Init(void);
static void TypeRush_Process(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* [TR-GAME] Word selection, countdown, scoring and command parsing. */
static const char *const game_words[WORD_COUNT] = {
    "rhythm", "light", "timer", "signal", "board", "focus", "pulse", "typing",
    "clock", "green", "yellow", "speed", "serial", "buffer", "button", "score",
    "bright", "flow", "micro", "pixel", "logic", "react", "power", "flash",
    "input", "output", "energy", "swift", "challenge", "breathe", "spark", "wave",
    "orbit", "shift", "dream", "coding", "tempo", "motion", "switch", "level"
};
static const uint32_t durations[] = {8000, 5000, 3000};
static void next_word(Game *g) {
    g->question++;
    if (g->question > GAME_ROUNDS) {
        g->question = GAME_ROUNDS;
        g->state = FINISHED;
        g->remaining = 0;
        return;
    }
    g->random = g->random * 1664525u + 1013904223u;
    strcpy(g->word, game_words[(g->random >> 16) % WORD_COUNT]);
    g->duration = durations[g->rate];
    g->remaining = g->duration;
    g->pos = 0;
    g->manual = 0;
}
static void game_init(Game *g, uint32_t now) {
    memset(g, 0, sizeof(*g));
    g->state = READY;
    g->duration = durations[0];
    g->remaining = g->duration;
    g->last_tick = now;
    g->random = 0x474;
    g->display_color = BLUE;
    g->result = "ready";
}
static void game_start(Game *g, uint32_t now) {
    uint8_t rate = g->rate;
    uint32_t seed = g->random ^ now;
    game_init(g, now);
    g->rate = rate;
    g->random = seed;
    g->state = RUNNING;
    g->result = "started";
    next_word(g);
    g->display_color = game_tier(g);
}
static LightColor game_tier(const Game *g) {
    if (g->remaining * 100u > g->duration * 60u) return GREEN;
    if (g->remaining * 100u >= g->duration * 30u) return YELLOW;
    return RED;
}
static void game_update(Game *g, uint32_t now) {
    uint32_t delta = now - g->last_tick; /* unsigned subtraction handles tick wrap */
    g->last_tick = now;
    while (g->state == RUNNING && delta) {
        uint32_t spent = delta < g->remaining ? delta : g->remaining;
        g->remaining -= spent;
        g->elapsed += spent;
        delta -= spent;
        if (!g->remaining) {
            g->misses++;
            g->last_award = 0;
            g->result = "timeout";
            next_word(g);
        }
    }
    if (!g->manual && (g->state == RUNNING || g->state == PAUSED))
        g->display_color = game_tier(g);
}
static void game_pause(Game *g, uint32_t now) {
    game_update(g, now);
    if (g->state == RUNNING) { g->state = PAUSED; g->result = "paused"; }
}
static void game_resume(Game *g, uint32_t now) {
    game_update(g, now);
    if (g->state == PAUSED) { g->state = RUNNING; g->result = "resumed"; }
}
static void game_key(Game *g, uint32_t key, uint32_t now) {
    game_update(g, now);
    if (g->state != RUNNING) return;
    /* Backspace deliberately does not erase accepted progress or accuracy history. */
    if (key < 32 || key > 126) return;
    g->attempts++;
    if (key == (uint8_t)g->word[g->pos]) {
        g->hits++;
        g->pos++;
        g->result = "hit";
        if (!g->word[g->pos]) {
            LightColor tier = game_tier(g);
            g->last_award = tier == GREEN ? 7u : tier == YELLOW ? 5u : 3u;
            g->score += g->last_award;
            g->correct++;
            g->result = "correct";
            next_word(g);
        }
    } else { g->wrong++; g->result = "wrong"; }
    game_update(g, now);
}
static void game_speed(Game *g, uint32_t now) {
    game_update(g, now);
    g->rate = (uint8_t)((g->rate + 1u) % 3u);
    /* A changed difficulty starts with the same remaining fraction, never a free refill. */
    uint32_t duration = durations[g->rate];
    g->remaining = (g->remaining * duration + g->duration - 1u) / g->duration;
    g->duration = duration;
    g->result = "speed";
    game_update(g, now);
}
static void game_color(Game *g) {
    g->display_color = g->display_color == BLUE ? GREEN : (LightColor)((g->display_color + 1u) % 3u);
    g->manual = 1;
    g->result = "color"; /* presentation override; score always uses the real time tier */
}
static const char *game_state_name(GameState state) {
    static const char *const names[] = {"ready", "running", "paused", "finished"};
    return names[state];
}
static const char *game_color_name(LightColor color) {
    static const char *const names[] = {"green", "yellow", "red", "blue"};
    return names[color];
}

static const char *game_command(Game *game, const char *line, uint32_t now) {
    if (!strcmp(line, "SYNC")) game_update(game, now);
    else if (!strcmp(line, "START")) game_start(game, now);
    else if (!strcmp(line, "PAUSE")) game_pause(game, now);
    else if (!strcmp(line, "RESUME")) game_resume(game, now);
    else if (!strcmp(line, "RESET")) game_init(game, now);
    else if (!strcmp(line, "COLOR")) { game_update(game, now); game_color(game); }
    else if (!strcmp(line, "SPEED")) game_speed(game, now);
    else if (!strncmp(line, "KEY ", 4)) {
        char *end;
        unsigned long key = strtoul(line + 4, &end, 10);
        if (line[4] < '0' || line[4] > '9' || *end || key < 32 || key > 126) return "INVALID_KEY";
        game_key(game, (uint32_t)key, now);
    } else return "UNKNOWN_COMMAND";
    return NULL;
}

/* [TR-HARDWARE] Register-level GPIO / EXTI / UART / TIM / PWM.
 * Clock setup is kept in the user's original HAL SystemClock_Config().
 * Do not also call HAL UART receive, HAL IRQ dispatch, or HAL PWM start on these peripherals.
 */
/* RX and TX use separate single-producer/single-consumer rings. */
static volatile uint8_t rx[RX_CAP], tx[TX_CAP];
static volatile uint16_t rx_head, rx_tail, tx_head, tx_tail;
static volatile uint32_t ticks, buttons, rx_errors, tx_drops;
static volatile bool rx_discard;
static uint32_t button_times[4];
static const uint32_t breath_periods[] = {1800, 1200, 700};
static void mode(GPIO_TypeDef *port, unsigned pin, unsigned value) {
    port->MODER = (port->MODER & ~(3u << (pin * 2u))) | (value << (pin * 2u));
}
static void alternate(GPIO_TypeDef *port, unsigned pin, unsigned af) {
    unsigned reg = pin / 8u, shift = (pin % 8u) * 4u;
    port->AFR[reg] = (port->AFR[reg] & ~(15u << shift)) | (af << shift);
    mode(port, pin, 2);
}
static void gpio_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->AHB2ENR;
    mode(GPIOA, 5, 1); /* Nucleo LD2: direct output */
    GPIOA->BSRR = 1u << (5u + 16u);
    for (unsigned pin = 4; pin <= 7; pin++) {
        mode(GPIOB, pin, 0);
        GPIOB->PUPDR = (GPIOB->PUPDR & ~(3u << (pin * 2u))) | (1u << (pin * 2u));
        unsigned reg = pin / 4u, shift = (pin % 4u) * 4u;
        SYSCFG->EXTICR[reg] = (SYSCFG->EXTICR[reg] & ~(15u << shift)) | (1u << shift);
    }
    EXTI->RTSR1 &= ~(15u << 4);
    EXTI->FTSR1 |= 15u << 4; /* external normally-open switches to GND */
    EXTI->PR1 = 15u << 4;
    EXTI->IMR1 |= 15u << 4;
    NVIC_SetPriority(EXTI4_IRQn, 2);
    NVIC_SetPriority(EXTI9_5_IRQn, 2);
    NVIC_EnableIRQ(EXTI4_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}
static void uart_init(void) {
    alternate(GPIOA, 2, 7);
    alternate(GPIOA, 3, 7);
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
    (void)RCC->APB1ENR1;
    /* PCLK1 source must match BRR=1476, including after CubeMX regeneration. */
    RCC->CCIPR &= ~RCC_CCIPR_USART2SEL;
    RCC->APB1RSTR1 |= RCC_APB1RSTR1_USART2RST;
    RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_USART2RST;
    NVIC_ClearPendingIRQ(USART2_IRQn);
    USART2->CR1 = 0;
    USART2->BRR = 1476; /* 170 MHz / 115200, OVER8=0, 8 data, no parity, 1 stop */
    USART2->CR2 = 0;
    USART2->CR3 = USART_CR3_EIE;
    USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    USART2->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
    NVIC_SetPriority(USART2_IRQn, 1);
    NVIC_EnableIRQ(USART2_IRQn);
}
static void timers_init(void) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN | RCC_APB1ENR1_TIM3EN;
    (void)RCC->APB1ENR1;
    RCC->APB1RSTR1 |= RCC_APB1RSTR1_TIM2RST | RCC_APB1RSTR1_TIM3RST;
    RCC->APB1RSTR1 &= ~(RCC_APB1RSTR1_TIM2RST | RCC_APB1RSTR1_TIM3RST);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    TIM2->PSC = 169;
    TIM2->ARR = 9999; /* 170MHz/170/10000 = 100Hz = 10ms */
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
    alternate(GPIOA, 6, 2); /* TIM3_CH1, red */
    alternate(GPIOA, 7, 2); /* TIM3_CH2, green */
    alternate(GPIOB, 0, 2); /* TIM3_CH3, blue */
    TIM3->PSC = 169;
    TIM3->ARR = 999; /* 1kHz hardware PWM carrier */
    TIM3->CCMR1 = (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE
                | (6u << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
    TIM3->CCMR2 = (6u << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE;
    TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;
    TIM3->CCR1 = TIM3->CCR2 = TIM3->CCR3 = 0;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}
static void board_init(void) { gpio_init(); uart_init(); timers_init(); }
static uint32_t board_millis(void) { return ticks; }
void TypeRush_TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) { TIM2->SR = 0; ticks += 10u; }
}
static void button_irq(unsigned index) {
    unsigned pin = index + 4u;
    if (EXTI->PR1 & (1u << pin)) {
        EXTI->PR1 = 1u << pin; /* write one to clear */
        if (!(GPIOB->IDR & (1u << pin)) && (uint32_t)(ticks - button_times[index]) >= 50u) {
            button_times[index] = ticks;
            buttons |= 1u << index;
        }
    }
}
void TypeRush_EXTI4_IRQHandler(void) { button_irq(0); }
void TypeRush_EXTI9_5_IRQHandler(void) { button_irq(1); button_irq(2); button_irq(3); }
static uint32_t board_buttons(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t pending = buttons;
    buttons = 0;
    __set_PRIMASK(primask);
    return pending;
}
void TypeRush_USART2_IRQHandler(void) {
    uint32_t status = USART2->ISR;
    if (status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) {
        USART2->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
        rx_errors++;
        rx_discard = true;
    }
    if (status & USART_ISR_RXNE) {
        uint8_t c = (uint8_t)USART2->RDR;
        if (rx_discard) {
            if (c == '\n') {
                /* Drop queued fragments as well; no malformed command may execute. */
                rx_head = rx_tail;
                rx_discard = false;
                uint16_t next = (uint16_t)((rx_head + 1u) % RX_CAP);
                rx[rx_head] = '\n';
                __DMB(); rx_head = next;
            }
        } else {
            uint16_t next = (uint16_t)((rx_head + 1u) % RX_CAP);
            if (next == rx_tail) { rx_errors++; rx_discard = true; }
            else { rx[rx_head] = c; __DMB(); rx_head = next; }
        }
    }
    if ((status & USART_ISR_TXE) && (USART2->CR1 & USART_CR1_TXEIE)) {
        if (tx_tail != tx_head) {
            USART2->TDR = tx[tx_tail];
            tx_tail = (uint16_t)((tx_tail + 1u) % TX_CAP);
        } else USART2->CR1 &= ~USART_CR1_TXEIE;
    }
}
static int board_getchar(void) {
    /* A very short critical section also serializes error recovery with the ISR. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    static uint32_t notified_errors;
    int c = -1;
    if (notified_errors != rx_errors) {
        notified_errors = rx_errors;
        c = -2; /* main loop must discard its partial command through next newline */
    } else if (rx_tail != rx_head) {
        c = rx[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1u) % RX_CAP);
    }
    __set_PRIMASK(primask);
    return c;
}
static bool board_send(const char *data, size_t length) {
    uint16_t head = tx_head;
    size_t free_bytes = (tx_tail + TX_CAP - head - 1u) % TX_CAP;
    if (length > free_bytes) { tx_drops++; return false; }
    for (size_t i = 0; i < length; i++) {
        tx[head] = (uint8_t)data[i]; head = (uint16_t)((head + 1u) % TX_CAP);
    }
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    __DMB(); tx_head = head;
    USART2->CR1 |= USART_CR1_TXEIE;
    __set_PRIMASK(primask);
    return true;
}
static void board_lights(const Game *g) {
    uint32_t brightness = 600;
    if (g->state == RUNNING) {
        uint32_t period = breath_periods[g->rate];
        uint32_t phase = g->elapsed % period;
        uint32_t triangle = phase < period / 2u ? phase : period - phase;
        brightness = 80u + triangle * 900u / (period / 2u);
    } else if (g->state == READY) brightness = 160;
    if (g->state == PAUSED) {
        /* Actual MODER+BSRR GPIO output, not just a constant PWM duty. */
        mode(GPIOA, 6, 1); mode(GPIOA, 7, 1); mode(GPIOB, 0, 1);
        bool red = g->display_color == RED || g->display_color == YELLOW;
        bool green = g->display_color == GREEN || g->display_color == YELLOW;
        GPIOA->BSRR = (red ? 1u << 6 : 1u << 22) | (green ? 1u << 7 : 1u << 23);
        GPIOB->BSRR = g->display_color == BLUE ? 1u : 1u << 16;
    } else {
        TIM3->CCR1 = (g->display_color == RED || g->display_color == YELLOW) ? brightness : 0;
        TIM3->CCR2 = (g->display_color == GREEN || g->display_color == YELLOW) ? brightness : 0;
        TIM3->CCR3 = g->display_color == BLUE ? brightness : 0;
        alternate(GPIOA, 6, 2); alternate(GPIOA, 7, 2); alternate(GPIOB, 0, 2);
    }
    GPIOA->BSRR = g->state == RUNNING && ((ticks / 500u) & 1u) ? 1u << 5 : 1u << 21;
}
static uint32_t board_rx_errors(void) { return rx_errors; }
static uint32_t board_tx_drops(void) { return tx_drops; }

/* [TR-APP] Same TypeRush v1 JSON protocol as the existing website. */
static void report(void) {
    char frame[640];
    int n = snprintf(frame, sizeof(frame),
        "{\"t\":\"state\",\"v\":1,\"state\":\"%s\",\"word\":\"%s\",\"pos\":%lu,"
        "\"score\":%lu,\"correct\":%lu,\"wrong\":%lu,\"misses\":%lu,\"attempts\":%lu,\"hits\":%lu,"
        "\"question\":%lu,\"total\":10,\"remaining\":%lu,\"duration\":%lu,\"elapsed\":%lu,"
        "\"rate\":%lu,\"color\":\"%s\",\"manual\":%lu,\"lastAward\":%lu,\"result\":\"%s\","
        "\"rxErrors\":%lu,\"txDrops\":%lu,\"protocolErrors\":%lu}\n",
        game_state_name(game.state), game.word, (unsigned long)game.pos,
        (unsigned long)game.score, (unsigned long)game.correct, (unsigned long)game.wrong,
        (unsigned long)game.misses, (unsigned long)game.attempts, (unsigned long)game.hits,
        (unsigned long)game.question, (unsigned long)game.remaining, (unsigned long)game.duration,
        (unsigned long)game.elapsed, (unsigned long)game.rate, game_color_name(game.display_color),
        (unsigned long)game.manual, (unsigned long)game.last_award, game.result,
        (unsigned long)board_rx_errors(), (unsigned long)board_tx_drops(), (unsigned long)protocol_errors);
    if (n > 0 && (size_t)n < sizeof(frame)) board_send(frame, (size_t)n);
}
static void error(const char *code) {
    char frame[100];
    protocol_errors++;
    int n = snprintf(frame, sizeof(frame), "{\"t\":\"error\",\"code\":\"%s\"}\n", code);
    if (n > 0 && (size_t)n < sizeof(frame)) board_send(frame, (size_t)n);
}
static void command(const char *line, uint32_t now) {
    const char *code = game_command(&game, line, now);
    if (code) { error(code); return; }
    report();
}

static void TypeRush_Init(void) {
    /* The user's HSI16 /4 *85 /2 clock must remain 170 MHz on both APBs. */
    if (SystemCoreClock != 170000000u || HAL_RCC_GetPCLK1Freq() != 170000000u
        || HAL_RCC_GetPCLK2Freq() != 170000000u) Error_Handler();
    board_init();
    game_init(&game, board_millis());
    board_lights(&game);
    last_report = board_millis();
    report();
}
static void TypeRush_Process(void) {
    uint32_t now = board_millis();
    GameState previous = game.state;
    uint32_t old_question = game.question;
    game_update(&game, now);
    uint32_t pending = board_buttons();
    if (pending & 1u) {
        if (game.state == PAUSED) game_resume(&game, now);
        else if (game.state != RUNNING) game_start(&game, now);
    }
    if (pending & 2u) {
        if (game.state == PAUSED) game_resume(&game, now);
        else game_pause(&game, now);
    }
    if (pending & 4u) game_color(&game);
    if (pending & 8u) game_speed(&game, now);
    int c;
    /* Bound work per iteration so a busy sender cannot starve countdown/light updates. */
    for (unsigned budget = 0; budget < 128u; budget++) {
        c = board_getchar();
        if (c == -1) break;
        if (c == -2) { command_length = 0; command_discard = true; continue; }
        if (c == '\r') continue;
        if (c == '\n') {
            if (command_discard) { error("RX_FRAME_DROPPED"); }
            else if (command_length) { command_line[command_length] = 0; command(command_line, board_millis()); }
            command_length = 0; command_discard = false;
        } else if (!command_discard) {
            if (c < 32 || c > 126 || command_length >= sizeof(command_line) - 1u) {
                command_discard = true; command_length = 0;
            } else command_line[command_length++] = (char)c;
        }
    }
    if (now != last_lights) { board_lights(&game); last_lights = now; }
    if (pending || previous != game.state || old_question != game.question
        || (uint32_t)(now - last_report) >= 100u) {
        report(); last_report = now;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
/* [TR-START] Call after Cube-generated GPIO/peripheral initialization. */
  TypeRush_Init();
/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* [TR-LOOP] ISRs only enqueue events; game logic runs in thread context. */
    TypeRush_Process();
    __WFI();
  }
/* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* [TR-AF-FIX] PA2/PA3 use USART2 AF7, not LPUART1 AF12.
   * Mirror this change in .ioc, or regeneration will restore the old AF12 block.
   */
  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* Set a breakpoint here to inspect assertions when USE_FULL_ASSERT is enabled. */
/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */