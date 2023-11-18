/**
 * @file dmx.cpp
 *
 */
/* Copyright (C) 2018-2023 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#if __GNUC__ > 8
# pragma GCC target ("general-regs-only")
#endif

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cassert>

#include "dmx.h"
#include "./../dmx_internal.h"

#include "arm/arm.h"
#include "arm/synchronize.h"
#include "arm/gic.h"

#include "h3.h"
#include "h3_uart.h"
#include "h3_hs_timer.h"
#include "h3_dma.h"
#include "h3_ccu.h"
#include "h3_gpio.h"
#include "h3_timer.h"

#include "irq_timer.h"

#include "rdm.h"
#include "rdm_e120.h"

#include "debug.h"

using namespace dmx;

extern "C" {
int console_error(const char*);
}

#ifndef ALIGNED
# define ALIGNED __attribute__ ((aligned (4)))
#endif

static constexpr auto DMX_DATA_OUT_INDEX = (1U << 2);

enum class TxRxState {
	IDLE = 0,
	PRE_BREAK,
	BREAK,
	MAB,
	DMXDATA,
	RDMDATA,
	CHECKSUMH,
	CHECKSUML,
	RDMDISC,
	DMXINTER
};

enum class PortState {
	IDLE = 0, TX, RX
};

struct TDmxMultiData {
	uint8_t data[buffer::SIZE]; // multiple of uint32_t
	uint32_t nLength;
};

struct TCoherentRegion {
	struct sunxi_dma_lli lli[config::max::OUT];
	struct TDmxMultiData dmx_data[config::max::OUT][DMX_DATA_OUT_INDEX] ALIGNED;
};

struct TRdmMultiData {
	uint8_t data[RDM_DATA_BUFFER_SIZE];
	uint16_t nChecksum;	// This must be uint16_t
	uint16_t _padding;
	uint32_t nIndex;
	uint32_t nDiscIndex;
};

static volatile TxRxState s_PortReceiveState[config::max::OUT] ALIGNED;

#if defined(ORANGE_PI)
static constexpr uint8_t s_nDmxDataDirectionGpioPin[config::max::OUT] = {
		GPIO_DMX_DATA_DIRECTION_OUT_C,
		GPIO_DMX_DATA_DIRECTION_OUT_B };
#else
static constexpr uint8_t s_nDmxDataDirectionGpioPin[config::max::OUT] = {
		GPIO_DMX_DATA_DIRECTION_OUT_D,
		GPIO_DMX_DATA_DIRECTION_OUT_A,
		GPIO_DMX_DATA_DIRECTION_OUT_B,
		GPIO_DMX_DATA_DIRECTION_OUT_C };
#endif

// DMX TX

static uint32_t s_nDmxTransmistBreakTimeINTV;
static uint32_t s_nDmxTransmitMabTimeINTV;
static uint32_t s_nDmxTransmitPeriodINTV;

static struct TCoherentRegion *s_pCoherentRegion;

static volatile uint32_t s_nDmxDataWriteIndex[config::max::OUT];
static volatile uint32_t s_nDmxDataReadIndex[config::max::OUT];

static volatile TxRxState s_tDmxSendState ALIGNED;

// DMX RX

static uint8_t s_RxDmxPrevious[config::max::IN][buffer::SIZE] ALIGNED;
static volatile struct Data s_aDmxData[config::max::IN][buffer::INDEX_ENTRIES] ALIGNED;
static volatile uint32_t s_nDmxDataBufferIndexHead[config::max::IN];
static volatile uint32_t s_nDmxDataBufferIndexTail[config::max::IN];
static volatile uint32_t s_nDmxDataIndex[config::max::IN];

static volatile uint32_t s_nDmxUpdatesPerSecond[config::max::IN];
static volatile uint32_t s_nDmxPackets[config::max::IN];
static volatile uint32_t s_nDmxPacketsPrevious[config::max::IN];

// RDM

static volatile uint32_t sv_RdmDataReceiveEnd;

static struct TRdmMultiData s_aRdmData[config::max::OUT][RDM_DATA_BUFFER_INDEX_ENTRIES] ALIGNED;
static struct TRdmMultiData *s_pRdmDataCurrent[config::max::OUT] ALIGNED;

static volatile uint32_t s_nRdmDataWriteIndex[config::max::OUT];
static volatile uint32_t s_nRdmDataReadIndex[config::max::OUT];

static volatile PortState sv_PortState[config::max::OUT] ALIGNED;

static char CONSOLE_ERROR[] ALIGNED = "DMXDATA %\n";
static constexpr auto CONSOLE_ERROR_LENGTH = (sizeof(CONSOLE_ERROR) / sizeof(CONSOLE_ERROR[0]));

static void irq_timer0_dmx_multi_sender(__attribute__((unused))uint32_t clo) {
#ifdef LOGIC_ANALYZER
	h3_gpio_set(6);
#endif
	switch (s_tDmxSendState) {
	case TxRxState::IDLE:
	case TxRxState::DMXINTER:
		H3_TIMER->TMR0_INTV = s_nDmxTransmistBreakTimeINTV;
		H3_TIMER->TMR0_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD); // 0x3;

		if (sv_PortState[0] == PortState::TX) {
			H3_UART1->LCR = UART_LCR_8_N_2 | UART_LCR_BC;
		}

		if (sv_PortState[1] == PortState::TX) {
			H3_UART2->LCR = UART_LCR_8_N_2 | UART_LCR_BC;
		}
#if defined (ORANGE_PI_ONE)
		if (sv_PortState[2] == PortState::TX) {
			H3_UART3->LCR = UART_LCR_8_N_2 | UART_LCR_BC;
		}
# ifndef DO_NOT_USE_UART0
		if (sv_PortState[3] == PortState::TX) {
			H3_UART0->LCR = UART_LCR_8_N_2 | UART_LCR_BC;
		}
# endif
#endif

		if (s_nDmxDataWriteIndex[0] != s_nDmxDataReadIndex[0]) {
			s_nDmxDataReadIndex[0] = (s_nDmxDataReadIndex[0] + 1) & (DMX_DATA_OUT_INDEX - 1);

			s_pCoherentRegion->lli[0].src = reinterpret_cast<uint32_t>(&s_pCoherentRegion->dmx_data[0][s_nDmxDataReadIndex[0]].data[0]);
			s_pCoherentRegion->lli[0].len = s_pCoherentRegion->dmx_data[0][s_nDmxDataReadIndex[0]].nLength;
		}

		if (s_nDmxDataWriteIndex[1] != s_nDmxDataReadIndex[1]) {
			s_nDmxDataReadIndex[1] = (s_nDmxDataReadIndex[1] + 1) & (DMX_DATA_OUT_INDEX - 1);

			s_pCoherentRegion->lli[1].src = reinterpret_cast<uint32_t>(&s_pCoherentRegion->dmx_data[1][s_nDmxDataReadIndex[1]].data[0]);
			s_pCoherentRegion->lli[1].len = s_pCoherentRegion->dmx_data[1][s_nDmxDataReadIndex[1]].nLength;
		}
#if defined (ORANGE_PI_ONE)
		if (s_nDmxDataWriteIndex[2] != s_nDmxDataReadIndex[2]) {
			s_nDmxDataReadIndex[2] = (s_nDmxDataReadIndex[2] + 1) & (DMX_DATA_OUT_INDEX - 1);

			s_pCoherentRegion->lli[2].src = reinterpret_cast<uint32_t>(&s_pCoherentRegion->dmx_data[2][s_nDmxDataReadIndex[2]].data[0]);
			s_pCoherentRegion->lli[2].len = s_pCoherentRegion->dmx_data[2][s_nDmxDataReadIndex[2]].nLength;
		}
# ifndef DO_NOT_USE_UART0
		if (s_nDmxDataWriteIndex[3] != s_nDmxDataReadIndex[3]) {
			s_nDmxDataReadIndex[3] = (s_nDmxDataReadIndex[3] + 1) & (DMX_DATA_OUT_INDEX - 1);

			s_pCoherentRegion->lli[3].src = reinterpret_cast<uint32_t>(&s_pCoherentRegion->dmx_data[3][s_nDmxDataReadIndex[3]].data[0]);
			s_pCoherentRegion->lli[3].len = s_pCoherentRegion->dmx_data[3][s_nDmxDataReadIndex[3]].nLength;
		}
# endif
#endif
		s_tDmxSendState = TxRxState::BREAK;
		break;
	case TxRxState::BREAK:
		H3_TIMER->TMR0_INTV = s_nDmxTransmitMabTimeINTV;
		H3_TIMER->TMR0_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD); // 0x3;

		if (sv_PortState[0] == PortState::TX) {
			H3_UART1->LCR = UART_LCR_8_N_2;
		}

		if (sv_PortState[1] == PortState::TX) {
			H3_UART2->LCR = UART_LCR_8_N_2;
		}
#if defined (ORANGE_PI_ONE)
		if (sv_PortState[2] == PortState::TX) {
			H3_UART3->LCR = UART_LCR_8_N_2;
		}
# ifndef DO_NOT_USE_UART0
		if (sv_PortState[3] == PortState::TX) {
			H3_UART0->LCR = UART_LCR_8_N_2;
		}
# endif
#endif
		s_tDmxSendState = TxRxState::MAB;
		break;
	case TxRxState::MAB:
		H3_TIMER->TMR0_INTV = s_nDmxTransmitPeriodINTV;
		H3_TIMER->TMR0_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD); // 0x3;

		if (sv_PortState[0] == PortState::TX) {
			H3_DMA_CHL0->DESC_ADDR = reinterpret_cast<uint32_t>(&s_pCoherentRegion->lli[0]);
			H3_DMA_CHL0->EN = DMA_CHAN_ENABLE_START;
		}

		if (sv_PortState[1] == PortState::TX) {
			H3_DMA_CHL1->DESC_ADDR = reinterpret_cast<uint32_t>(&s_pCoherentRegion->lli[1]);
			H3_DMA_CHL1->EN = DMA_CHAN_ENABLE_START;
		}
#if defined (ORANGE_PI_ONE)
		if (sv_PortState[2] == PortState::TX) {
			H3_DMA_CHL2->DESC_ADDR = reinterpret_cast<uint32_t>(&s_pCoherentRegion->lli[2]);
			H3_DMA_CHL2->EN = DMA_CHAN_ENABLE_START;
		}
# ifndef DO_NOT_USE_UART0
		if (sv_PortState[3] == PortState::TX) {
			H3_DMA_CHL3->DESC_ADDR = reinterpret_cast<uint32_t>(&s_pCoherentRegion->lli[3]);
			H3_DMA_CHL3->EN = DMA_CHAN_ENABLE_START;
		}
# endif
#endif
		isb();

		s_tDmxSendState = TxRxState::DMXINTER;
		break;
	default:
		assert(0);
		__builtin_unreachable();
		break;
	}
#ifdef LOGIC_ANALYZER
	h3_gpio_clr(6);
#endif
}

#include <cstdio>

static void fiq_in_handler(const uint32_t nUart, const H3_UART_TypeDef *pUart, const uint32_t nIIR) {
#ifdef LOGIC_ANALYZER
	h3_gpio_set(3);
#endif
	uint32_t nIndex;

	isb();

	if (pUart->LSR & (UART_LSR_BI | UART_LSR_FE | UART_LSR_FIFOERR)) {
		s_PortReceiveState[nUart] = TxRxState::PRE_BREAK;
	}

	auto nRFL = pUart->RFL;

	while(nRFL--) {
		while ((pUart->LSR & UART_LSR_DR) != UART_LSR_DR)
			;
		const auto nData = static_cast<uint8_t>(pUart->O00.RBR);
		dmb();

		switch (s_PortReceiveState[nUart]) {
		case TxRxState::IDLE:
			s_pRdmDataCurrent[nUart]->data[0] = nData;
			s_pRdmDataCurrent[nUart]->nIndex = 1;

			s_PortReceiveState[nUart] = TxRxState::RDMDISC;
			break;
		case TxRxState::PRE_BREAK:
			s_PortReceiveState[nUart] = TxRxState::BREAK;
			break;
		case TxRxState::BREAK:
			switch (nData) {
			case START_CODE:
				s_PortReceiveState[nUart] = TxRxState::DMXDATA;
				s_aDmxData[nUart][s_nDmxDataBufferIndexHead[nUart]].Data[0] = START_CODE;
				s_nDmxDataIndex[nUart] = 1;
				s_nDmxPackets[nUart]++;
				break;
			case E120_SC_RDM:
				s_pRdmDataCurrent[nUart]->data[0] = E120_SC_RDM;
				s_pRdmDataCurrent[nUart]->nChecksum = E120_SC_RDM;
				s_pRdmDataCurrent[nUart]->nIndex = 1;

				s_PortReceiveState[nUart] = TxRxState::RDMDATA;
				break;
			default:
				s_PortReceiveState[nUart] = TxRxState::IDLE;
				break;
			}
			break;
		case TxRxState::DMXDATA:
			s_aDmxData[nUart][s_nDmxDataBufferIndexHead[nUart]].Data[s_nDmxDataIndex[nUart]] = nData;
			s_nDmxDataIndex[nUart]++;

			if (s_nDmxDataIndex[nUart] > max::CHANNELS) {
				s_PortReceiveState[nUart] = TxRxState::IDLE;
				s_aDmxData[nUart][s_nDmxDataBufferIndexHead[nUart]].Statistics.nSlotsInPacket = max::CHANNELS;
				s_nDmxDataBufferIndexHead[nUart] = (s_nDmxDataBufferIndexHead[nUart] + 1) & buffer::INDEX_MASK;
				return;
			}
			break;
		case TxRxState::RDMDATA:
			if (s_pRdmDataCurrent[nUart]->nIndex > RDM_DATA_BUFFER_SIZE) {
				s_PortReceiveState[nUart] = TxRxState::IDLE;
			} else {
				nIndex = s_pRdmDataCurrent[nUart]->nIndex;
				s_pRdmDataCurrent[nUart]->data[nIndex] = nData;
				s_pRdmDataCurrent[nUart]->nIndex++;

				s_pRdmDataCurrent[nUart]->nChecksum = static_cast<uint16_t>(s_pRdmDataCurrent[nUart]->nChecksum + nData);

				const auto *p = reinterpret_cast<struct TRdmMessage *>(&s_pRdmDataCurrent[nUart]->data[0]);

				if (s_pRdmDataCurrent[nUart]->nIndex == p->message_length) {
					s_PortReceiveState[nUart] = TxRxState::CHECKSUMH;
				}
			}
			break;
		case TxRxState::CHECKSUMH:
			nIndex = s_pRdmDataCurrent[nUart]->nIndex;
			s_pRdmDataCurrent[nUart]->data[nIndex] = nData;
			s_pRdmDataCurrent[nUart]->nIndex++;
			s_pRdmDataCurrent[nUart]->nChecksum = static_cast<uint16_t>(s_pRdmDataCurrent[nUart]->nChecksum - static_cast<uint16_t>(nData << 8));

			s_PortReceiveState[nUart] = TxRxState::CHECKSUML;
			break;
		case TxRxState::CHECKSUML: {
			nIndex = s_pRdmDataCurrent[nUart]->nIndex;
			s_pRdmDataCurrent[nUart]->data[nIndex] = nData;
			s_pRdmDataCurrent[nUart]->nIndex++;

			s_pRdmDataCurrent[nUart]->nChecksum = static_cast<uint16_t>(s_pRdmDataCurrent[nUart]->nChecksum - nData);

			const auto *p = reinterpret_cast<struct TRdmMessage *>(&s_aRdmData[nUart][s_nRdmDataWriteIndex[nUart]].data[0]);

			if ((s_aRdmData[nUart][s_nRdmDataWriteIndex[nUart]].nChecksum == 0) && (p->sub_start_code == E120_SC_SUB_MESSAGE)) {
				s_nRdmDataWriteIndex[nUart] = (s_nRdmDataWriteIndex[nUart] + 1) & RDM_DATA_BUFFER_INDEX_MASK;
				s_pRdmDataCurrent[nUart] = &s_aRdmData[nUart][s_nRdmDataWriteIndex[nUart]];
				sv_RdmDataReceiveEnd = h3_hs_timer_lo_us();
				dmb();
			}

			s_PortReceiveState[nUart] = TxRxState::IDLE;
		}
			break;
		case TxRxState::RDMDISC:
			nIndex = s_pRdmDataCurrent[nUart]->nIndex;

			if (nIndex < 24) {
				s_pRdmDataCurrent[nUart]->data[nIndex] = nData;
				s_pRdmDataCurrent[nUart]->nIndex++;
			}

			break;
		default:
			s_PortReceiveState[nUart] = TxRxState::IDLE;
			break;
		}
	}

	if (((pUart->USR & UART_USR_BUSY) == 0) && ((nIIR & UART_IIR_IID_TIME_OUT) == UART_IIR_IID_TIME_OUT)) {
		if (s_PortReceiveState[nUart] == TxRxState::DMXDATA) {
			s_PortReceiveState[nUart] = TxRxState::IDLE;
			s_aDmxData[nUart][s_nDmxDataBufferIndexHead[nUart]].Statistics.nSlotsInPacket = s_nDmxDataIndex[nUart] - 1;
			s_nDmxDataBufferIndexHead[nUart] = (s_nDmxDataBufferIndexHead[nUart] + 1) & buffer::INDEX_MASK;
		}

		if (s_PortReceiveState[nUart] == TxRxState::RDMDISC) {
			s_PortReceiveState[nUart] = TxRxState::IDLE;
			s_nRdmDataWriteIndex[nUart] = (s_nRdmDataWriteIndex[nUart] + 1) & RDM_DATA_BUFFER_INDEX_MASK;
			s_pRdmDataCurrent[nUart] = &s_aRdmData[nUart][s_nRdmDataWriteIndex[nUart]];
			sv_RdmDataReceiveEnd = h3_hs_timer_lo_us();
			dmb();
			h3_gpio_clr(10);
		}
	}
#ifdef LOGIC_ANALYZER
	h3_gpio_clr(3);
#endif
}

static void __attribute__((interrupt("FIQ"))) fiq_dmx_multi(void) {
	dmb();
#ifdef LOGIC_ANALYZER
	h3_gpio_set(3);
#endif
	auto nIIR = H3_UART1->O08.IIR;
	if (nIIR & UART_IIR_IID_RD) {
		fiq_in_handler(1, reinterpret_cast<H3_UART_TypeDef *>(H3_UART1_BASE), nIIR);
		H3_GIC_CPUIF->EOI = H3_UART1_IRQn;
		gic_unpend(H3_UART1_IRQn);
	}

	nIIR = H3_UART2->O08.IIR;
	if (nIIR & UART_IIR_IID_RD) {
		fiq_in_handler(2, reinterpret_cast<H3_UART_TypeDef *>(H3_UART2_BASE), nIIR);
		H3_GIC_CPUIF->EOI = H3_UART2_IRQn;
		gic_unpend(H3_UART2_IRQn);
	}
#if defined (ORANGE_PI_ONE)
	nIIR = H3_UART3->O08.IIR;
	if (nIIR & UART_IIR_IID_RD) {
		fiq_in_handler(3, reinterpret_cast<H3_UART_TypeDef *>(H3_UART3_BASE), nIIR);
		H3_GIC_CPUIF->EOI = H3_UART3_IRQn;
		gic_unpend(H3_UART3_IRQn);
	}

# ifndef DO_NOT_USE_UART0
	nIIR = H3_UART0->O08.IIR;
	if (nIIR & UART_IIR_IID_RD) {
		fiq_in_handler(0, reinterpret_cast<H3_UART_TypeDef *>(H3_UART0_BASE), nIIR);
		H3_GIC_CPUIF->EOI = H3_UART0_IRQn;
		gic_unpend(H3_UART0_IRQn);
	}
# endif
#endif
#ifdef LOGIC_ANALYZER
	h3_gpio_clr(3);
#endif
	dmb();
}

static void irq_timer1_dmx_receive(__attribute__((unused)) uint32_t clo) {
	for (uint32_t i = 0; i < config::max::IN; i++) {
		s_nDmxUpdatesPerSecond[i] = s_nDmxPackets[i] - s_nDmxPacketsPrevious[i];
		s_nDmxPacketsPrevious[i] = s_nDmxPackets[i];
	}
}

static void uart_config(uint32_t nUart) {
	H3_UART_TypeDef *p = nullptr;

	if (nUart == 1) {
		p = reinterpret_cast<H3_UART_TypeDef *>(H3_UART1_BASE);

		uint32_t value = H3_PIO_PORTG->CFG0;
		// PG6, TX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PG6_SELECT_CFG0_SHIFT));
		value |= H3_PG6_SELECT_UART1_TX << PG6_SELECT_CFG0_SHIFT;
		// PG7, RX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PG7_SELECT_CFG0_SHIFT));
		value |= H3_PG7_SELECT_UART1_RX << PG7_SELECT_CFG0_SHIFT;
		H3_PIO_PORTG->CFG0 = value;

		H3_CCU->BUS_SOFT_RESET4 |= CCU_BUS_SOFT_RESET4_UART1;
		H3_CCU->BUS_CLK_GATING3 |= CCU_BUS_CLK_GATING3_UART1;
	} else if (nUart == 2) {
		p = reinterpret_cast<H3_UART_TypeDef *>(H3_UART2_BASE);

		uint32_t value = H3_PIO_PORTA->CFG0;
		// PA0, TX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PA0_SELECT_CFG0_SHIFT));
		value |= H3_PA0_SELECT_UART2_TX << PA0_SELECT_CFG0_SHIFT;
		// PA1, RX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PA1_SELECT_CFG0_SHIFT));
		value |= H3_PA1_SELECT_UART2_RX << PA1_SELECT_CFG0_SHIFT;
		H3_PIO_PORTA->CFG0 = value;

		H3_CCU->BUS_SOFT_RESET4 |= CCU_BUS_SOFT_RESET4_UART2;
		H3_CCU->BUS_CLK_GATING3 |= CCU_BUS_CLK_GATING3_UART2;
	}
#if defined (ORANGE_PI_ONE)
	else if (nUart == 3) {
		p = reinterpret_cast<H3_UART_TypeDef *>(H3_UART3_BASE);

		uint32_t value = H3_PIO_PORTA->CFG1;
		// PA13, TX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PA13_SELECT_CFG1_SHIFT));
		value |= H3_PA13_SELECT_UART3_TX << PA13_SELECT_CFG1_SHIFT;
		// PA14, RX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PA14_SELECT_CFG1_SHIFT));
		value |= H3_PA14_SELECT_UART3_RX << PA14_SELECT_CFG1_SHIFT;
		H3_PIO_PORTA->CFG1 = value;

		H3_CCU->BUS_SOFT_RESET4 |= CCU_BUS_SOFT_RESET4_UART3;
		H3_CCU->BUS_CLK_GATING3 |= CCU_BUS_CLK_GATING3_UART3;
	}
# ifndef DO_NOT_USE_UART0
	else if (nUart == 0) {
		p = reinterpret_cast<H3_UART_TypeDef *>(H3_UART0_BASE);

		uint32_t value = H3_PIO_PORTA->CFG0;
		// PA4, TX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PA4_SELECT_CFG0_SHIFT));
		value |= H3_PA4_SELECT_UART0_TX << PA4_SELECT_CFG0_SHIFT;
		// PA5, RX
		value &= static_cast<uint32_t>(~(GPIO_SELECT_MASK << PA5_SELECT_CFG0_SHIFT));
		value |= H3_PA5_SELECT_UART0_RX << PA5_SELECT_CFG0_SHIFT;
		H3_PIO_PORTA->CFG0 = value;

		H3_CCU->BUS_SOFT_RESET4 |= CCU_BUS_SOFT_RESET4_UART0;
		H3_CCU->BUS_CLK_GATING3 |= CCU_BUS_CLK_GATING3_UART0;
	}
# endif
#endif

	assert(p != nullptr);

	if (p != nullptr) {
		p->O08.FCR = 0;
		p->LCR = UART_LCR_DLAB;
		p->O00.DLL = BAUD_250000_L;
		p->O04.DLH = BAUD_250000_H;
		p->O04.IER = 0;
		p->LCR = UART_LCR_8_N_2;
	}

	isb();
}

static void UartEnableFifoTx(const uint32_t nPortIndex) {	// DMX TX
	auto *pUart = _port_to_uart(nPortIndex);
	assert(pUart != nullptr);

	if (pUart != nullptr) {
		pUart->O08.FCR = UART_FCR_EFIFO | UART_FCR_TRESET;
		pUart->O04.IER = 0;
		isb();
	}
}

static void UartEnableFifoRx(const uint32_t nPortIndex) {	// RDM RX
	auto *pUart = _port_to_uart(nPortIndex);
	assert(pUart != nullptr);

	if (pUart != nullptr) {
		pUart->O08.FCR = UART_FCR_EFIFO | UART_FCR_RRESET | UART_FCR_TRIG1;
		pUart->O04.IER = UART_IER_ERBFI;
		isb();
	}
}

Dmx *Dmx::s_pThis = nullptr;

Dmx::Dmx() {
	DEBUG_ENTRY

	assert(s_pThis == nullptr);
	s_pThis = this;

	// DMX TX

	s_pCoherentRegion = reinterpret_cast<struct TCoherentRegion *>(H3_MEM_COHERENT_REGION + MEGABYTE/2);

	s_nDmxTransmistBreakTimeINTV = transmit::BREAK_TIME_MIN * 12 ;
	s_nDmxTransmitMabTimeINTV = transmit::MAB_TIME_MIN * 12 ;
	s_nDmxTransmitPeriodINTV = (transmit::PERIOD_DEFAULT * 12) - (transmit::MAB_TIME_MIN * 12) - (transmit::BREAK_TIME_MIN * 12);

	s_tDmxSendState = TxRxState::IDLE;

	for (uint32_t i = 0; i < config::max::OUT; i++) {
		// DMX TX
		ClearData(i);
		s_nDmxDataWriteIndex[i] = 0;
		s_nDmxDataReadIndex[i] = 0;
		m_nDmxTransmissionLength[i] = 0;
		// DMA UART TX
		auto *lli = &s_pCoherentRegion->lli[i];
		H3_UART_TypeDef *p = _port_to_uart(i);

		lli->cfg = DMA_CHAN_CFG_DST_IO_MODE | DMA_CHAN_CFG_SRC_LINEAR_MODE
				| DMA_CHAN_CFG_SRC_DRQ(DRQSRC_SDRAM)
				| DMA_CHAN_CFG_DST_DRQ(i + DRQDST_UART0TX);
		lli->src = reinterpret_cast<uint32_t>(&s_pCoherentRegion->dmx_data[i][s_nDmxDataReadIndex[i]].data[0]);
		lli->dst = reinterpret_cast<uint32_t>(&p->O00.THR);
		lli->len = s_pCoherentRegion->dmx_data[i][s_nDmxDataReadIndex[i]].nLength;
		lli->para = DMA_NORMAL_WAIT;
		lli->p_lli_next = DMA_LLI_LAST_ITEM;
		//
		m_dmxPortDirection[i] = PortDirection::INP;
		//
		sv_PortState[i] = PortState::IDLE;
		// RDM RX
		s_nRdmDataWriteIndex[i] = 0;
		s_nRdmDataReadIndex[i] = 0;
		s_pRdmDataCurrent[i] = &s_aRdmData[i][0];
		s_PortReceiveState[i] = TxRxState::IDLE;
		// DMX RX
		s_nDmxDataBufferIndexHead[i] = 0;
		s_nDmxDataBufferIndexTail[i] = 0;
		s_nDmxDataIndex[i] = 0;
		s_nDmxUpdatesPerSecond[i] = 0;
		s_nDmxPackets[i] = 0;
		s_nDmxPacketsPrevious[i] = 0;
	}

#ifdef LOGIC_ANALYZER
	h3_gpio_fsel(3, GPIO_FSEL_OUTPUT);
	h3_gpio_clr(3);
	h3_gpio_fsel(6, GPIO_FSEL_OUTPUT);
	h3_gpio_clr(6);
	h3_gpio_fsel(10, GPIO_FSEL_OUTPUT);
	h3_gpio_clr(10);
#endif

	h3_gpio_fsel(s_nDmxDataDirectionGpioPin[0], GPIO_FSEL_OUTPUT);
	h3_gpio_clr(s_nDmxDataDirectionGpioPin[0]);	// 0 = input, 1 = output
	h3_gpio_fsel(s_nDmxDataDirectionGpioPin[1], GPIO_FSEL_OUTPUT);
	h3_gpio_clr(s_nDmxDataDirectionGpioPin[1]);	// 0 = input, 1 = output
#if defined (ORANGE_PI_ONE)
	h3_gpio_fsel(s_nDmxDataDirectionGpioPin[2], GPIO_FSEL_OUTPUT);
	h3_gpio_clr(s_nDmxDataDirectionGpioPin[2]);	// 0 = input, 1 = output
# ifndef DO_NOT_USE_UART0
	h3_gpio_fsel(s_nDmxDataDirectionGpioPin[3], GPIO_FSEL_OUTPUT);
	h3_gpio_clr(s_nDmxDataDirectionGpioPin[3]);	// 0 = input, 1 = output
# endif
#endif

	uart_config(1);
	uart_config(2);
#if defined (ORANGE_PI_ONE)
	uart_config(3);
# ifndef DO_NOT_USE_UART0
	uart_config(0);
# endif
#endif

	__disable_fiq();

	arm_install_handler(reinterpret_cast<unsigned>(fiq_dmx_multi), ARM_VECTOR(ARM_VECTOR_FIQ));

	gic_fiq_config(H3_UART1_IRQn, GIC_CORE0);
	gic_fiq_config(H3_UART2_IRQn, GIC_CORE0);
#if defined (ORANGE_PI_ONE)
	gic_fiq_config(H3_UART3_IRQn, GIC_CORE0);
# ifndef DO_NOT_USE_UART0
	gic_fiq_config(H3_UART0_IRQn, GIC_CORE0);
# endif
#endif

	UartEnableFifoTx(0);
	UartEnableFifoTx(1);
#if defined (ORANGE_PI_ONE)
	UartEnableFifoTx(2);
# ifndef DO_NOT_USE_UART0
	UartEnableFifoTx(3);
# endif
#endif

	irq_timer_init();
	irq_timer_set(IRQ_TIMER_0, irq_timer0_dmx_multi_sender);
	irq_timer_set(IRQ_TIMER_1, irq_timer1_dmx_receive);

	H3_TIMER->TMR0_CTRL |= TIMER_CTRL_SINGLE_MODE;
	H3_TIMER->TMR0_INTV = 12000; // Wait 1ms
	H3_TIMER->TMR0_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD); // 0x3;

	H3_TIMER->TMR1_INTV = 0xB71B00; // 1 second
	H3_TIMER->TMR1_CTRL &= ~(TIMER_CTRL_SINGLE_MODE);
	H3_TIMER->TMR1_CTRL |= (TIMER_CTRL_EN_START | TIMER_CTRL_RELOAD);

	H3_CCU->BUS_SOFT_RESET0 |= CCU_BUS_SOFT_RESET0_DMA;
	H3_CCU->BUS_CLK_GATING0 |= CCU_BUS_CLK_GATING0_DMA;

#if 0
	H3_DMA->IRQ_PEND0 |= H3_DMA->IRQ_PEND0;
	H3_DMA->IRQ_PEND1 |= H3_DMA->IRQ_PEND1;

	H3_DMA->IRQ_EN0 = DMA_IRQ_EN0_DMA0_PKG_IRQ_EN | DMA_IRQ_EN0_DMA1_PKG_IRQ_EN
			| DMA_IRQ_EN0_DMA2_PKG_IRQ_EN | DMA_IRQ_EN0_DMA3_PKG_IRQ_EN;
#endif
	isb();

	__enable_fiq();

	DEBUG_EXIT
}

void Dmx::SetPortDirection(const uint32_t nPortIndex, PortDirection portDirection, bool bEnableData) {
	DEBUG_PRINTF("nPort=%d, portDirection=%d, bEnableData=%d", nPortIndex, portDirection, bEnableData);
	assert(nPortIndex < config::max::OUT);

	const auto nUart = _port_to_uart(nPortIndex);

	if (m_dmxPortDirection[nPortIndex] != portDirection) {
		m_dmxPortDirection[nPortIndex] = portDirection;

		StopData(nUart, nPortIndex);

		switch (portDirection) {
		case PortDirection::OUTP:
			h3_gpio_set(s_nDmxDataDirectionGpioPin[nPortIndex]);	// 0 = input, 1 = output
			break;
		case PortDirection::INP:
			h3_gpio_clr(s_nDmxDataDirectionGpioPin[nPortIndex]);	// 0 = input, 1 = output
			break;
		default:
			assert(0);
			__builtin_unreachable();
			break;
		}
	} else if (!bEnableData) {
		StopData(nUart, nPortIndex);
	}

	if (bEnableData) {
		StartData(nUart, nPortIndex);
	}
}

void Dmx::ClearData(uint32_t nPortIndex) {
	for (uint32_t j = 0; j < DMX_DATA_OUT_INDEX; j++) {
		auto *p = &s_pCoherentRegion->dmx_data[nPortIndex][j];
		auto *p32 = reinterpret_cast<uint32_t *>(p->data);

		for (uint32_t i = 0; i < buffer::SIZE / 4; i++) {
			*p32++ = 0;
		}

		p->nLength = 513; // Including START Code
	}
}

void Dmx::StartDmxOutput(__attribute__((unused)) const uint32_t nPortIndex) {
	// Nothing to do here
}

void Dmx::StartOutput(__attribute__((unused)) const uint32_t nPortIndex) {
	// Nothing to do here
}

void Dmx::SetOutput(__attribute__((unused)) const bool doForce) {
	// Nothing to do here
}

void Dmx::StartData(H3_UART_TypeDef *pUart, const uint32_t nPortIndex) {
	assert(sv_PortState[nPortIndex] == PortState::IDLE);

	switch (m_dmxPortDirection[nPortIndex]) {
	case PortDirection::OUTP:
		UartEnableFifoTx(nPortIndex);
		sv_PortState[nPortIndex] = PortState::TX;
		dmb();
		break;
	case PortDirection::INP: {
		if (pUart != nullptr) {
			while (!(pUart->USR & UART_USR_TFE))
				;
		}

		UartEnableFifoRx(nPortIndex);
		s_PortReceiveState[nPortIndex] = TxRxState::IDLE;
		sv_PortState[nPortIndex] = PortState::RX;
		dmb();
		break;
	}
	default:
		assert(0);
		__builtin_unreachable();
		break;
	}
}

void Dmx::StopData(H3_UART_TypeDef *pUart, const uint32_t nPortIndex) {
	assert(pUart != nullptr);
	assert(nPortIndex < config::max::OUT);

	dmb();
	if (sv_PortState[nPortIndex] == PortState::IDLE) {
		return;
	}

	if (m_dmxPortDirection[nPortIndex] == PortDirection::OUTP) {
		auto IsIdle = false;

		do {
			dmb();
			if (s_tDmxSendState == TxRxState::DMXINTER) {
				while (!(pUart->USR & UART_USR_TFE))
					;
				IsIdle = true;
			}
		} while (!IsIdle);
	} else if (m_dmxPortDirection[nPortIndex] == PortDirection::INP) {
		pUart->O08.FCR = 0;
		pUart->O04.IER = 0;

		s_PortReceiveState[nPortIndex] = TxRxState::IDLE;
	}

	sv_PortState[nPortIndex] = PortState::IDLE;
	dmb();
}

// DMX Send

void Dmx::SetDmxBreakTime(uint32_t nBreakTime) {
	DEBUG_PRINTF("nBreakTime=%u", nBreakTime);

	m_nDmxTransmitBreakTime = std::max(transmit::BREAK_TIME_MIN, nBreakTime);
	s_nDmxTransmistBreakTimeINTV = m_nDmxTransmitBreakTime * 12;
	//
	SetDmxPeriodTime(m_nDmxTransmitPeriodRequested);
}

void Dmx::SetDmxMabTime(uint32_t nMabTime) {
	DEBUG_PRINTF("nMabTime=%u", nMabTime);

	m_nDmxTransmitMabTime = std::min(std::max(transmit::MAB_TIME_MIN, nMabTime), transmit::MAB_TIME_MAX);
	s_nDmxTransmitMabTimeINTV = m_nDmxTransmitMabTime * 12;
	//
	SetDmxPeriodTime(m_nDmxTransmitPeriodRequested);
}

void Dmx::SetDmxPeriodTime(uint32_t nPeriod) {
	DEBUG_ENTRY
	DEBUG_PRINTF("nPeriod=%u", nPeriod);

	m_nDmxTransmitPeriodRequested = nPeriod;

	auto nLengthMax = m_nDmxTransmissionLength[0];

	DEBUG_PRINTF("nLengthMax=%u", nLengthMax);

	for (uint32_t i = 1; i < config::max::OUT; i++) {
		if (m_nDmxTransmissionLength[i] > nLengthMax) {
			nLengthMax = m_nDmxTransmissionLength[i];
		}
	}

	const auto nPackageLengthMicroSeconds = m_nDmxTransmitBreakTime + m_nDmxTransmitMabTime + (nLengthMax * 44) + 44;

	if (nPeriod != 0) {
		if (nPeriod < nPackageLengthMicroSeconds) {
			m_nDmxTransmitPeriod = std::max(transmit::BREAK_TO_BREAK_TIME_MIN, nPackageLengthMicroSeconds + 44);
		} else {
			m_nDmxTransmitPeriod = nPeriod;
		}
	} else {
		m_nDmxTransmitPeriod =  std::max(transmit::BREAK_TO_BREAK_TIME_MIN, nPackageLengthMicroSeconds + 44);
	}

	s_nDmxTransmitPeriodINTV = (m_nDmxTransmitPeriod * 12) - s_nDmxTransmistBreakTimeINTV - s_nDmxTransmitMabTimeINTV;

	DEBUG_PRINTF("nPeriod=%u, nLengthMax=%u, m_nDmxTransmitPeriod=%u", nPeriod, nLengthMax, m_nDmxTransmitPeriod);
	DEBUG_ENTRY
}

void Dmx::SetDmxSlots(uint16_t nSlots) {
	DEBUG_ENTRY
	DEBUG_PRINTF("nSlots=%u", nSlots);

	if ((nSlots >= 2) && (nSlots <= dmx::max::CHANNELS)) {
		m_nDmxTransmitSlots = nSlots;

		for (uint32_t i = 0; i < config::max::OUT; i++) {
			if (m_nDmxTransmissionLength[i] != 0) {
				m_nDmxTransmissionLength[i] = std::min(m_nDmxTransmissionLength[i], static_cast<uint32_t>(nSlots));
				DEBUG_PRINTF("m_nDmxTransmissionLength[%u]=%u", i, m_nDmxTransmissionLength[i]);
			}
		}

		SetDmxPeriodTime(m_nDmxTransmitPeriodRequested);
	}

	DEBUG_EXIT
}

void Dmx::SetOutputStyle(__attribute__((unused)) const uint32_t nPortIndex, __attribute__((unused))  const dmx::OutputStyle outputStyle) {

}

dmx::OutputStyle Dmx::GetOutputStyle(__attribute__((unused)) const uint32_t nPortIndex) const {
	return dmx::OutputStyle::CONTINOUS;
}

void Dmx::SetSendDataWithoutSC(const uint32_t nPortIndex, const uint8_t *pData, uint32_t nLength) {
	assert(pData != 0);
	assert(nLength != 0);

	const auto nNext = (s_nDmxDataWriteIndex[nPortIndex] + 1) & (DMX_DATA_OUT_INDEX - 1);
	auto *p = &s_pCoherentRegion->dmx_data[nPortIndex][nNext];

	auto *pDst = p->data;
	nLength = std::min(nLength, static_cast<uint32_t>(m_nDmxTransmitSlots));
	p->nLength = nLength + 1U;

	__builtin_prefetch(pData);
	memcpy(&pDst[1], pData,  nLength);

	if (nLength != m_nDmxTransmissionLength[nPortIndex]) {
		m_nDmxTransmissionLength[nPortIndex] = nLength;
		SetDmxPeriodTime(m_nDmxTransmitPeriodRequested);
	}

	s_nDmxDataWriteIndex[nPortIndex] = nNext;
}

void Dmx::Blackout() {
	DEBUG_ENTRY

	for (uint32_t nPortIndex = 0; nPortIndex < config::max::OUT; nPortIndex++) {
		if (sv_PortState[nPortIndex] != PortState::TX) {
			continue;
		}

		assert(nUart < config::max::OUT);

		const auto nNext = (s_nDmxDataWriteIndex[nPortIndex] + 1) & (DMX_DATA_OUT_INDEX - 1);
		auto *p = &s_pCoherentRegion->dmx_data[nPortIndex][nNext];

		auto *p32 = reinterpret_cast<uint32_t *>(p->data);

		for (uint32_t i = 0; i < buffer::SIZE / 4; i++) {
			*p32++ = 0;
		}

		p->data[0] = dmx::START_CODE;

		s_nDmxDataWriteIndex[nPortIndex] = nNext;
	}

	DEBUG_EXIT
}

void Dmx::FullOn() {
	DEBUG_ENTRY

	for (uint32_t nPortIndex = 0; nPortIndex < config::max::OUT; nPortIndex++) {
		if (sv_PortState[nPortIndex] != PortState::TX) {
			continue;
		}

		assert(nUart < config::max::OUT);

		const auto nNext = (s_nDmxDataWriteIndex[nPortIndex] + 1) & (DMX_DATA_OUT_INDEX - 1);
		auto *p = &s_pCoherentRegion->dmx_data[nPortIndex][nNext];

		auto *p32 = reinterpret_cast<uint32_t *>(p->data);

		for (uint32_t i = 0; i < buffer::SIZE / 4; i++) {
			*p32++ = static_cast<uint32_t>(~0);
		}

		p->data[0] = dmx::START_CODE;

		s_nDmxDataWriteIndex[nPortIndex] = nNext;
	}

	DEBUG_EXIT
}

// DMX Receive

const uint8_t *Dmx::GetDmxChanged(uint32_t nPortIndex) {
	const auto *p = GetDmxAvailable(nPortIndex);

	if (p == nullptr) {
		return nullptr;
	}

	const auto *pSrc16 = reinterpret_cast<const uint16_t*>(p);
	auto *pDst16 = reinterpret_cast<uint16_t *>(&s_RxDmxPrevious[nPortIndex][0]);

	auto isChanged = false;

	for (auto i = 0; i < buffer::SIZE / 2; i++) {
		if (*pDst16 != *pSrc16) {
			*pDst16 = *pSrc16;
			isChanged = true;
		}
		pDst16++;
		pSrc16++;
	}

	return (isChanged ? p : nullptr);
}

const uint8_t* Dmx::GetDmxCurrentData(const uint32_t nPortIndex) {
	dmb();
	return const_cast<const uint8_t *>(s_aDmxData[nPortIndex][s_nDmxDataBufferIndexTail[nPortIndex]].Data);
}

const uint8_t *Dmx::GetDmxAvailable(const uint32_t nPortIndex)  {
	dmb();
	if (s_nDmxDataBufferIndexHead[nPortIndex] == s_nDmxDataBufferIndexTail[nPortIndex]) {
		return nullptr;
	} else {
		const auto *p = const_cast<const uint8_t *>(s_aDmxData[nPortIndex][s_nDmxDataBufferIndexTail[nPortIndex]].Data);
		s_nDmxDataBufferIndexTail[nPortIndex] = (s_nDmxDataBufferIndexTail[nPortIndex] + 1) & buffer::INDEX_MASK;
		return p;
	}
}

uint32_t Dmx::GetDmxUpdatesPerSecond(const uint32_t nPortIndex) {
	dmb();
	return s_nDmxUpdatesPerSecond[nPortIndex];
}

// RDM

uint32_t Dmx::RdmGetDateReceivedEnd() {
	return sv_RdmDataReceiveEnd;
}

// RDM Send

void Dmx::RdmSendRaw(const uint32_t nPortIndex, const uint8_t* pRdmData, uint32_t nLength) {
	assert(nPortIndex < config::max::OUT);
	assert(pRdmData != nullptr);
	assert(nLength != 0);

	auto *p = _port_to_uart(nPortIndex);
	assert(p != nullptr);

	while (!(p->LSR & UART_LSR_TEMT))
		;

	p->LCR = UART_LCR_8_N_2 | UART_LCR_BC;
	udelay(RDM_TRANSMIT_BREAK_TIME);

	p->LCR = UART_LCR_8_N_2;
	udelay(RDM_TRANSMIT_MAB_TIME);

	for (uint32_t i = 0; i < nLength; i++) {
		while (!(p->LSR & UART_LSR_THRE))
			;
		p->O00.THR = pRdmData[i];
	}

	while ((p->USR & UART_USR_BUSY) == UART_USR_BUSY) {
		static_cast<void>(EXT_UART->O00.RBR);
	}
}

// RDM Receive

const uint8_t *Dmx::RdmReceive(uint32_t nPortIndex) {
	assert(nPortIndex < config::max::OUT);

	dmb();
	if (s_nRdmDataWriteIndex[nPortIndex] == s_nRdmDataReadIndex[nPortIndex]) {
		return nullptr;
	} else {
		const auto *p = &s_aRdmData[nPortIndex][s_nRdmDataReadIndex[nPortIndex]].data[0];
		s_nRdmDataReadIndex[nPortIndex] = (s_nRdmDataReadIndex[nPortIndex] + 1) & RDM_DATA_BUFFER_INDEX_MASK;
		return p;
	}
}

const uint8_t *Dmx::RdmReceiveTimeOut(const uint32_t nPortIndex, uint16_t nTimeOut) {
	assert(nPortIndex < config::max::OUT);

	uint8_t *p = nullptr;
	const auto nMicros = H3_TIMER->AVS_CNT1;

	do {
		if ((p = const_cast<uint8_t*>(RdmReceive(nPortIndex))) != nullptr) {
			return p;
		}
	} while ((H3_TIMER->AVS_CNT1 - nMicros) < nTimeOut);

	return p;
}
