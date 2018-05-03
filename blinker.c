#ifdef X86_DEBUG
#include <stdio.h>
#include <stdint.h>
#endif

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>


uint8_t leds_mask = 0;
int timeslot = 0;

void blinker_FSM();


struct state {
    uint8_t alarm;
    uint8_t supply;
    uint8_t usb_activity;
};

#define NO_ALARM        0x00
#define ACTIVE_ALARM    0x01
#define PENDING_ALARM   0x02

#define MAIN_SUPPLY     0x00
#define USB_SUPPLY      0x01

#define ALL_OFF         0x00
#define RED_ON          0x01
#define GREEN_ON        0x02
#define BLUE_ON         0x04

struct state device_state = {
    .alarm = NO_ALARM,
    .supply = MAIN_SUPPLY,
    .usb_activity = 0,
};

uint8_t process_alarms()
{
    return device_state.alarm; 
};

uint8_t power_supply() { return device_state.supply; };
uint8_t activity_on_usb() { return device_state.usb_activity; };

/* 0 means active in this timeslot, 1..98 means idle in this timeslot */
struct led_timeslots_counters {
    int16_t red;
    int8_t green;
};

struct led_timeslots_counters ltsc = {
    .red = 0,
    .green = 0,
};

void dump_ltsc()
{
#ifdef X86_DEBUG
    printf("ltsc: %04d %03d ", ltsc.red, ltsc.green);
#endif
}

void dump_leds()
{
#ifdef X86_DEBUG
    if (leds_mask & RED_ON) printf("R");
    if (leds_mask & GREEN_ON) printf("G");
    if (leds_mask & BLUE_ON) printf("B");

    printf("\n");
#endif
}

void update_state()
{
    if (timeslot < 100) {
        device_state.supply = USB_SUPPLY;
    } else if (timeslot < 150) {
        device_state.alarm = ACTIVE_ALARM;
    } else if (timeslot < 200) {
        device_state.alarm = PENDING_ALARM;
        device_state.usb_activity = 1;
    } else if (timeslot < 250) {
        device_state.usb_activity = 0;
    } else if (timeslot < 500) {
        device_state.supply = MAIN_SUPPLY;
    } else if (timeslot < 501) {
        device_state.alarm = ACTIVE_ALARM;
    }
}


int main()
{
    int i = 0;
    rcc_clock_setup_pll(&rcc_clock_config[RCC_CLOCK_VRANGE1_HSI_RAW_16MHZ]);
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7|GPIO8);

    rcc_periph_clock_enable(RCC_TIM2);
    nvic_enable_irq(NVIC_TIM2_IRQ);
    rcc_periph_reset_pulse(RST_TIM2);
    timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,
                TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(TIM2, ((rcc_apb1_frequency * 2) / 5000));
    timer_disable_preload(TIM2);
    timer_continuous_mode(TIM2);
    timer_set_period(TIM2, 65535);
    timer_set_oc_value(TIM2, TIM_OC1, 500);
    timer_enable_counter(TIM2);
    timer_enable_irq(TIM2, TIM_DIER_CC1IE);



    
    while(1) {
        gpio_toggle(GPIOB, GPIO7);
        for(i = 0; i < 1000000; i++) {
            __asm__("nop");
        }
    }


    for (timeslot = 0; timeslot < 2000; timeslot++) {
#ifdef X86_DEBUG
        printf("timeslot: %04d ", timeslot);
#endif
        blinker_FSM();
        dump_leds();
        update_state();
    }
}

void burn_leds(uint8_t mask)
{
    leds_mask = mask;
}

void tim2_isr(void)
{
    if(timer_get_flag(TIM2, TIM_SR_CC1IF)) {
        timer_clear_flag(TIM2, TIM_SR_CC1IF);
        gpio_toggle(GPIOB, GPIO8);
    }
    __asm__("nop");
}


#define INITIAL         1
#define PROCESS_ALARMS  2
#define CHECK_SUPPLY    3
#define CHECK_USB       4
#define FINISH          5

void blinker_FSM()
{
    uint8_t state = INITIAL;
    while (1) {
        switch(state) {
            case INITIAL:
                burn_leds(ALL_OFF); // clear lighting leds mask
                if (NO_ALARM != process_alarms()) {
                    state = PROCESS_ALARMS;
                    break;
                }

                state = CHECK_USB;
                break;

            case PROCESS_ALARMS:
                state = CHECK_USB;
                if (ltsc.red == 0) {
                    burn_leds(RED_ON);
                    if (ACTIVE_ALARM == process_alarms()) {
                        ltsc.red = 100;
                    } else {
                        ltsc.red = 1000;
                    }
                    state = FINISH;
                } else if (ltsc.red > 100) {
                    /* special case: we already shown pending alarm but got active alarm */
                    if (ACTIVE_ALARM == process_alarms()) {
                        ltsc.red = 100;
                        burn_leds(RED_ON);
                        state = FINISH;
                    }
                }
                break;

            case CHECK_USB:
                if (activity_on_usb()) {
                    if (ltsc.green == 0) {
                        burn_leds(GREEN_ON);
                        ltsc.green = 98;
                        state = FINISH;
                        break;
                    }
                }

                state = CHECK_SUPPLY;
                break;

            case CHECK_SUPPLY:
                state = FINISH;
                if (MAIN_SUPPLY == power_supply()) {
                    burn_leds(BLUE_ON);
                    break;
                }

                if (ltsc.green == 0) {
                    // if green led not used for indication now
                    burn_leds(GREEN_ON);
                }
                break;

            case FINISH:
                if (ltsc.red) ltsc.red--;
                if (ltsc.green) ltsc.green--;
                dump_ltsc();
                return;

        }
    }
}
