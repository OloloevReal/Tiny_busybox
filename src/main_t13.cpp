#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

/*
board_build.f_cpu  = 9600000L
*/

/*
PB0 - GREEN LED
PB1 - BLUE LED
PB4 - RED LED

PB3 - Button_1
*/

/*
PORTB ^= _BV(PB0); togle port
*/


#define BTN1 PB3
#define mask_pwm ((uint8_t)(~(_BV(PB0) | _BV(PB1) | _BV(PB4))))

#define timeToPowerOff 35
#define delayToSwitchOff 1200
#define delayToSwitchOn 150

volatile uint16_t timer_us = 0;
volatile unsigned int timer_ms = 0;
volatile unsigned int timer_btn = 0;

enum Btn_Status
{
    BTN_UP,
    BTN_DOWN
};

enum State_Status
{
    S_Sleep,
    S_Work,
    S_WakingUp
};

Btn_Status btn_status;
State_Status main_status;

uint8_t getBrightCRT(uint8_t val)
{
    return val<2?0:((long)val * val * val + 130305) >> 16;
}
volatile uint8_t pwm_i = 0;
uint8_t pwm_red = 0;
uint8_t pwm_green = 0;
uint8_t pwm_blue = 0;


void power_off(){
    cli(); // Disable interrupts before next commands
    ADCSRA &= ~_BV(ADEN);
    wdt_disable();                       // Disable watch dog timer to save power
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set sleep mode power down
    sleep_enable();
    //sleep_bod_disable(); // Disable brown-out detector
    sei();               // Enable interrupts
    sleep_cpu();
    sleep_disable();
}

ISR(TIM0_OVF_vect){
    if(timer_us++ >= 36){
        timer_us = 0;
        timer_ms++;   
    }    

    uint8_t pb = 0x00;

    if( pwm_red != 0 && pwm_red >=  pwm_i){
        pb |= _BV(PB4);  
    }

    if(pwm_blue != 0 && pwm_blue >= pwm_i){      
        pb |= _BV(PB1);
    }

    if(pwm_green != 0 && pwm_green >= pwm_i){
        pb |= _BV(PB0);
    }

    PORTB = (PORTB & mask_pwm) | pb;
    pwm_i++;
}

ISR(PCINT0_vect){
    if(bit_is_clear(PINB, BTN1)){
        btn_status = BTN_DOWN;
        timer_btn = timer_ms;
    }else{
        btn_status = BTN_UP;
    }
}

void initTimers(){
    DDRB |= _BV(DD4) | _BV(DD0) | _BV(DD1);
    TCCR0A =  0x00; //_BV(COM0A1);//_BV(COM0A1) | _BV(COM0B1) | _BV(WGM00) | _BV(WGM01);
    TCCR0B = _BV(CS00); //| _BV(WGM02);

    TCNT0 = 0;
    //OCR0A = 255;
    TIMSK0 = _BV(TOIE0);
    
    sei();
}

void pwm_set(uint8_t &pwm_ch, uint8_t value, bool force){
    // if(force)
    //     pwm_ch = value;
    // else if ((pwm_ch >= value-2 && pwm_ch <= value + 2))
    // {
    //     pwm_ch = value;
    // }
    pwm_ch = value;
}

void pwm_red_set(uint8_t value, bool force = false){
    pwm_set(pwm_red, value, force);
}

void pwm_blue_set(uint8_t value, bool force = false){
    pwm_set(pwm_blue, value, force);
}

void pwm_green_set(uint8_t value, bool force = false){
    pwm_set(pwm_green, value, force);
}

void initBtn()
{
    PORTB |= _BV(BTN1); // Enable PULL_UP resistor
    GIMSK |= _BV(PCIE); // Enable Pin Change Interrupts
    PCMSK |= _BV(BTN1); // Use PCINTn as interrupt pin (Button I/O pin)
}

bool timeToSleep(){
    return timer_ms >= timeToPowerOff * 1000UL?true:false;
}

unsigned int getDelay(){
    if(main_status == S_Work)
        return delayToSwitchOff;
    return delayToSwitchOn;
}

bool pressedButton()
{
    if(btn_status == BTN_DOWN && (timer_ms - timer_btn) >= getDelay()){
        return true;
    }
    return false;
}

void shutdownChannel(volatile uint8_t &ch)
{
    for (uint8_t i = ch; i != 0; i--)
    {
        ch = i;
        _delay_us(800);
    }
    ch = 0;
}

int main(){
    initTimers();
    initBtn();

    pwm_red_set(0);
    pwm_blue_set(0);
    pwm_green_set(0);

    main_status = S_Work;
    btn_status = BTN_UP;
    uint8_t i = 0;
    uint8_t id = 0;

    while(1){
        if (timeToSleep() && main_status != S_WakingUp){
            main_status = S_Sleep;
        }
        if(pressedButton()){
                if(main_status == S_Work){
                    main_status = S_Sleep;
                }else{
                    main_status = S_Work;
                    timer_ms = 0;
                }
                btn_status = BTN_UP;
        }else{
            if(main_status == S_Work){          
                if(i++ == 255){
                    i = 0;
                    id++;
                }
                switch (id)
                {
                    case 0:
                        pwm_blue_set(getBrightCRT(i));
                       // // pwm_green_set(getBrightCRT(255-i));
                        pwm_green_set(getBrightCRT(128 - i/2));
                        break;  
                    case 1:
                        pwm_blue_set(getBrightCRT(255-i));
                        pwm_red_set(getBrightCRT(i));
                        break;
                    case 2:
                        pwm_red_set(getBrightCRT(255 - i));
                        pwm_green_set(getBrightCRT(i));
                        break;
                    case 3:
                        pwm_green_set(getBrightCRT(255-i));
                        pwm_blue_set(getBrightCRT(i));
                        pwm_red_set(getBrightCRT(i));
                        break;
                    case 4:
                        pwm_red_set(getBrightCRT(255 - i));
                        break;
                    case 5:
                        pwm_blue_set(getBrightCRT(255-i));
                        pwm_green_set(getBrightCRT(i/2));
                        pwm_red_set(getBrightCRT(i));
                        break;
                    case 6:
                        pwm_red_set(getBrightCRT(255 - i));
                        break;
                    default:
                    id = 0;
                }
                _delay_ms(3);
            }else if (main_status == S_Sleep){
                shutdownChannel(pwm_red);
                shutdownChannel(pwm_blue);
                shutdownChannel(pwm_green);
                _delay_ms(10);
                power_off();
                main_status = S_WakingUp;
                //id = 1;
            }
            if(main_status == S_WakingUp && btn_status == BTN_UP){
                main_status = S_Sleep;
            }
        }
    }
    return 0;
}