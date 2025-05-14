#include <libpynq.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#define MAX_SIZE 1000

int BEATLED = 0;
int graphPosition = 0;

const int calibrateThresholdL = BUTTON0;
const int calibrateThresholdH = BUTTON1;
const int reset_button = BUTTON2;
const int exit_button = BUTTON3; 

typedef struct frequency2_t{
  float sample;
  int *schmittTriggerState;
  float lowerThreshold;
  float upperThreshold;
  int *previousTime;
  adc_channel_t pin;
  float *frequency;
} frequency_t;

int millis(){
  //"time_t" is a 32 bit int just like "int"
  return clock()*1000 / CLOCKS_PER_SEC;
}

int calculateFrequency(int timeDifference){
  if (timeDifference == 0){
    return 0;
  }
  return 60000 / timeDifference;
}

float smoothener(float change, float old, float factor){
  return change*factor+old*(1-factor);
}

float average2(float v1, float v2){
  return 0.5*(v1+v2);
}

// void customWait(){
//   for (size_t i = 0; i < 5000000; i++){
//     // frequencyDetection(float sample, int *schmittTriggerState, float lowerThreshold, float upperThreshold, int *previousTime);
//   }
// }

// void waitUntilASecondPassed(unsigned int *timeValue){
//     while(millis()-(*timeValue)<1000){
//         // printf("waiting %d...\n", millis()-(*timeValue));
//       // frequencyDetection(float sample, int *schmittTriggerState, float lowerThreshold, float upperThreshold, int *previousTime);
//     }
//     *timeValue=millis();
// }

void plot(display_t *display, uint16_t x, uint16_t y, uint16_t height, uint16_t top, uint16_t bottom, uint16_t color){
  uint16_t pointTop    = fmax(top   , top+y-0.5*height);
  uint16_t pointBottom = fmin(bottom, top+y+0.5*height);
  if (pointTop >= pointBottom) return;
  // displayDrawPixel ( display, x, y, color );
  displayDrawFillRect ( display, x, pointBottom, x, pointTop, color );
}

void sensorGraph( display_t *display, int *x, float measurement, float slope, float threshold ){
  uint16_t top = 32;
  uint16_t bottom = DISPLAY_HEIGHT-1;
  uint16_t range = bottom - top;
  displayDrawFillRect ( display, *x, top, *x, bottom, RGB_BLACK );
  float slopeFactor = range/10;
  float measurementFactor = range/3.3;
  plot ( display, *x, threshold   * slopeFactor      , 4, top, bottom, RGB_WHITE );
  plot ( display, *x, measurement * measurementFactor, 4, top, bottom, RGB_RED );
  plot ( display, *x, slope       * slopeFactor      , 4, top, bottom, RGB_BLUE );
  (*x)++;
  if((*x)>=DISPLAY_HEIGHT) (*x)=0;
}

float addToAverage( float value, int *count, float *average ){
  if (*count<0) return *average;
  *average += (value-(*average)) / (*count);
  // *average += (value-(*average)) / ((*count)++);
  return *average;
}

float getAverageMeasurement(){
  //smoothen()
  //average()
  return 0;
}

float getSlope(){
  return 0;
}

void newMeasurement(){
}

void updateDisplay(float frequency, float threshold, display_t *display, FontxFile *fx){
  char string[MAX_SIZE];
  uint8_t byte[] = "";
    sprintf(string, "%.0fBPM\n%.0000f\n", frequency, threshold);
    for (int i = 0; i < 20; i++)
    {
      byte[i] = (uint8_t)string[i];
    }
    displayFillScreen(display, RGB_BLACK);
    displayDrawString(display, fx, 0, 31, byte, RGB_WHITE);
}

// float amountOfChange(float value, float *previousValue){
//   float difference = value-(*previousValue);
//   (*previousValue) = value;
//   return difference;
// }

int amountOfChangeInt(int value, int *previousValue){
  int difference = value-(*previousValue);
  (*previousValue) = value;
  return difference;
}

int softwareSchmittTrigger(float sample, int *schmittTriggerState, float lowerThreshold, float upperThreshold){
  // printf("%f %f %f %d\n", lowerThreshold, sample, upperThreshold, *schmittTriggerState);
  if(*schmittTriggerState==1 && sample<lowerThreshold){
    *schmittTriggerState = 0;
    printf("trigger off\n");
    return 0;
  }else if (*schmittTriggerState==0 && sample>upperThreshold){
    *schmittTriggerState = 1;
    printf("trigger on\n");
    return 1;
  }
  return 0;
}

