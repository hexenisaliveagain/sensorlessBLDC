/*
 * Kod bezsensorycznego sterownika BLDC
 * Sterowanie z wykorzystaniem wbudowanego komparatora 
 * Kod skomentowany w zgodzie z opsiem wyprowadzeń na module Arduino Nano.
 * Prędkość silnika regulowana za pomocą nastaw szerokości wypełnienia przebiegu PWM na pinach sterujących w danym kroku komutacji.
 * Maciej Gałda
 * github repo: https://github.com/hexenisaliveagain/sensorlessBLDC
 * 2022
 */
//Sekcja prekonfiguracji

#define SPEED_UP          A0          // Wejście żądania zwiększenia prękości
#define SPEED_DOWN        A1          // Wejście żądania zmniejszenia prękości
#define PWM_MAX_DUTY      255
#define PWM_MIN_DUTY      50
#define PWM_START_DUTY    100

byte bldc_step = 0, motor_speed;  //definicja zmiennych określająca krok komutacji oraz prędkości silnika
unsigned int i;
//sekcja konfiguracji Arduino Freamework
void setup() {
  DDRD  |= 0x38;           // Piny 3, 4, i 5 skonfigurowane jako wyjścia 
  PORTD  = 0x00;
  DDRB  |= 0x0E;           // Piny 9, 10, 11 skonfigurowane jako wyjścia 
  PORTB  = 0x31;
  // Źródło zegara dla Timer1 ustalone na clkI/O / 1 (bez preskalera)
  TCCR1A = 0;
  TCCR1B = 0x01;
   // Źródło zegara dla Timer2 ustalone na clkI/O / 1 (bez preskalera)
  TCCR2A = 0;
  TCCR2B = 0x01;

  ACSR   = 0x10;           // Czyszczenie flagi przerwań komparatora
  pinMode(SPEED_UP,   INPUT_PULLUP);  //Aktywacja wewnętrznych podciągnięć pinu SPEED_UP
  pinMode(SPEED_DOWN, INPUT_PULLUP);  //Aktywacja wewnętrznych podciągnięć pinu SPEED_DOWN
}
//Konfiguracja funkcji przerwania komparatora
ISR (ANALOG_COMP_vect) {
  //Pętla synchronizacji.
  //Pętla opóźnia wykonywanie kolejnych kroków komutacji w oparciu o stan wyjścia komparatora. 
  //Manipulacja warunkiem zakończenia pętli pozwala na dostrojenie odstępów czasu w jakim kolejne kroki komutacji zostaną przerpowadzone.
  for(i = 0; i < 10; i++) {
    if(bldc_step & 1){  
      if(!(ACSR & 0x20))  
      i -= 1;
    }
    else {
      if((ACSR & 0x20)) 
      i -= 1;
    }
  }
  bldc_move();  //Wykonanie kroku komutacji
  bldc_step++;  //inkrementacja kroku komutacji
  bldc_step %= 6; 
}
void bldc_move(){        // Funckja z krokami komutacji
  switch(bldc_step){
    case 0:
      AH_BL();          //Załączenie kroku komutacji gdzie dla fazy A (U) załącza się zasilanie (AH -> A is HIGH) a fazę B (V) zwiera się do masy (BL -> B is LOW)
      BEMF_C_RISING();  //Funkcja rekonfiguracji przerwań komutatora
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
//sekcja konfiguracji pętli Arduino Freamework
void loop() {   //Pętla główna
  SET_PWM_DUTY(PWM_START_DUTY);    //Początkowa wartość wypełnienia PWM sygnałów sterujących
  i = 5000;
  // Rozkręcenie silnika do momentu wyindukowania odpowiedniego napięcia na niezasilonej fazie silnika dla komparatora 
  while(i > 100) {
    delayMicroseconds(i);
    bldc_move();
    bldc_step++;
    bldc_step %= 6;
    i = i - 20;
  } 
  motor_speed = PWM_START_DUTY; //Ustalenie nastawy prędkości dla silnika
  ACSR |= 0x08;                    // Aktywacja przerwań komparatora. Od tego momentu funkcja przerwań komparatora przejmuje kontrolę nad 
  while(1) {  //motor control loop
    while(!(digitalRead(SPEED_UP)) && motor_speed < PWM_MAX_DUTY){
      motor_speed++;
      SET_PWM_DUTY(motor_speed);
      delay(100);
    }
    while(!(digitalRead(SPEED_DOWN)) && motor_speed > PWM_MIN_DUTY){
      motor_speed--;
      SET_PWM_DUTY(motor_speed);
      delay(100);
    }
  }
}
//sekcja funkcji
void BEMF_A_RISING(){       //funckja 
  ADCSRB = (0 << ACME);    // AIN1 jako wejście komparatora
  ACSR |= 0x03;            // Ustawienie przerwania od komparatora na zbocze rosnące
}
void BEMF_A_FALLING(){
  ADCSRB = (0 << ACME);    // AIN1 jako wejście komparatora
  ACSR &= ~0x01;           // Ustawienie przerwania od komparatora na zbocze opadające
}
void BEMF_B_RISING(){
  ADCSRA = (0 << ADEN);   // Wyłączenie modułu ADC
  ADCSRB = (1 << ACME);
  ADMUX = 2;              // AIN2 jako wejście komparatora
  ACSR |= 0x03;         
}
void BEMF_B_FALLING(){
  ADCSRA = (0 << ADEN);   // Wyłączenie modułu ADC
  ADCSRB = (1 << ACME);
  ADMUX = 2;              //  AIN2 jako wejście komparatora
  ACSR &= ~0x01;
}
void BEMF_C_RISING(){
  ADCSRA = (0 << ADEN);   //  Wyłączenie modułu ADC
  ADCSRB = (1 << ACME);
  ADMUX = 3;              //  AIN3 jako wejście komparatora
  ACSR |= 0x03;
}
void BEMF_C_FALLING(){
  ADCSRA = (0 << ADEN);   //  Wyłączenie modułu ADC
  ADCSRB = (1 << ACME);
  ADMUX = 3;              //  AIN3 jako wejście komparatora
  ACSR &= ~0x01;
}

void AH_BL(){
  PORTD &= ~0x28;
  PORTD |=  0x10;
  TCCR1A =  0;            // Włączenie PWM na pinie 11, pin 9 i 10 wygaszony
  TCCR2A =  0x81;         //
}
void AH_CL(){
  PORTD &= ~0x30;
  PORTD |=  0x08;
  TCCR1A =  0;            // Włączenie PWM na pinie 11, pin 9 i 10 wygaszony
  TCCR2A =  0x81;         //
}
void BH_CL(){
  PORTD &= ~0x30;
  PORTD |=  0x08;
  TCCR2A =  0;            //  Włączenie PWM na pinie 10, pin 9 i 11 wygaszony
  TCCR1A =  0x21;         //
}
void BH_AL(){
  PORTD &= ~0x18;
  PORTD |=  0x20;
  TCCR2A =  0;            // Włączenie PWM na pinie 10, pin 9 i 11 wygaszony
  TCCR1A =  0x21;         //
}
void CH_AL(){
  PORTD &= ~0x18;
  PORTD |=  0x20;
  TCCR2A =  0;            // Włączenie PWM na pinie 9, pin 10 i 11 wygaszony
  TCCR1A =  0x81;         //
}
void CH_BL(){
  PORTD &= ~0x28;
  PORTD |=  0x10;
  TCCR2A =  0;            //  Włączenie PWM na pinie 9, pin 10 i 11 wygaszony
  TCCR1A =  0x81;         //
}

void SET_PWM_DUTY(byte duty){ //funckja nastaw wypełnienia PWM
  if(duty < PWM_MIN_DUTY)
    duty  = PWM_MIN_DUTY;
  if(duty > PWM_MAX_DUTY)
    duty  = PWM_MAX_DUTY;
  OCR1A  = duty;                   // Set pin 9  PWM duty cycle
  OCR1B  = duty;                   // Set pin 10 PWM duty cycle
  OCR2A  = duty;                   // Set pin 11 PWM duty cycle
}
