#include "uart.h"
#include "mmio.h"
#include "ringbuf.h"

#define RCC_APB1ENR  0x40023840u
#define GPIOA_MODER  0x40020000u
#define GPIOA_OTYPER 0x40020004u
#define GPIOA_AFRL   0x40020020u
#define USART2_SR    0x40004400u
#define USART2_DR    0x40004404u
#define USART2_BRR   0x40004408u
#define USART2_CR1   0x4000440Cu
#define NVIC_ISER1   0xE000E104u

#define USART2EN   (1u << 17)
/* RM0390 25.6.1 SR and 25.6.4 CR1 bit positions. */
#define SR_ORE     (1u << 3)
#define SR_RXNE    (1u << 5)
#define SR_TC      (1u << 6)
#define SR_TXE     (1u << 7)
#define CR1_RE     (1u << 2)
#define CR1_TE     (1u << 3)
#define CR1_RXNEIE (1u << 5)
#define CR1_TXEIE  (1u << 7)
#define CR1_UE     (1u << 13)
/* RM0390 table 38: USART2 global interrupt is IRQ 38 (ISER1 bit 6). */
#define USART2_IRQ 38u

#define RESET_PCLK1_HZ 16000000u
#define UART_BAUD 115200u
/* OVER8=0: BRR encodes round(PCLK1 / baud), here 0x008B. */
#define UART_BRR ((RESET_PCLK1_HZ + UART_BAUD / 2u) / UART_BAUD)
_Static_assert(UART_BRR >= 16u && UART_BRR <= 0xFFFFu,
               "USART divider must fit OVER8=0 BRR with USARTDIV >= 1");

static ringbuf_t tx, rx;
static volatile uint32_t rx_dropped, rx_overruns;

void uart_init(void)
{
    ringbuf_init(&tx);
    ringbuf_init(&rx);
    /* main() has already enabled GPIOA. Open and read back the APB1 gate. */
    write32(RCC_APB1ENR, read32(RCC_APB1ENR) | USART2EN);
    (void)read32(RCC_APB1ENR);

    write32(USART2_CR1, 0u);
    /* PA2 push-pull AF7 (TX), PA3 AF7 (RX); keep PA5, SWD and other fields. */
    write32(GPIOA_MODER, (read32(GPIOA_MODER) & ~(0xFu << 4)) | (0xAu << 4));
    write32(GPIOA_OTYPER, read32(GPIOA_OTYPER) & ~(1u << 2));
    write32(GPIOA_AFRL, (read32(GPIOA_AFRL) & ~(0xFFu << 8)) | (0x77u << 8));

    write32(USART2_BRR, UART_BRR);
    /* OVER8=0, M=0, PCE=0, CR2 STOP=00: 8N1. TXEIE is set only while bytes wait. */
    write32(USART2_CR1, CR1_UE | CR1_TE | CR1_RE | CR1_RXNEIE);
    write32(NVIC_ISER1, 1u << (USART2_IRQ - 32u));
}

void USART2_IRQHandler(void)
{
    uint32_t sr = read32(USART2_SR);
    if (sr & (SR_RXNE | SR_ORE)) {
        /* Reading SR then DR clears RXNE and ORE (RM0390 25.6.1). */
        uint8_t byte = (uint8_t)read32(USART2_DR);
        if (sr & SR_ORE) ++rx_overruns;
        if ((sr & SR_RXNE) && !ringbuf_put(&rx, byte)) ++rx_dropped;
    }
    uint32_t cr1 = read32(USART2_CR1);
    if ((cr1 & CR1_TXEIE) && (sr & SR_TXE)) {
        uint8_t byte;
        if (ringbuf_get(&tx, &byte)) write32(USART2_DR, byte);
        else write32(USART2_CR1, cr1 & ~CR1_TXEIE); /* nothing left: stop TXE IRQs */
    }
}

static void tx_irq_enable(void)
{
    /* CR1 is also written by the ISR, so make the read-modify-write atomic. */
    uint32_t mask = irq_save();
    write32(USART2_CR1, read32(USART2_CR1) | CR1_TXEIE);
    irq_restore(mask);
}

void uart_puts(const char *text)
{
    while (*text != '\0') {
        while (!ringbuf_put(&tx, (uint8_t)*text)) {
            tx_irq_enable(); /* buffer full: make sure the ISR is draining it */
        }
        ++text;
    }
    tx_irq_enable();
}

void uart_flush(void)
{
    while (!ringbuf_empty(&tx) || (read32(USART2_CR1) & CR1_TXEIE)) {
    }
    /* TXE only means TDR is empty. Wait for the last frame to finish too. */
    while ((read32(USART2_SR) & SR_TC) == 0u) {
    }
}

bool uart_getc(uint8_t *byte)
{
    return ringbuf_get(&rx, byte);
}

uint32_t uart_rx_dropped(void) { return rx_dropped; }
uint32_t uart_rx_overruns(void) { return rx_overruns; }