void frequencyDetection(frequency_t frequencyValue, display_t *display){
  // return;
  // float sample = frequencyValue.sample;
  int *schmittTriggerState = frequencyValue.schmittTriggerState;
  float lowerThreshold = frequencyValue.lowerThreshold;
  float upperThreshold = frequencyValue.upperThreshold;
  int *previousTime = frequencyValue.previousTime;
  adc_channel_t pin = frequencyValue.pin;
  //float sample, int *schmittTriggerState, float lowerThreshold, float upperThreshold, int *previousTime
  if ((softwareSchmittTrigger(adc_read_channel(pin), schmittTriggerState, lowerThreshold, upperThreshold)||0)&&1){
    printf("PULSE!\n");
    green_led_onoff(2,BEATLED);
    BEATLED = !BEATLED;
    int timeDifference = amountOfChangeInt(millis(), previousTime);
    float frequency = calculateFrequency(timeDifference);
    // printf("%f\n", frequency);
    // printf("times %d %d difference %d frequency %f\n", millis(), *previousTime, timeDifference, frequency);
    *frequencyValue.schmittTriggerState = *schmittTriggerState;
    *frequencyValue.previousTime        = *previousTime;
    *frequencyValue.frequency = smoothener(frequency, *frequencyValue.frequency, 0.5);
    // *frequencyValue.frequency = frequency;
    // *frequencyValue.frequency = 0.0;
  }
  sensorGraph( display, &graphPosition, 0, 1, 2 );
}

void waitUntilSomeTimePassed(unsigned int *timeValue, unsigned int duration, frequency_t frequencyValue, display_t *display){
// void waitUntilSomeTimePassed(unsigned int *timeValue, unsigned int duration){
  //duration is in milliseconds
    while(millis()-(*timeValue)<duration){
        // printf("waiting %d...\n", millis()-(*timeValue));
      frequencyDetection(frequencyValue, display);
      // printf("%f",frequencyValue.lowerThreshold);
      if (get_button_state(exit_button) || get_button_state(reset_button)) break;
    }
    *timeValue=millis();
}

void waitSomeTime(unsigned int duration, frequency_t frequencyValue, display_t *display){
  unsigned int timeValue = millis();
  waitUntilSomeTimePassed(&timeValue, duration, frequencyValue, display);
}

float float_max(float a, float b){
  if (a>=b)
    return a;
  return b;
}

void sendSignal(unsigned int *timeValue, frequency_t frequencyValue, float threshold, display_t *display, FontxFile *fx){
  waitUntilSomeTimePassed(timeValue, 1000, frequencyValue, display);
  if (get_button_state(exit_button) || get_button_state(reset_button)) return;
  updateDisplay(*(frequencyValue.frequency), threshold, display, fx);
  char string[MAX_SIZE];
  uint8_t byte[] = "";
  sprintf(string, "%.0fB", float_max(240,*(frequencyValue.frequency)));//fmax(240, )
  for (int i = 0; i < 20; i++)
  {
    byte[i] = (uint8_t)string[i];
  }
  //should probably use strcpy();

  // (char *)byte
  
  green_led_on(3);
  printf("sending...\n");
  for (int i=0; byte[i]!='\0'; i++)
  {
    uart_send(UART0, byte[i]);
    // sleep_msec(100);
    // customWait();
    waitSomeTime(100, frequencyValue, display);
    if (byte[i]=='\0'){
      printf("\\0");
    }else{
      printf("%c", byte[i]);
    }
  }
  printf("\n");
  green_led_off(3);

  // for (int i = 0; byte[i] != '\0'; i++)
  // {
  //   uart_send(UART0, byte[i]);
  //   sleep_msec(100);
  //   printf("sent byte %c\n", byte[i]);
  // }
}

