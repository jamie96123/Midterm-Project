#include "main.h"
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

UART_HandleTypeDef hlpuart1;

/* USER CODE BEGIN PD */
#define RX_RING_SIZE 256U
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static uint8_t rxByte;
static char lineBuffer[32];
static uint8_t lineLength = 0U;
static uint8_t lineTooLong = 0U;

static volatile uint8_t rxRing[RX_RING_SIZE];
static volatile uint16_t rxHead = 0U;
static volatile uint16_t rxTail = 0U;
static volatile uint32_t rxDropped = 0U;
static volatile uint32_t commandCount = 0U;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_LPUART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void RingPush(uint8_t data);
static uint8_t RingPop(uint8_t *data);
static void ProcessReceivedByte(uint8_t data);
static void ExecuteCommand(const char *command);
/* USER CODE END PFP */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_LPUART1_UART_Init();

    /* USER CODE BEGIN 2 */
    /* PPT BASIC 01: send once after startup. */
    uint8_t startupMessage[] = "HELLO WORLD\r\n";
    if (HAL_UART_Transmit(&hlpuart1, startupMessage,
                         sizeof(startupMessage) - 1U, 100U) != HAL_OK)
        Error_Handler();

    /* PPT BASIC 03: start single-byte interrupt reception. */
    if (HAL_UART_Receive_IT(&hlpuart1, &rxByte, 1U) != HAL_OK)
        Error_Handler();

    /* USER CODE END 2 */
    while (1)
    {
        /* USER CODE BEGIN 3 */
        HAL_Delay(200U);  /* Interrupts keep receiving during this delay. */

        uint8_t data;
        while (RingPop(&data) != 0U)
        {
            ProcessReceivedByte(data);
        }
        /* USER CODE END 3 */
    }
}

/* USER CODE BEGIN 4 */
static void RingPush(uint8_t data)
{
    uint16_t next = (rxHead + 1U) % RX_RING_SIZE;
    if (next == rxTail)
    {
        rxDropped++;
        return;
    }
    rxRing[rxHead] = data;
    rxHead = next;
}

static uint8_t RingPop(uint8_t *data)
{
    if (rxHead == rxTail) return 0U;
    *data = rxRing[rxTail];
    rxTail = (rxTail + 1U) % RX_RING_SIZE;
    return 1U;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != LPUART1) return;
    RingPush(rxByte);
    HAL_UART_Receive_IT(huart, &rxByte, 1U);
}

static void ProcessReceivedByte(uint8_t data)
{
    if (data == '\r') return;

    if (data == '\n')
    {
        if (lineTooLong == 0U && lineLength > 0U)
        {
            lineBuffer[lineLength] = '\0';
            ExecuteCommand(lineBuffer);
        }
        lineLength = 0U;
        lineTooLong = 0U;
    }
    else if (lineLength < sizeof(lineBuffer) - 1U)
    {
        lineBuffer[lineLength++] = (char)data;
    }
    else
    {
        lineTooLong = 1U;  /* Ignore an oversized command through the next LF. */
    }
}

static void ExecuteCommand(const char *command)
{
    if (strcmp(command, "RED ON") == 0)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
    else if (strcmp(command, "RED OFF") == 0)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);
    else if (strcmp(command, "YELLOW ON") == 0)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
    else if (strcmp(command, "YELLOW OFF") == 0)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
    else if (strcmp(command, "GREEN ON") == 0)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
    else if (strcmp(command, "GREEN OFF") == 0)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
    else
    {
        uint8_t error[] = "ERR COMMAND\r\n";
        HAL_UART_Transmit(&hlpuart1, error, sizeof(error) - 1U, 100U);
        return;
    }

    commandCount++;

    /* Added for the web UI: acknowledge a command only after executing it. */
    char reply[32] = "OK ";
    strcat(reply, command);
    strcat(reply, "\r\n");
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reply, strlen(reply), 100U);
}

/* USER CODE END 4 */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6,
                      GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF12_LPUART1;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void MX_LPUART1_UART_Init(void)
{
    RCC_PeriphCLKInitTypeDef clock = {0};
    clock.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
    clock.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) Error_Handler();

    __HAL_RCC_LPUART1_CLK_ENABLE();
    hlpuart1.Instance = LPUART1;
    hlpuart1.Init.BaudRate = 115200;
    hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
    hlpuart1.Init.StopBits = UART_STOPBITS_1;
    hlpuart1.Init.Parity = UART_PARITY_NONE;
    hlpuart1.Init.Mode = UART_MODE_TX_RX;
    hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&hlpuart1) != HAL_OK) Error_Handler();
    if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK) Error_Handler();

    HAL_NVIC_SetPriority(LPUART1_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(LPUART1_IRQn);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    if (HAL_PWREx_ControlVoltageScaling(
            PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
        Error_Handler();

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = RCC_PLLM_DIV4;
    osc.PLL.PLLN = 85;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
