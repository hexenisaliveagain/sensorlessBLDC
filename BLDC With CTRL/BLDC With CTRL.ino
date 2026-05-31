#define SPEED_UP          A0          // BLDC motor speed-up button
#define SPEED_DOWN        A1          // BLDC motor speed-down button
#define PWM_MAX_DUTY      255
#define PWM_MIN_DUTY      30
#define PWM_START_DUTY    58
#define PWM_INIT_SPEED    30
#define PWM_SAFE_MARGIN   10
#define START_DELAY_US    5000
#define START_END_DELAY_US 180
#define START_RAMP_STEP_US 40
#define BEMF_DEBOUNCE_SAMPLES 6
#define BEMF_BLANKING_US  80UL
#define STALL_TIMEOUT_US  120000UL
#define MAX_SYNC_RECOVERY 3
#define PI_CONTROL_PERIOD_US 5000UL
#define PI_KP_NUM         1L
#define PI_KI_NUM         1L
#define PI_DEN            32L

int bldc_step = 0;
volatile byte motor_speed = PWM_INIT_SPEED;
byte target_pwm = PWM_INIT_SPEED;
int speed;
float speed_f;
bool motor_stop = true;
bool cw = true;
volatile unsigned long last_bemf_us = 0;
volatile unsigned long bemf_period_us = 0;
volatile unsigned long last_commutation_us = 0;
volatile bool motor_synchronized = false;
byte sync_recovery_count = 0;
long pi_integral = 0;
unsigned long target_period_us = 0;
unsigned long last_pi_update_us = 0;
unsigned int i;

void setup() {
  DDRD  |= 0x38;          // Piny 3, 4, i 5 skonfigurowane jako wyjścia
  PORTD  = 0x00;
  DDRB  |= 0x0E;          // Piny 9, 10, 11 skonfigurowane jako wyjścia
  PORTB  = 0x31;
  // Źródło zegara dla Timer1 ustalone na clkI/O / 1 (bez preskalera)
  TCCR1A = 0;
  TCCR1B = 0x01;
  // Źródło zegara dla Timer2 ustalone na clkI/O / 1 (bez preskalera)
  TCCR2A = 0;
  TCCR2B = 0x01;
  //Komparator analogowy
  ACSR   = 0x10;           // Czyszczenie flagi przerwań komparatora
  pinMode(SPEED_UP,   INPUT_PULLUP); //Aktywacja wewnętrznych podciągnięć pinu SPEED_UP
  pinMode(SPEED_DOWN, INPUT_PULLUP); //Aktywacja wewnętrznych podciągnięć pinu SPEED_DOWN
  Serial.begin(9600);
}

// Przerwanie komparatora reaguje na przejście BEMF przez zero. Kod ignoruje
// szpilki tuż po komutacji (blanking), filtruje stan komparatora i wykonuje
// komutację po opóźnieniu ~30 stopni elektrycznych wyliczonym z poprzedniego
// okresu BEMF. Dzięki temu moment komutacji podąża za rzeczywistym wirnikiem,
// a nie za stałą pętlą opóźniającą.
ISR (ANALOG_COMP_vect) {
  unsigned long now_us = micros();

  if ((now_us - last_commutation_us) < BEMF_BLANKING_US) {
    ACSR |= (1 << ACI);
    return;
  }

  if (!bemf_is_stable()) {
    ACSR |= (1 << ACI);
    return;
  }

  if (last_bemf_us != 0) {
    bemf_period_us = now_us - last_bemf_us;
    unsigned int commutation_delay_us = constrain(bemf_period_us / 2, 20UL, 3000UL);
    delayMicroseconds(commutation_delay_us);
  }

  last_bemf_us = now_us;
  motor_synchronized = true;

  if (cw == true){
    bldc_move();
  }
  else {
    bldc_move_reverse(); //Wykonanie kroku komutacji
  }

  last_commutation_us = micros();
  bldc_step++; //inkrementacja kroku komutacji
  bldc_step %= 6; //Dzielenie modulo ograniczające maksymalny krok komutacji do 6
}

bool bemf_is_stable() {
  byte stable_samples = 0;
  bool expected_high = cw ? !(bldc_step & 1) : (bldc_step & 1);

  for (byte sample = 0; sample < BEMF_DEBOUNCE_SAMPLES; sample++) {
    bool comparator_high = ACSR & (1 << ACO);
    if (comparator_high == expected_high) {
      stable_samples++;
    }
    delayMicroseconds(4);
  }

  return stable_samples >= (BEMF_DEBOUNCE_SAMPLES - 1);
}