void calibrate2(float *lowerThreshold, float *upperThreshold, const int calibrationButton, adc_channel_t pin){
  wait_until_button_pushed(calibrationButton);
  printf("calibrating...\n");
  float sample = 0;
  float lowestValue = 0;
  float highestValue = 0;
  float averageValue = 0;
  while(get_button_state(calibrationButton)){
    sample = adc_read_channel(pin);
    lowestValue=fmin(sample, lowestValue);
    highestValue=fmax(sample, highestValue);
    averageValue=smoothener(sample, averageValue, 0.1);
    // printf("sample %f highestValue %f lowestValue %f averageValue %f\n", sample, highestValue, lowestValue, averageValue);
  }
  // if (upperMode) return average2(highestValue, averageValue);
  printf("highestValue %f lowestValue %f averageValue %f\n", highestValue, lowestValue, averageValue);
  *upperThreshold = averageValue;
  *lowerThreshold = highestValue;
}

float calibrate(const int calibrationButton, adc_channel_t pin, int upperMode){
  wait_until_button_pushed(calibrationButton);
  printf("calibrating...\n");
  float sample = 0;
  float lowestValue = 10;
  float highestValue = 0;
  float averageValue = 0;
  while(get_button_state(calibrationButton)){
    sample = adc_read_channel(pin);
    lowestValue=smoothener(fmin(sample, lowestValue), lowestValue, 0.1);
    highestValue=smoothener(fmax(sample, highestValue), highestValue, 0.1);
    averageValue=smoothener(sample, averageValue, 0.1);
    // printf("sample %f highestValue %f lowestValue %f averageValue %f\n", sample, highestValue, lowestValue, averageValue);
    // printf("sample %f\n", sample);
  }
  // if (upperMode) return average2(highestValue, averageValue);
  printf("highestValue %f lowestValue %f averageValue %f\n", highestValue, lowestValue, averageValue);
  // if (upperMode) return averageValue;
  if (upperMode) return lowestValue;
  return highestValue;
}

void displayDrawStringReally(display_t *display, FontxFile *fx, uint16_t x, uint16_t y, char *string, uint16_t color ){
  uint8_t asciii[strlen(string)];
  for(size_t i=0; i<strlen(string); i++){
    asciii[i]=string[i];
    // string[i]='\0';
  }
  // string[1]='\0';
  displayDrawString(display, fx, x, y, asciii, color);
}

void calibrateThresholds(display_t *display, FontxFile *fx, float *lowerThreshold, float *upperThreshold, adc_channel_t pin, const int calibrateThresholdL, const int calibrateThresholdH){
  // printf("press button 0 to start smart calibration...\n");
  // calibrate2(lowerThreshold, upperThreshold, calibrateThresholdL, pin);
  displayFillScreen(display, RGB_BLACK);
  for(int i=0; i<3; i++)
    green_led_off(i);
  printf("press button 0 to start neutral calibration...\n");
  green_led_on(0);
  char string[14]="neutral    ";
  displayDrawStringReally(display, fx, 0,  31, string, RGB_WHITE);
  strcpy(string,"calibration  ");
  displayDrawStringReally(display, fx, 0, 63, string, RGB_WHITE);
  strcpy(string,"press button0");
  displayDrawStringReally(display, fx, 0,  95, string, RGB_WHITE);
  // green_led_on(1);
  *lowerThreshold= calibrate(calibrateThresholdL, pin, 0);
  green_led_off(0);

  green_led_on(1);
  printf("press button 1 to start pulse calibration...\n");
  displayFillScreen(display, RGB_BLACK);
  // char string[14]="pulse      ";
  strcpy(string,"pulse        ");
  displayDrawStringReally(display, fx, 0,  31, string, RGB_WHITE);
  strcpy(string,"calibration  ");
  displayDrawStringReally(display, fx, 0, 63, string, RGB_WHITE);
  strcpy(string,"press button1");
  displayDrawStringReally(display, fx, 0,  95, string, RGB_WHITE);
  *upperThreshold= calibrate(calibrateThresholdH, pin, 1);
  if(calibrateThresholdH){}//unused parameters error is gone
  printf("done calibrating: lower: %f, upper: %f\n", *lowerThreshold, *upperThreshold);
  green_led_off(1);
}

void valueTransmission(float threshold, display_t *display, FontxFile *fx, unsigned int *timeValue, frequency_t frequencyValue){
  sendSignal(timeValue, frequencyValue, threshold, display, fx);
}

