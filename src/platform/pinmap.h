


#ifndef PINMAP_H
#define PINMAP_H

#define V24_TXC_DCE 0
#define V24_RXC     4
#define V24_TXC_DTE 5
#define V24_DCD     9
#define V24_DSR     10
#define V24_CTS     11
#define V24_RXD     12
#define V24_RTS     13
#define V24_TXD     14
#define V24_DTR     15

typedef enum{
    V24_TXC_DCE_T = V24_TXC_DCE,
    V24_RXC_T     = V24_RXC,
    V24_TXC_DTE_T = V24_TXC_DTE,
    V24_DCD_T     = V24_DCD,
    V24_DSR_T     = V24_DSR,
    V24_CTS_T     = V24_CTS,
    V24_RXD_T     = V24_RXD,
    V24_RTS_T     = V24_RTS,
    V24_TXD_T     = V24_TXD,
    V24_DTR_T     = V24_DTR
} V24_PIN_T;


#endif /* PINMAP_H */