void bldc_move(){        // BLDC motor commutation function
  switch(bldc_step){
    case 0:
      AH_BL();
      BEMF_C_RISING();
      break;
    case 1:
      AH_CL();
      BEMF_B_FALLING();
      break;
    case 2:
      BH_CL();
      BEMF_A_RISING();
      break;
    case 3:
      BH_AL();
      BEMF_C_FALLING();
      break;
    case 4:
      CH_AL();
      BEMF_B_RISING();
      break;
    case 5:
      CH_BL();
      BEMF_A_FALLING();
      break;
  }
}
void bldc_move_reverse(){        // BLDC motor commutation function
  switch(bldc_step){
    case 5:
      AH_BL();
      BEMF_C_FALLING();
      break;
    case 4:
      AH_CL();
      BEMF_B_RISING();
      break;
    case 3:
      BH_CL();
      BEMF_A_FALLING();
      break;
    case 2:
      BH_AL();
      BEMF_C_RISING();
      break;
    case 1:
      CH_AL();
      BEMF_B_FALLING();
      break;
    case 0:
      CH_BL();
      BEMF_A_RISING();
      break;
  }
}
void loop() {
  handle_serial_commands();

  if (!motor_stop) {
    update_speed_regulator();
    check_sync_timeout();
  }
}

void handle_serial_commands() {
  if (!Serial.available()) {
    return;
  }

  char c = Serial.read();
  switch (c)
  {
    case 'r':
      if (motor_stop == true){
        SET_PWM_DUTY(PWM_START_DUTY);    //Początkowa wartość wypełnienia PWM sygnałów sterujących
        motor_launch();
        enable_bemf_interrupt();
        SET_PWM_DUTY(motor_speed);
        Serial.println("Motor Starts");
        motor_stop = false;
      }
      else {
        Serial.println("Motor Already Runs!!");
      }
      break;
    case 's':
      ACSR &= ~(1 << ACIE);
      stop();
      motor_stop = true;
      Serial.println("Motor Stopped");
      break;
    case 'd':
      Serial.print("PWM: ");
      Serial.println(motor_speed);
      Serial.print("BEMF period us: ");
      Serial.println(bemf_period_us);
      break;
    case 'x':
      speed = Serial.parseInt();
      if (speed > 10 && speed < 101) {
        speed_f = speed * 2.54;
        target_pwm = constrain((int)speed_f, PWM_MIN_DUTY, PWM_MAX_DUTY - PWM_SAFE_MARGIN);
        Serial.println(speed_f);
        Serial.println(speed);
      }
      break;
    case 'k':
      if (motor_stop) {
        cw = true;
      }
      Serial.println("Direction CW");
      break;
    case 'l':
      if (motor_stop) {
        cw = false;
      }
      Serial.println("Direction CCW");
      break;
  }
}

void motor_launch(){
  byte motor_speed_buffer2 = PWM_START_DUTY;
  i = START_DELAY_US;
  // Motor start
  bldc_step = 0;
  //Rozkręcenie silnika do momentu wyindukowania odpowiedniego napięcia na niezasilonej fazie silnika dla komparatora
  while(i > START_END_DELAY_US) {
    delayMicroseconds(i);
    if (cw == true) {
      bldc_move();
    }
    else {
      bldc_move_reverse();
    }

    bldc_step++;
    bldc_step %= 6;
    i = i - START_RAMP_STEP_US;
    if (i < 3000 && motor_speed_buffer2 < 90){
      motor_speed_buffer2 = motor_speed_buffer2 + 2;
      SET_PWM_DUTY(motor_speed_buffer2);
    }
  }

  motor_speed = max(target_pwm, (byte)PWM_START_DUTY);
  target_period_us = i * 2UL;
  last_bemf_us = micros();
  last_commutation_us = last_bemf_us;
  bemf_period_us = target_period_us;
  pi_integral = 0;
  motor_synchronized = true;
  sync_recovery_count = 0;
}

void enable_bemf_interrupt() {
  ACSR |= (1 << ACI);     // skasowanie flagi przerwania komparatora
  ACSR |= (1 << ACIE);    // od tego momentu przerwanie komparatora przejmuje komutację
}

void update_speed_regulator() {
  unsigned long now_us = micros();
  if ((now_us - last_pi_update_us) < PI_CONTROL_PERIOD_US || bemf_period_us == 0) {
    return;
  }
  last_pi_update_us = now_us;

  target_period_us = pwm_to_target_period(target_pwm);
  long error = (long)bemf_period_us - (long)target_period_us; // dodatni błąd = silnik zwolnił
  pi_integral = constrain(pi_integral + error, -12000L, 12000L);
  long correction = ((error * PI_KP_NUM) + (pi_integral * PI_KI_NUM)) / PI_DEN;
  int regulated_pwm = (int)target_pwm + (int)correction;

  motor_speed = constrain(regulated_pwm, PWM_MIN_DUTY, PWM_MAX_DUTY - PWM_SAFE_MARGIN);
  SET_PWM_DUTY(motor_speed);
}

