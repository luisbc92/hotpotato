
#include <msp430g2553.h>

/*
 * Definitions
 */
#define PWM_CHANNELS	3
#define PWM_RES			16	// Don't change, see set color
#define TIMER_COUNT		3

/*
 * Global Variables
 */
unsigned timer[TIMER_COUNT];
unsigned long clock;
unsigned pwmChannel[PWM_CHANNELS];
int capChannel;				// Holds current reading
int capBase;				// Initialized to an average of reading at startup
int capTreshold = 1000;		// Threshold to consider as a touch

/*
 * Colors
 */
typedef struct COLOR {
	int h;	// Hue
	int s;	// Saturation
	int b;	// Brightness
} color_t;

color_t off = {0, 0, 0};
color_t white = {0, 0, 255};
color_t red	= {0, 255, 255};
color_t yellow = {40, 255, 255};
color_t green = {80, 255, 255};
color_t blue = {160, 255, 255};
color_t orange = {20, 255, 255};
color_t lblue = {120, 255, 255};
color_t purple = {200, 255, 200};
color_t pink = {200, 255, 255};

/*
 * Hot Potato States
 */
typedef enum GAME_STATE {
	INACTIVE,
	FLYING,
	TAKEN,
	PASS,
	LOSE
} state_t;

/*
 * Function Prototypes
 */
void ledSetColor();
void capCalibrate();
int capButton();
unsigned long getRandom(unsigned long, unsigned long);


/*
 * Main Loop
 */
int main(void) {

	/*
	 * Initialization
	 */
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    // Port Configuration
    P1SEL2 = BIT7;				// Capacitive Sensing (P1.7)
    P1DIR = 0x7F;				// LEDs (All P1 - 1.7)
    P2DIR = 0xFF;				// LEDs (All P2)

    // Oscillators
    BCSCTL2 = SELM_0 | DIVM_0 | DIVS_0;	// DCOCLK, DIV by 1

    /*if (CALBC1_16MHZ != 0xFF) {
    	__delay_cycles(100000);
    	DCOCTL = 0x00;
    	BCSCTL1 = CALBC1_16MHZ;	// Set DCO to 16MHz
    	DCOCTL = CALDCO_16MHZ;
    }*/

    BCSCTL1 |= XT2OFF | DIVA_0;	// Disable XT2CLK
    BCSCTL3 = XT2S_0 | LFXT1S_2 | XCAP_1;

    // Timer 0 (Capacitive Sensing)
    TA0CTL = TASSEL_3 | ID_0 | MC_2;	// INCLK (Capacitive oscillator)

    // Timer 1 (PWM / Clock)
    TA1CCTL0 = CM_3 | CCIS_0 | OUTMOD_0 | CCIE;	// Compare mode
    TA1CCTL1 = CM_3 | CCIS_2 | CAP | OUTMOD_0;	// Capture mode
    TA1CCR0 = 1600;							// (16Mhz / 1600 = 10000Hz)
    TA1CTL = TASSEL_2 | ID_0 | MC_1;			// SMCLK, DIV1, Up Mode

    // Interrupts
    IFG1 &= ~(WDTIFG);		// Clear Watchdog Interrupt
    IE1 |= WDTIE;			// Enable Watchdog Interrupt
    __bis_SR_register(GIE);	// Enable interrupts

    // Watchdog (Capacitive Sensing)
    // Interval timer, ACLK, DIV64 (12KHz / 64 = 187.5Hz)
    WDTCTL = WDTPW | WDTTMSEL | WDTSSEL | WDTIS0 | WDTIS1;
	
    // Capacitive Sensing
    capCalibrate();

    /*
     * Infinite Loop
     */

    static state_t state = FLYING;	// Game State
	static color_t color;			// Current Color
	static unsigned long throwTime;	// Random time to throw ball

    while (1) {

    	switch (state) {
    	case FLYING:
    		// Cycle colors
    		//color.s = 255; color.b = 255;
    		color = green;
    		/*if (timer[0] > 50) {
    			color.h++;
    			if (color.h == 255) color.h = 0;
    			timer[0] = 0;
    		}*/
    		// Detect if ball is taken
    		if (capButton()) {
    			clock = 0;
    			timer[0] = 0;
    			timer[1] = 0;
    			throwTime = getRandom(10000, 40000);
    			color = yellow;
    			state = TAKEN;
    		}
    		// Detect Inactivity
    		if (clock > 200000) {
    			clock = 0;
    			state = INACTIVE;
    		}
    		break;
    	case TAKEN:
    		// Flash color
    		//color.s = 255;
    		if (timer[0] > 1500) {
    			color.b = 255 - color.b;
    			timer[0] = 0;
    		}
    		// Wait random time to pass
    		if (timer[1] > throwTime) {
    			timer[0] = 0;
    			timer[1] = 0;
    			state = PASS;
    		}
    		// If ball is thrown before time, lose
    		if (capButton() == 0 && timer[1] > 1000) {
    			timer[0] = 0;
    			timer[1] = 0;
    			state = LOSE;
    		}
    		break;
    	case PASS:
    		// Flash white
    		color.s = 0;
    		if (timer[0] > 500) {
    			color.b = 255 - color.b;
    			timer[0] = 0;
    		}
    		// 500ms to throw ball, if not player loses
    		if (timer[1] > 10000) {
    			timer[0] = 0;
    			timer[1] = 0;
    			state = LOSE;
    		}
    		// If player throws ball, reset to flying
    		if (capButton() == 0) {
    			timer[0] = 0;
    			timer[1] = 1;
    			state = FLYING;
    		}
    		break;
    	case LOSE:
    		// Flash red
    		color.h = red.h; color.s = 255;
    		if (timer[0] > 5000) {
    			color.b = 255 - color.b;
    			timer[0] = 0;
    		}
    		// Wait 5s and restart
    		if (timer[1] > 50000) {
    			timer[0] = 0;
    			timer[1] = 0;
    			state = FLYING;
    		}
    		break;
    	case INACTIVE:
			// Turn Off LED
			color = off;
			// Wake Up
			if (capButton()) {
				clock = 0;
				state = FLYING;
			}
			break;
    	}

    	ledSetColor(color);

    }

	return 0;
}

