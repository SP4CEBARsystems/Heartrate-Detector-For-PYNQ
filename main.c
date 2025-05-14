#include <libpynq.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#define MAX_SIZE 1000

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
  //time_t is a 32 bit int just like int
  return clock()*1000 / CLOCKS_PER_SEC;
}

int calculateFrequency(int timeDifference){
  if (timeDifference == 0){
    return 0;
  }
  return 60000 / timeDifference;
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
  if(*schmittTriggerState==1 && sample<lowerThreshold){
    *schmittTriggerState = 0;
    return 0;
  }else if (*schmittTriggerState==0 && sample>upperThreshold){
    *schmittTriggerState = 1;
    return 1;
  }
  return 0;
}

void frequencyDetection(frequency_t frequencyValue){
  // float sample = frequencyValue.sample;
  int *schmittTriggerState = frequencyValue.schmittTriggerState;
  float lowerThreshold = frequencyValue.lowerThreshold;
  float upperThreshold = frequencyValue.upperThreshold;
  int *previousTime = frequencyValue.previousTime;
  adc_channel_t pin = frequencyValue.pin;
  //float sample, int *schmittTriggerState, float lowerThreshold, float upperThreshold, int *previousTime
  if ((softwareSchmittTrigger(adc_read_channel(pin), schmittTriggerState, lowerThreshold, upperThreshold)||0)&&1){
    int timeDifference = amountOfChangeInt(millis(), previousTime);
    float frequency = calculateFrequency(timeDifference);
    // printf("times %d %d difference %d frequency %d\n", millis(), previousTime, timeDifference, frequency);
    *frequencyValue.schmittTriggerState = *schmittTriggerState;
    *frequencyValue.previousTime        = *previousTime;
    *frequencyValue.frequency = frequency;
  }
}

void waitUntilSomeTimePassed(unsigned int *timeValue, unsigned int duration, frequency_t frequencyValue){
  //duration is in milliseconds
    while(millis()-(*timeValue)<duration){
        // printf("waiting %d...\n", millis()-(*timeValue));
      frequencyDetection(frequencyValue);
    }
    *timeValue=millis();
}

void waitSomeTime(unsigned int duration, frequency_t frequencyValue){
  unsigned int timeValue = millis();
  waitUntilSomeTimePassed(&timeValue, duration, frequencyValue);
}

void sendSignal(float frequency, unsigned int *timeValue, frequency_t frequencyValue){
  waitUntilSomeTimePassed(timeValue, 1000, frequencyValue);
  char string[MAX_SIZE];
  uint8_t byte[] = "";
  sprintf(string, "%.0fBPM", frequency);
  for (int i = 0; i < 20; i++)
  {
    byte[i] = (uint8_t)string[i];
  }

  // (char *)byte
  for (int i=0; byte[i]!='\0'; i++)
  {
    uart_send(UART0, byte[i]);
    // sleep_msec(100);
    // customWait();
    waitSomeTime(100, frequencyValue);
    printf("new code sent byte %c\n", byte[i]);
  }

  // for (int i = 0; byte[i] != '\0'; i++)
  // {
  //   uart_send(UART0, byte[i]);
  //   sleep_msec(100);
  //   printf("sent byte %c\n", byte[i]);
  // }
}

float smoothener(float change, float old, float factor){
  return change*factor+old*(1-factor);
}

float average2(float v1, float v2){
  return 0.5*(v1+v2);
}

float calibrate(const int calibrationButton, adc_channel_t pin, int upperMode){
  wait_until_button_pushed(calibrationButton);
  printf("calibrating...\n");
  float sample = 0;
  // float lowestValue = 0;
  float highestValue = 0;
  float averageValue = 0;
  while(get_button_state(calibrationButton)){
    sample = adc_read_channel(pin);
    // lowestValue=fmin(sample, lowestValue);
    highestValue=fmax(sample, highestValue);
    averageValue=smoothener(sample, averageValue, 0.1);
  }
  if (upperMode) return average2(highestValue, averageValue);
  return highestValue;
}

void calibrateThresholds(float *lowerThreshold, float *upperThreshold, adc_channel_t pin, const int calibrateThresholdL, const int calibrateThresholdH){
  printf("press button 0 to start neutral calibration...\n");
  *lowerThreshold= calibrate(calibrateThresholdL, pin, 0);
  printf("press button 1 to start pulse calibration...\n");
  *upperThreshold= calibrate(calibrateThresholdH, pin, 1);
  printf("done calibrating: lower: %f, upper: %f\n", *lowerThreshold, *upperThreshold);
}

void valueTransmission(float frequency, float threshold, display_t *display, FontxFile *fx, unsigned int *timeValue, frequency_t frequencyValue){
  updateDisplay(frequency, threshold, display, fx);
  sendSignal(frequency, timeValue, frequencyValue);
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

  switchbox_set_pin(IO_PMODA1, SWB_UART0_TX);
  uart_init(UART0);
  uart_reset_fifos(UART0);
  display_t display;
  display_init(&display);
  displayFillScreen(&display, RGB_BLACK);
  FontxFile fx[2];
  InitFontx(fx, "./ILMH32XB.FNT", "./ILMH32XB.FNT");

  adc_init();
  buttons_init();
  
  adc_channel_t pin = ADC0;
  // float threshold = 0.007*10000000;
  float threshold = 0.1;
  // float thresholdTrim = 0;
  // float previousSample = adc_read_channel(pin);
  int previousTime = millis();
  const int calibrateThresholdL = BUTTON0;
  const int calibrateThresholdH = BUTTON1;
  // const int resetThreshold     = BUTTON1;
  // const int thresholdTrimUp    = BUTTON2;
  // const int thresholdTrimDown  = BUTTON3;
  int frequency = 0;

  frequency_t frequencyValue;
  frequencyValue.sample              = 0;
  frequencyValue.schmittTriggerState = &schmittTriggerState;
  frequencyValue.lowerThreshold      = lowerThreshold;
  frequencyValue.upperThreshold      = upperThreshold;
  frequencyValue.previousTime        = &previousTime;
  frequencyValue.pin        = pin;

  calibrateThresholds(&lowerThreshold, &upperThreshold, pin, calibrateThresholdL, calibrateThresholdH);


  do{
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
    valueTransmission(frequency, threshold, &display, fx, &timeValue, frequencyValue);
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
  display_destroy(&display);
  pynq_destroy();
  return EXIT_SUCCESS;
}