unsigned long pwm_to_target_period(byte pwm) {
  // Przybliżona mapa bez dodatkowego czujnika prędkości: większe żądane PWM
  // oznacza krótszy docelowy okres między kolejnymi przejściami BEMF.
  return map(pwm, PWM_MIN_DUTY, PWM_MAX_DUTY - PWM_SAFE_MARGIN, 7000UL, 500UL);
}

void check_sync_timeout() {
  unsigned long now_us = micros();
  unsigned long timeout_us = max(STALL_TIMEOUT_US, bemf_period_us * 4UL);

  if (!motor_synchronized || ((now_us - last_bemf_us) <= timeout_us)) {
    return;
  }

  ACSR &= ~(1 << ACIE);
  motor_synchronized = false;
  sync_recovery_count++;

  if (sync_recovery_count <= MAX_SYNC_RECOVERY) {
    byte recovery_pwm = constrain(target_pwm + 15, PWM_START_DUTY, PWM_MAX_DUTY - PWM_SAFE_MARGIN);
    SET_PWM_DUTY(recovery_pwm);
    motor_launch();
    enable_bemf_interrupt();
    Serial.println("Sync recovery");
  }
  else {
    stop();
    motor_stop = true;
    Serial.println("Sync lost - stopped");
  }
}

void BEMF_A_RISING(){       //funkcja rekonfigurująca komparator
  ADCSRB = (0 << ACME);    // AIN1 jako wejście komparatora
  ACSR |= 0x03;            // Ustawienie przerwania od na zbocze rosnące na wejściu komparatora
}
void BEMF_A_FALLING(){
  ADCSRB = (0 << ACME);    // Select AIN1 as comparator negative input
  ACSR &= ~0x01;           // Set interrupt on falling edge
}
void BEMF_B_RISING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 2;              // Select analog channel 2 as comparator negative input
  ACSR |= 0x03;
}
void BEMF_B_FALLING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 2;              // Select analog channel 2 as comparator negative input
  ACSR &= ~0x01;
}
void BEMF_C_RISING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 3;              // Select analog channel 3 as comparator negative input
  ACSR |= 0x03;
}
void BEMF_C_FALLING(){
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 3;              // Select analog channel 3 as comparator negative input
  ACSR &= ~0x01;
}

void AH_BL(){
  PORTD &= ~0x28;         //Wyłączenie wyjść 9 i 10
  PORTD |=  0x10;         //Włączenie pinu 11 jako wyjście
  TCCR1A =  0;            //Włączenie PWM na pinie 11
  TCCR2A =  0x81;         //
}
void AH_CL(){
  PORTD &= ~0x30;
  PORTD |=  0x08;
  TCCR1A =  0;            // Turn pin 11 (OC2A) PWM ON (pin 9 & pin 10 OFF)
  TCCR2A =  0x81;         //
}
void BH_CL(){
  PORTD &= ~0x30;
  PORTD |=  0x08;
  TCCR2A =  0;            // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
  TCCR1A =  0x21;         //
}
void BH_AL(){
  PORTD &= ~0x18;
  PORTD |=  0x20;
  TCCR2A =  0;            // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
  TCCR1A =  0x21;         //
}
void CH_AL(){
  PORTD &= ~0x18;
  PORTD |=  0x20;
  TCCR2A =  0;            // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
  TCCR1A =  0x81;         //
}
void CH_BL(){
  PORTD &= ~0x28;
  PORTD |=  0x10;
  TCCR2A =  0;            // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
  TCCR1A =  0x81;         //
}
void stop(){
  SET_PWM_DUTY(0);
  PORTD  =  0x00;
  TCCR2A =  0;            // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
  TCCR1A =  0;         //
  motor_synchronized = false;
}

void SET_PWM_DUTY(byte duty){ //wpisanie zadanego wypełnienia do rejestrów liczników
  if(duty < PWM_MIN_DUTY)     //Wypełnienie nie może być mniejsze od wartości minimalnej
    duty  = PWM_MIN_DUTY;
  if(duty > PWM_MAX_DUTY)     //Wypełnienie nie może być większe od wartości maksymalnej
    duty  = PWM_MAX_DUTY;
  OCR1A  = duty;                   // Set pin 9  PWM duty cycle
  OCR1B  = duty;                   // Set pin 10 PWM duty cycle
  OCR2A  = duty;                   // Set pin 11 PWM duty cycle
}
