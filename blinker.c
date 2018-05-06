#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

int volatile timeslot = 0;

void blinker_FSM(void);
uint8_t process_alarms(void);
uint8_t power_supply(void);
uint8_t activity_on_usb(void);
void update_state(void);
void burn_leds(uint8_t mask);

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

uint8_t process_alarms(void) { return device_state.alarm; }
uint8_t power_supply(void) { return device_state.supply; }
uint8_t activity_on_usb(void) { return device_state.usb_activity; }

/* 0 means active in this time slot, 1..98 means idle in this time slot */
struct led_timeslots_counters {
    int16_t red;
    int8_t  green;
};

struct led_timeslots_counters ltsc = {
    .red    = 0,
    .green  = 0,
};


/* emulate device state changes over time */
void update_state(void)
{

    if (timeslot > 2700) {
        device_state.usb_activity = 0;
    } else if (timeslot > 2500) {
        device_state.supply = MAIN_SUPPLY;
    } else if (timeslot > 2000) {
        device_state.alarm = PENDING_ALARM;
    } else if (timeslot > 400) {
        device_state.alarm = ACTIVE_ALARM;
        device_state.usb_activity = 1;
    } else if (timeslot > 200) {
        device_state.supply = USB_SUPPLY;
    } else if (timeslot >= 0) {
        device_state.supply = MAIN_SUPPLY;
        device_state.alarm = NO_ALARM;
        device_state.usb_activity = 0;
    } 
}


int main(void)
{
    rcc_clock_setup_msi(&rcc_clock_config[RCC_CLOCK_VRANGE1_MSI_RAW_2MHZ]);
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8|GPIO7|GPIO6|GPIO5);
    

    rcc_periph_clock_enable(RCC_TIM2);
    nvic_enable_irq(NVIC_TIM2_IRQ);
    rcc_periph_reset_pulse(RST_TIM2);
    timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,
                TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_set_prescaler(TIM2, 2000);
    timer_disable_preload(TIM2);
    timer_continuous_mode(TIM2);
    timer_set_period(TIM2, 10);
    timer_set_oc_value(TIM2, TIM_OC1, 10);
    timer_enable_counter(TIM2);
    timer_enable_irq(TIM2, TIM_DIER_CC1IE);
    
 
    while(1) {
        update_state();
    }
}

void burn_leds(uint8_t mask)
{
    (mask & RED_ON) ? gpio_set(GPIOB, GPIO8) : gpio_clear(GPIOB, GPIO8);
    (mask & GREEN_ON) ? gpio_set(GPIOB, GPIO7) : gpio_clear(GPIOB, GPIO7);
    (mask & BLUE_ON) ? gpio_set(GPIOB, GPIO6) : gpio_clear(GPIOB, GPIO6);
}

void tim2_isr(void)
{

    if(timer_get_flag(TIM2, TIM_SR_CC1IF)) {
        timer_clear_flag(TIM2, TIM_SR_CC1IF);
        blinker_FSM();
        timeslot ++;
        if (timeslot > 6000) timeslot = 0;
        gpio_toggle(GPIOB, GPIO5); // timer pulses for debugging purposes
    }
}


#define INITIAL         1
#define PROCESS_ALARMS  2
#define CHECK_SUPPLY    3
#define CHECK_USB       4
#define FINISH          5

void blinker_FSM(void)
{
    uint8_t state = INITIAL;
    uint8_t leds = ALL_OFF;
    while (1) {
        switch(state) {
            case INITIAL:
                if (NO_ALARM != process_alarms()) {
                    state = PROCESS_ALARMS;
                    break;
                }

                state = CHECK_USB;
                break;

            case PROCESS_ALARMS:
                state = CHECK_USB;
                if (ltsc.red == 0) {
                    leds = RED_ON;
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
                        leds = RED_ON;
                        state = FINISH;
                    }
                }
                break;

            case CHECK_USB:
                if (activity_on_usb()) {
                    if (ltsc.green == 0) {
                        leds = GREEN_ON;
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
                    leds = BLUE_ON;
                    break;
                }

                if (ltsc.green == 0) {
                    // if green led not used for indication now
                    leds = GREEN_ON;
                }
                break;

            case FINISH:
                if (ltsc.red) ltsc.red--;
                if (ltsc.green) ltsc.green--;
                burn_leds(leds);
                return;

        }
    }
}