/*
 * Timer 1 Interrupt
 * 1000Hz
 *
 * PWM / Clock
 */

#pragma vector=TIMER1_A0_VECTOR
__interrupt void TIMER1_A0_ISR_HOOK(void)
{
	/*
	 * PWM
	 */
	static int pwmCycle = 0;
	static int i = 0;

	//for (i = 0; i < PWM_CHANNELS; i++) {
		// Modified to repeat signal on 4 pins
		if (pwmChannel[i] > pwmCycle) {
			P1OUT |= (1 << i) + (1 << (i+3));
			P2OUT |= (1 << i) + (1 << (i+3));
		} else {
			P1OUT &= ~((1 << i) + (1 << (i+3)));
			P2OUT &= ~((1 << i) + (1 << (i+3)));
		}
	//}

	pwmCycle++;
	if (pwmCycle == 16) {
		pwmCycle = 0;
		i++;
		if (i == 3) i = 0;
	}


	/*
	 * Clock
	 */
	clock++;

	/*
	 * Timers
	 */
	int j;
	for (j = 0; j < TIMER_COUNT; j++) {
		timer[j]++;
	}
}


/*
 * Watchdog Timer Interrupt
 * 187.5Hz
 *
 * Capacitive Sensing
 */

#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR_HOOK(void)
{
	capChannel = TA0R;		// Save current reading
	TA0CTL |= TACLR;		// Clear timer
}

/*
 * LED Functions
 */
void ledSetColor(color_t color) {
	// Set according to connected color
	int *r = &pwmChannel[0];
	int *g = &pwmChannel[1];
	int *b = &pwmChannel[2];

	// Convert HSB to RGB
	if (color.s == 0) {
		*r = *g = *b = color.b / PWM_RES;	// Does not work for PWM_RES != 16
	} else {
		unsigned scaledHue = color.h * 6;
		unsigned sector = scaledHue >> 8;
		unsigned offset = scaledHue - (sector << 8);
		unsigned p = (color.b * (255 - color.s)) >> 8;
		unsigned q = (color.b * (255 - ((color.s * offset) >> 8) )) >> 8;
		unsigned t = (color.b * (255 - ((color.s * (255 - offset)) >> 8) )) >> 8;

		switch (sector) {
		case 0:
			*r = color.b / PWM_RES;
			*g = t       / PWM_RES;
			*b = p       / PWM_RES;
			break;
		case 1:
			*r = q       / PWM_RES;
			*g = color.b / PWM_RES;
			*b = p       / PWM_RES;
			break;
		case 2:
			*r = p       / PWM_RES;
			*g = color.b / PWM_RES;
			*b = t       / PWM_RES;
			break;
		case 3:
			*r = p       / PWM_RES;
			*g = q       / PWM_RES;
			*b = color.b / PWM_RES;
			break;
		case 4:
			*r = t       / PWM_RES;
			*g = p       / PWM_RES;
			*b = color.b / PWM_RES;
			break;
		default:
			*r = color.b / PWM_RES;
			*g = p       / PWM_RES;
			*b = q       / PWM_RES;
			break;
		}
	}
}



/*
 * Capacitive Sensor
 */
void capCalibrate() {
	int i;
	capBase = 0;

	for (i = 0; i < 64; i++) {
		capBase += capChannel / 64;
		__delay_cycles(32000);
	}
}

int capButton() {
	if (capChannel < capBase - capTreshold) {
		return 1;
	} else {
		return 0;
	}
}


/*
 * Random Number
 */
unsigned long getRandom(unsigned long low, unsigned long high) {
	static unsigned long a = 543;
	static unsigned long b = 234;
	static unsigned long c = 982;
	static unsigned long x = 2;

	x++;
	a = (a^c^x);
	b = (b+a);
	c = (c+(b>>1)^a);

	return (c%(high-low)) + low;
}




