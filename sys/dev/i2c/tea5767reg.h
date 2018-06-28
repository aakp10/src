#ifndef __TEA5767REG_H__
#define __TEA5767REG_H__   

/* TEA5767 ADDR */
#define TEA5767_ADDR            0x60

/* 1st byte */  
#define TEA5767_MUTE            0x80    /* Set Mute */
#define TEA5767_SEARCH          0x40    /* Activate search Mode */

/* 3rd byte */  
#define TEA5767_SUD             0x80    /* Search Up */
#define TEA5767_SSL_1           0x60    /* ADC o/p = 10 */
#define TEA5767_SSL_2           0x40    /* ADC o/p = 7 */
#define TEA5767_SSL_3           0x20    /* ADC o/p = 5 */
#define TEA5767_MONO            0x08    /* Force Mono */
#define TEA5767_MUTE_R          0x04    /* Mute Right */
#define TEA5767_MUTE_L          0x02    /* Mute Left */

/* 4th byte */  
#define TEA5767_STANDBY         0x40    
#define TEA5767_FM_BAND         0x20    /* Set Japanese FM Band */
#define TEA5767_CLK_FREQ        0x10    /* Selected: 32.768kHz Unselected: 13 MHz */    
#define TEA5767_SMUTE           0x08    
#define TEA5767_SNC             0x02    /* Stereo Noise Cancelling */
#define TEA5767_SEARCH_IND      0x01    

/* 5th byte */  
#define TEA5767_PLLREF          0x80    /* If enabled TEA5767_CLK_FREQ : 6.5MHZ*/

/* Read Mode MASKS*/

/* 1st byte */
#define TEA5767_READY_FLAG      0x80
#define TEA5767_BAND_LIMIT      0X40

/* 3rd byte */
#define TEA5767_STEREO          0x80
#define TEA5767_IF_COUNTER      0x7f

/* 4th byte */
#define TEA5767_ADC_LEVEL       0xf0

#endif  