// void scheduler(float frequency, float threshold, display_t *display, FontxFile *fx, unsigned int *timeValue, frequency_t frequencyValue){
//   valueTransmission(frequency, threshold, display, fx, timeValue, frequencyValue);
//   // waitUntilASecondPassed()
//   // frequencyDetection();
// }

 

int main(void)
{
  pynq_init();

  int schmittTriggerState = 0;
  float lowerThreshold, upperThreshold;
  unsigned int timeValue = 0;

  // switchbox_set_pin(IO_PMODA1, SWB_UART0_TX);
  switchbox_set_pin(IO_AR13, SWB_UART0_TX);
  uart_init(UART0);
  uart_reset_fifos(UART0);
  display_t display;
  display_init(&display);
  displayFillScreen(&display, RGB_BLACK);
  FontxFile fx[2];
  // InitFontx(fx, "./ILMH32XB.FNT", "./ILMH32XB.FNT");
  
  InitFontx(fx, "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILMH32XB.FNT", "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILMH32XB.FNT");
  
  // InitFontx(fx16G, "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILGH16XB.FNT", "");

  // gpio_reset();
  leds_init_onoff();
  adc_init();
  buttons_init();
  
  adc_channel_t pin = ADC0;
  // float threshold = 0.007*10000000;
  float threshold = 0.1;
  // float thresholdTrim = 0;
  // float previousSample = adc_read_channel(pin);
  int previousTime = millis();
  // const int resetThreshold     = BUTTON1;
  // const int thresholdTrimUp    = BUTTON2;
  // const int thresholdTrimDown  = BUTTON3;
  float frequency = 0;


  frequency_t frequencyValue;
  frequencyValue.sample              = 0;
  frequencyValue.schmittTriggerState = &schmittTriggerState;
  frequencyValue.previousTime        = &previousTime;
  frequencyValue.pin                 = pin;
  frequencyValue.frequency           = &frequency;

  
  

  int first = 1;

  do{

    if (first || get_button_state(reset_button)){
      calibrateThresholds(&display, fx, &lowerThreshold, &upperThreshold, pin, calibrateThresholdL, calibrateThresholdH);
      frequencyValue.lowerThreshold      = lowerThreshold;
      frequencyValue.upperThreshold      = upperThreshold;
      first=0;
    }
    // float difference = amountOfChange(adc_read_channel(pin), &previousSample);
    // printf("%f\n", difference);
    // printf("%d\n", millis());
    // for (size_t i = 0; i < 5000000; i++){}
    // sleep_msec(50);

    // if(get_button_state(calibrateThreshold)){
    //   threshold = fmax(difference, threshold);
    //   updateDisplay(frequency, threshold, &display, fx);
    // }
    // if(get_button_state(resetThreshold)){
    //   threshold = 0;
    //   updateDisplay(frequency, threshold, &display, fx);
    // }
    // if(get_button_state(thresholdTrimUp)){
    //   thresholdTrim += 0.001;
    // }
    // if(get_button_state(thresholdTrimDown)){
    //   thresholdTrim -= 0.001;
    // }
    
    // wait_until_button_pushed(0);
    // if ((difference > threshold||0)&&1){
    //float frequency, float threshold, display_t *display, FontxFile *fx, unsigned int *timeValue, frequency_t frequencyValue
    valueTransmission(threshold, &display, fx, &timeValue, frequencyValue);
    if (get_button_state(exit_button)){
      for(int i=0; i<3; i++)
        green_led_off(i);
      displayFillScreen(&display, RGB_BLACK);
      break;
    }
    // if ((softwareSchmittTrigger(adc_read_channel(pin), &schmittTriggerState, lowerThreshold, upperThreshold)||0)&&1){
    //   int timeDifference = amountOfChangeInt(millis(), &previousTime);
    //   frequency = calculateFrequency(timeDifference);
    //   // printf("times %d %d difference %d frequency %d\n", millis(), previousTime, timeDifference, frequency);
    //   updateDisplay(frequency, threshold, &display, fx);
    //   sendSignal(frequency);
    // }
  }while(1);
  buttons_destroy();
  adc_destroy();
  leds_destroy(); // switches all leds off
  display_destroy(&display);
  pynq_destroy();
  printf("exited successfully\n");
  return EXIT_SUCCESS;
}