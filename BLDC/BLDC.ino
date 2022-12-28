
#define SPEED_UP          A0          // BLDC motor speed-up button
#define SPEED_DOWN        A1          // BLDC motor speed-down button
#define PWM_MAX_DUTY      255
#define PWM_MIN_DUTY      0
#define PWM_START_DUTY    70

byte bldc_step = 0, motor_speed = 0, motor_speed_buffer;
bool motor_stop = true;
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
//Pętla synchronizacji pracy komutatora i obrotów silnika
  //Pętla opóźnia wykonywanie kolejnych kroków komutacji w oparciu o stan wyjścia komparatora
  //Manipulacja warunkiem zakończenia pętli strojącej for() pozwala na dostrojenie odstępów czasu w jakim kolejne kroki komutacji zostaną przeprowadzone
ISR (ANALOG_COMP_vect) {
  // BEMF debounce
  motor_speed_buffer = motor_speed; //skopiowanie nastawy wypełnienia PWM
  for(i = 0; i < 4; i++) {
    if(bldc_step & 1){
      if(!(ACSR & 0x20)) 
      i -= 1;
      if (motor_speed_buffer < (PWM_MAX_DUTY-10)){ //podniesienie wartości wypełnienia jeśli silnik jest dodatkowo obciążany
      motor_speed_buffer = motor_speed_buffer + 2; //wartość wypełnienia przebiegu PWM jest podniesiona o 2 
      SET_PWM_DUTY(motor_speed_buffer);             //zadanie zmodyfikowanej nastawy wypełnienia
      }
    }
    else {
      if((ACSR & 0x20)) 
      i -= 1;
      if (motor_speed_buffer < (PWM_MAX_DUTY-10)){
        motor_speed_buffer = motor_speed_buffer + 2;
      SET_PWM_DUTY(motor_speed_buffer);
      }
    }
  }
  
  bldc_move(); //Wykonanie kroku komutacji 
  bldc_step++; //inkrementacja kroku komutacji
  bldc_step %= 6; //Dzielenie modulo ograniczające maksymalny krok komutacji do 6
 
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

void loop() {
  SET_PWM_DUTY(PWM_START_DUTY);    //Początkowa wartość wypełnienia PWM sygnałów sterujących
  i = 5000;
  // Motor start
  //Rozkręcenie silnika do momentu wyindukowania odpowiedniego napięcia na niezasilonej fazie silnika dla komparatora 
  while(i > 100) {
    delayMicroseconds(i);
    bldc_move();
    bldc_step++;
    bldc_step %= 6;
    i = i - 20;
  }
  motor_speed = PWM_START_DUTY;     //Ustalenie nastawy prędkości dla silnika
  ACSR |= 0x08;                    // Aktywacja przerwań komparatora. Od tego momentu funkcja przerwań komparatora przejmuje kontrolę nad silnikiem

  while(1) {
    while(!(digitalRead(SPEED_UP)) && motor_speed < PWM_MAX_DUTY){
      motor_speed++;
      SET_PWM_DUTY(motor_speed);
      delay(100);
      Serial.print("Speed UP \n");
    }
    while(!(digitalRead(SPEED_DOWN)) && motor_speed > PWM_MIN_DUTY){
      motor_speed--;
      SET_PWM_DUTY(motor_speed);
      delay(100);
      Serial.print("Speed Down \n");
    }
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

void SET_PWM_DUTY(byte duty){ //wpisanie zadanego wypełnienia do rejestrów liczników
  if(duty < PWM_MIN_DUTY)     //Wypełnienie nie może być mniejsze od wartości minimalnej
    duty  = PWM_MIN_DUTY;
  if(duty > PWM_MAX_DUTY)     //Wypełnienie nie może być większe od wartości maksymalnej
    duty  = PWM_MAX_DUTY;
  OCR1A  = duty;                   // Set pin 9  PWM duty cycle
  OCR1B  = duty;                   // Set pin 10 PWM duty cycle
  OCR2A  = duty;                   // Set pin 11 PWM duty cycle
}
