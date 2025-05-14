#include <libpynq.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>

// #include <chrono>


// #include <libpynq.h>
// #include <unistd.h>

// #include <arm_shared_memory_system.h>
// #include <display.h>
// #include <gpio.h>
// #include <lcdconfig.h>
// #include <log.h>
// #include <platform.h>
// #include <string.h>
// #include <switchbox.h>
// #include <unistd.h>
// #include <util.h>

#define MAX_SIZE 1000

int BEATLED = 0;
int graphPosition = 0;
// float averagingSizeExponent = 1;
int averagingSize = 5000;
// float threshold = .0015;
float threshold = .03;

int bpmTextShown = 0;


// int ledstate = 0;

// float thresholdAdjustment = 0.00001;
float thresholdAdjustment = 0.0001;

const int calibrateThresholdL = BUTTON0;
const int calibrateThresholdH = BUTTON1;
const int reset_button        = BUTTON2;
const int exit_button         = BUTTON3; 

struct timeval program_start;

typedef struct frequency2_t{
  float sample;
  // int *schmittTriggerState;
  // float lowerThreshold;
  // float upperThreshold;
  int *previousTime;
  adc_channel_t pin;
  float *frequency;
  float *previousMeasurement;
  int *timeDifference;
} frequency_t;




//U T I L I T Y

int millis(){
  //"time_t" is a 32 bit int just like "int"
  // return clock()*1000 / CLOCKS_PER_SEC;
  struct timeval time_now;
  gettimeofday(&time_now, 0);
  int elapsed = ( time_now.tv_sec - program_start.tv_sec ) * 1000 + (time_now.tv_usec - program_start.tv_usec)/1000;
  return elapsed;
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


//G R A P H I C S

void displayClearText(display_t *display, uint16_t color){
  // displayFillScreen( display, color );
  displayDrawFillRect ( display, 0, 0, DISPLAY_WIDTH-1, 32, color );
}

void plotSegment(display_t *display, uint16_t x, uint16_t yTop, uint16_t yBottom, uint16_t top, uint16_t bottom, uint16_t color){
  uint16_t pointTop    = fmax( top   , bottom - yBottom     );
  uint16_t pointBottom = fmin( bottom, bottom - yTop );
  if (pointTop >= pointBottom) return;
  // displayDrawPixel ( display, x, y, color );
  displayDrawFillRect ( display, x, pointBottom, x, pointTop, color );
}

void plotSolid(display_t *display, uint16_t x, uint16_t y, uint16_t top, uint16_t bottom, uint16_t color){
  plotSegment( display, x, 0, y, top, bottom, color);
  // uint16_t pointTop    = fmax(top   , bottom-(y+0.5*height));
  // uint16_t pointBottom = fmin(bottom, bottom-(y-0.5*height));
  // if (pointTop >= pointBottom) return;
  // // displayDrawPixel ( display, x, y, color );
  // displayDrawFillRect ( display, x, pointBottom, x, pointTop, color );
}

void plot(display_t *display, uint16_t x, uint16_t y, uint16_t height, uint16_t top, uint16_t bottom, uint16_t color){
  plotSegment( display, x, y-0.5*height, y+0.5*height, top, bottom, color);
  // uint16_t pointTop    = fmax(top   , bottom-(y+0.5*height));
  // uint16_t pointBottom = fmin(bottom, bottom-(y-0.5*height));
  // if (pointTop >= pointBottom) return;
  // // displayDrawPixel ( display, x, y, color );
  // displayDrawFillRect ( display, x, pointBottom, x, pointTop, color );
}

void sensorGraph( display_t *display, int *x, float measurement, float slope, float threshold, int pulseWasDetected ){
  // return;
  uint16_t top = 32;
  uint16_t bottom = DISPLAY_HEIGHT-1;
  // uint16_t actualTop = bottom-top;
  uint16_t range = bottom - top;
  int pulse = slope>threshold;
  displayDrawFillRect ( display, *x, top, *x, bottom, RGB_BLACK );
  float slopeFactor, measurementFactor;
  if (get_switch_state( SWITCH0 )){
    slopeFactor = 100 * range/10;
    measurementFactor = 0.8 * range/3.3;
  }else{
    slopeFactor = 200 * range/10;
    measurementFactor = 8 * range/3.3;
  }
  // pulse ? RGB_GREEN : RGB_BLUE
  uint16_t slopeColor;
  if (pulse){
    if(pulseWasDetected){
      slopeColor = RGB_GREEN;
    }else{
      slopeColor = rgb_conv(0,127,0);
      // slopeColor = RGB_CYAN;
    }
  }else{
    slopeColor = RGB_BLUE;
  } 
  plot     ( display, *x, threshold   * slopeFactor      , 4, top, bottom, RGB_GRAY );
  plotSolid( display, *x, slope       * slopeFactor         , top, bottom, slopeColor );
  // plot ( display, *x, slope       * slopeFactor      , 4, top, bottom, RGB_BLUE  );
  plot     ( display, *x, measurement * measurementFactor, 4, top, bottom, RGB_RED   );
  // if(pulse){plot ( display, *x, actualTop, 20, top, bottom, RGB_GREEN );}
  // if(pulse){plot ( display, *x, 0, 20, top, bottom, RGB_GREEN );}
  (*x)++;
  if((*x)>=DISPLAY_HEIGHT) (*x)=0;
}

float readSensor( adc_channel_t pin ){
  if(get_switch_state( SWITCH0 ))
    return 3.3-adc_read_channel( pin );
  return adc_read_channel( pin );
}

float addToAverage( float value, int *count, float *average ){
  (*count)++;
  if (*count==0) return *average;
  (*average) += (value-(*average)) / (*count);
  return *average;
}

float getAverageMeasurement( adc_channel_t pin, int totalCount ){
  float average = 0;
  int count     = 0;
  for( int i=0; i<totalCount; i++ ){
    // if(pin){}//pin is not unused
    addToAverage( readSensor(pin), &count, &average );
    // addToAverage( 1 +0.5*((sin(0.0007*millis()))>0.5) +0.1*(sin(0.0035*millis())) +0.5*(sin(2000*millis())) +0.2*(sin(11111*millis())), &count, &average );
  }
  return average;
}

float float_min(float a, float b){
  if (a<=b)
    return a;
  return b;
}

float amountOfChange(float value, float *previousValue){
  float difference = value-(*previousValue);
  (*previousValue) = value;
  return difference;
}

int amountOfChangeInt(int value, int *previousValue){
  int difference = value-(*previousValue);
  (*previousValue) = value;
  return difference;
}

float getSlope( float sample, float *previousSample ){
  return amountOfChange( sample, previousSample );
}

// void newMeasurement(){
// }

void drawWordFloat( char *format, float value, uint16_t color, int y, char *string, int *charLength, int *x, display_t *display, FontxFile *fx ){
  sprintf(string, format, value);
  // printf("p0 %d\n", strlen(string));
  (*charLength)+=strlen(string);
  if((*charLength)>29) return;
  *x = displayDrawString( display, fx, *x, y, (uint8_t *)string, color );
}

void drawWordInt( char *format, int value, uint16_t color, int y, char *string, int *charLength, int *x, display_t *display, FontxFile *fx ){
  sprintf(string, format, value);
  // printf("p0 %d\n", strlen(string));
  (*charLength)+=strlen(string);
  if((*charLength)>29) return;
  *x = displayDrawString( display, fx, *x, y, (uint8_t *)string, color );
}

void drawWord( char *format, uint16_t color, int y, char *string, int *charLength, int *x, display_t *display, FontxFile *fx ){
  // sprintf(string, format);
  strcpy(string, format);
  // printf("p0 %d\n", strlen(string));
  (*charLength)+=strlen(string);
  if((*charLength)>29) return;
  *x = displayDrawString( display, fx, *x, y, (uint8_t *)string, color );
}

void updateDisplay(float frequency, float measurement, int timeDifference, display_t *display, FontxFile *fx){
  // return;
  char string[MAX_SIZE];
  // char string[15];
  int x = 0;
  int y = 15;
  int charLength = 0;

  
  if(get_switch_state( SWITCH1 )) {
    if(!bpmTextShown){
      x=50;
      displayClearText( display, RGB_BLACK );
      drawWord( "BPM\n", RGB_GREEN, y, string, &charLength, &x, display, fx );
      bpmTextShown = 1;
      x=0;
    }
    displayDrawFillRect ( display, 0, 0, 31, 15, RGB_BLACK );
    drawWordFloat( "%.0f\n", frequency     , RGB_GREEN, y, string, &charLength, &x, display, fx );
    return;
  }
  bpmTextShown = 0;
  displayClearText( display, RGB_BLACK );
  // uint8_t byte[] = "";
  // sprintf(string, "%.0fB %.3fV %d\n", frequency, measurement, get_switch_state( SWITCH0 ));
  // sprintf(string, "%.0fB %.3fV A%d T%.1f S%d\n", frequency, measurement, averagingSize, threshold, get_switch_state( SWITCH0 ));
  // sprintf(string, "%.0fB %.3fV A%d S%d\n", frequency, measurement, averagingSize, get_switch_state( SWITCH0 ));
  // for (int i = 0; i < 20; i++)
  // {
  //   byte[i] = (uint8_t)string[i];
  // }
  // displayDrawString(display, fx, 0, 31, byte, RGB_WHITE);

  drawWordFloat( "%.0fBPM\n", frequency     , RGB_GREEN, y, string, &charLength, &x, display, fx );
  

  drawWordFloat( "%.3fV\n"  , measurement   , RGB_RED  , y, string, &charLength, &x, display, fx );
  drawWordInt  ( "%dms\n"   , timeDifference, RGB_GREEN, y, string, &charLength, &x, display, fx );
  drawWordFloat( "%.2fHz\n" , frequency/60  , RGB_GREEN, y, string, &charLength, &x, display, fx );

  // sprintf(string, "%.0fBPM\n", frequency);
  // // printf("p0 %d\n", strlen(string));
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_GREEN);

  // sprintf(string, "%.3fV\n", measurement);
  // // printf("p1 %d\n", strlen(string));
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_RED);

  // sprintf(string, "%dms\n", timeDifference);
  // // printf("p2 %d\n", strlen(string));
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_GREEN);

  // sprintf(string, "%.2fHz\n", frequency/60);
  // // printf("p3 %d\n", strlen(string));
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_GREEN);

  x = 0;
  y = 31;
  charLength = 0;

  drawWordInt  ( "A%d\n"  , averagingSize              , RGB_RED  , y, string, &charLength, &x, display, fx );
  drawWordInt  ( "S%d\n"  , get_switch_state( SWITCH0 ), RGB_WHITE, y, string, &charLength, &x, display, fx );
  drawWordFloat( "T%.5f\n", threshold                  , RGB_GRAY , y, string, &charLength, &x, display, fx );
  drawWordFloat( "%.1fs\n", millis()*0.001             , RGB_WHITE, y, string, &charLength, &x, display, fx );

  // sprintf(string,   "A%d\n", averagingSize);
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_RED);

  // sprintf(string,   "S%d\n", get_switch_state( SWITCH0 ));
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_WHITE);

  // sprintf(string,   "T%.5f\n", threshold);
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_GRAY);

  // sprintf(string,   "%.1fs\n", millis()*0.001);
  // charLength+=strlen(string);
  // if(charLength>29) return;
  // x = displayDrawString(display, fx, x, y, (uint8_t *)string, RGB_WHITE);
}

// int softwareSchmittTrigger(float sample, int *schmittTriggerState, float lowerThreshold, float upperThreshold){
//   // printf("%f %f %f %d\n", lowerThreshold, sample, upperThreshold, *schmittTriggerState);
//   if(*schmittTriggerState==1 && sample<lowerThreshold){
//     *schmittTriggerState = 0;
//     printf("trigger off\n");
//     return 0;
//   }else if (*schmittTriggerState==0 && sample>upperThreshold){
//     *schmittTriggerState = 1;
//     printf("trigger on\n");
//     return 1;
//   }
//   return 0;
// }

void displayPulse(){
  printf("PULSE!\n");
  green_led_onoff(2,BEATLED);
  BEATLED = !BEATLED;
}

int pulseDetected(frequency_t frequencyValue){
  int *previousTime = frequencyValue.previousTime;
  int timeDifference = amountOfChangeInt(millis(), previousTime);
  if (timeDifference<200) return 0;
  float frequency = calculateFrequency(timeDifference);
  // printf("%f\n", frequency);
  // printf("times %d %d difference %d frequency %f\n", millis(), *previousTime, timeDifference, frequency);
  *frequencyValue.previousTime        = *previousTime;
  *frequencyValue.timeDifference      = timeDifference;
  *frequencyValue.frequency = frequency;
  // *frequencyValue.frequency = smoothener(frequency, *frequencyValue.frequency, 0.9);
  // *frequencyValue.frequency = frequency;
  // *frequencyValue.frequency = 0.0;
  displayPulse();
  return 1;
}

void frequencyDetection(frequency_t frequencyValue, display_t *display){
  // return;
  // float sample = frequencyValue.sample;
  // int *schmittTriggerState = frequencyValue.schmittTriggerState;
  // float lowerThreshold = frequencyValue.lowerThreshold;
  // float upperThreshold = frequencyValue.upperThreshold;
  float *previousMeasurement = frequencyValue.previousMeasurement;
  adc_channel_t pin = frequencyValue.pin;

  float measurement = getAverageMeasurement( pin, averagingSize );
  float slope = getSlope(measurement, previousMeasurement);
  int pulseWasDetected = 0;
  if(slope>threshold)
    pulseWasDetected = pulseDetected(frequencyValue);

  //float sample, int *schmittTriggerState, float lowerThreshold, float upperThreshold, int *previousTime
  // if ((softwareSchmittTrigger(adc_read_channel(pin), schmittTriggerState, lowerThreshold, upperThreshold)||0)&&1){
  //   pulseDetected(frequencyValue);
  //   *frequencyValue.schmittTriggerState = *schmittTriggerState;
  // }
  // sensorGraph( display, &graphPosition, 0.5*(1+sin(0.1*millis())), 3*(1+sin(0.07*millis())), threshold );
  
  sensorGraph( display, &graphPosition, measurement, slope, threshold, pulseWasDetected );
  
  *previousMeasurement = measurement;
}

void buttonInput(){
  if (millis() % 10 != 0) return;
  if (get_button_state(BUTTON0)){
    threshold -= thresholdAdjustment;
  }
  if (get_button_state(BUTTON1)){
    threshold += thresholdAdjustment;
  }
}

// void waitUntilSomeTimePassed2(unsigned int *timeValue, unsigned int duration){
// // void waitUntilSomeTimePassed(unsigned int *timeValue, unsigned int duration){
//   //duration is in milliseconds
//     while(millis()-(*timeValue)<duration){
//       // printf("waiting %d...\n", millis()-(*timeValue));
//       // frequencyDetection(frequencyValue, display);
//       // buttonInput();
//       // printf("%f",frequencyValue.lowerThreshold);
//       // usleep(1);
//       sleep_msec(1);
//       // printf("duration: %d\n",millis()-(*timeValue));
//       // printf("millis: %d\n",millis());
//       // if (get_button_state(exit_button) || get_button_state(reset_button)) break;
//       if (get_button_state(exit_button)) break;
//     }
//     *timeValue=millis();
// }

void waitUntilSomeTimePassed(unsigned int *timeValue, unsigned int duration, frequency_t frequencyValue, display_t *display){
// void waitUntilSomeTimePassed(unsigned int *timeValue, unsigned int duration){
  //duration is in milliseconds
    while(millis()-(*timeValue)<duration){
      // printf("waiting %d...\n", millis()-(*timeValue));
      frequencyDetection(frequencyValue, display);
      buttonInput();
      // printf("%f",frequencyValue.lowerThreshold);
      // if (get_button_state(exit_button) || get_button_state(reset_button)) break;
      if (get_button_state(exit_button)) break;
    }
    *timeValue=millis();
}

void waitSomeTime(unsigned int duration, frequency_t frequencyValue, display_t *display){
  unsigned int timeValue = millis();
  waitUntilSomeTimePassed(&timeValue, duration, frequencyValue, display);
}

void sendSignal( frequency_t frequencyValue, display_t *display ){
  // return;
  
  char string[MAX_SIZE];
  uint8_t byte[] = "";
  // printf("freq%.0f\n", *(frequencyValue.frequency));
  sprintf(string, "%.0fB", float_min(240,*(frequencyValue.frequency)));//fmax(240, )
  for (int i = 0; i < 20; i++)
  {
    byte[i] = (uint8_t)string[i];
  }
  //should probably use strcpy();

  // (char *)byte
  // (uint8_t *)string
  
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

// void calibrate2(float *lowerThreshold, float *upperThreshold, const int calibrationButton, adc_channel_t pin){
//   wait_until_button_pushed(calibrationButton);
//   printf("calibrating...\n");
//   float sample = 0;
//   float lowestValue = 0;
//   float highestValue = 0;
//   float averageValue = 0;
//   while(get_button_state(calibrationButton)){
//     sample = adc_read_channel(pin);
//     lowestValue=fmin(sample, lowestValue);
//     highestValue=fmax(sample, highestValue);
//     averageValue=smoothener(sample, averageValue, 0.1);
//     // printf("sample %f highestValue %f lowestValue %f averageValue %f\n", sample, highestValue, lowestValue, averageValue);
//   }
//   // if (upperMode) return average2(highestValue, averageValue);
//   printf("highestValue %f lowestValue %f averageValue %f\n", highestValue, lowestValue, averageValue);
//   *upperThreshold = averageValue;
//   *lowerThreshold = highestValue;
// }

// float calibrate(const int calibrationButton, adc_channel_t pin, int upperMode){
//   wait_until_button_pushed(calibrationButton);
//   printf("calibrating...\n");
//   float sample = 0;
//   float lowestValue = 10;
//   float highestValue = 0;
//   float averageValue = 0;
//   while(get_button_state(calibrationButton)){
//     sample = adc_read_channel(pin);
//     lowestValue =smoothener(fmin(sample, lowestValue), lowestValue, 0.1);
//     highestValue=smoothener(fmax(sample, highestValue), highestValue, 0.1);
//     averageValue=smoothener(sample, averageValue, 0.1);
//     // printf("sample %f highestValue %f lowestValue %f averageValue %f\n", sample, highestValue, lowestValue, averageValue);
//     // printf("sample %f\n", sample);
//   }
//   // if (upperMode) return average2(highestValue, averageValue);
//   printf("highestValue %f lowestValue %f averageValue %f\n", highestValue, lowestValue, averageValue);
//   // if (upperMode) return averageValue;
//   if (upperMode) return lowestValue;
//   return highestValue;
// }

void displayDrawStringReally(display_t *display, FontxFile *fx, uint16_t x, uint16_t y, char *string, uint16_t color ){
  uint8_t asciii[strlen(string)];
  for(size_t i=0; i<strlen(string); i++){
    asciii[i]=string[i];
    // string[i]='\0';
  }
  // string[1]='\0';
  displayDrawString(display, fx, x, y, asciii, color);
}

// void calibrateThresholds(display_t *display, FontxFile *fx, float *lowerThreshold, float *upperThreshold, adc_channel_t pin, const int calibrateThresholdL, const int calibrateThresholdH){
//   // printf("press button 0 to start smart calibration...\n");
//   // calibrate2(lowerThreshold, upperThreshold, calibrateThresholdL, pin);
//   displayFillScreen(display, RGB_BLACK);
//   for(int i=0; i<3; i++)
//     green_led_off(i);
//   printf("press button 0 to start neutral calibration...\n");
//   green_led_on(0);
//   char string[14]="neutral    ";
//   displayDrawStringReally(display, fx, 0,  31, string, RGB_WHITE);
//   strcpy(string,"calibration  ");
//   displayDrawStringReally(display, fx, 0, 63, string, RGB_WHITE);
//   strcpy(string,"press button0");
//   displayDrawStringReally(display, fx, 0,  95, string, RGB_WHITE);
//   // green_led_on(1);
//   *lowerThreshold= calibrate(calibrateThresholdL, pin, 0);
//   green_led_off(0);

//   green_led_on(1);
//   printf("press button 1 to start pulse calibration...\n");
//   displayFillScreen(display, RGB_BLACK);
//   // char string[14]="pulse      ";
//   strcpy(string,"pulse        ");
//   displayDrawStringReally(display, fx, 0,  31, string, RGB_WHITE);
//   strcpy(string,"calibration  ");
//   displayDrawStringReally(display, fx, 0, 63, string, RGB_WHITE);
//   strcpy(string,"press button1");
//   displayDrawStringReally(display, fx, 0,  95, string, RGB_WHITE);
//   *upperThreshold= calibrate(calibrateThresholdH, pin, 1);
//   if(calibrateThresholdH){}//unused parameters error is gone
//   printf("done calibrating: lower: %f, upper: %f\n", *lowerThreshold, *upperThreshold);
//   green_led_off(1);
// }

void valueTransmission( display_t *display, FontxFile *fx, unsigned int *timeValue, frequency_t frequencyValue){
  
  
  // waitUntilSomeTimePassed2(timeValue, 1000);
  
  waitUntilSomeTimePassed(timeValue, 1000, frequencyValue, display);

  // ledstate = !ledstate;
  // green_led_onoff( 3, ledstate );
  // return;

  // if (get_button_state(exit_button) || get_button_state(reset_button)) return;
  if (get_button_state(exit_button)) return;
  
  updateDisplay(*(frequencyValue.frequency), *(frequencyValue.previousMeasurement), *frequencyValue.timeDifference, display, fx);
  
  // threshold, 

  sendSignal( frequencyValue, display );

}

// void scheduler(float frequency, float threshold, display_t *display, FontxFile *fx, unsigned int *timeValue, frequency_t frequencyValue){
//   valueTransmission(frequency, threshold, display, fx, timeValue, frequencyValue);
//   // waitUntilASecondPassed()
//   // frequencyDetection();
// }


void clearAllIndicators( display_t *display ){
  for(int i=0; i<3; i++)
    green_led_off(i);
  displayFillScreen( display, RGB_BLACK );
}

int programExit(){
  return get_button_state(exit_button);
}


int main(void)
{
  
  pynq_init();

  // int schmittTriggerState = 0;
  // float lowerThreshold, upperThreshold;
  unsigned int timeValue = 0;
  
  gettimeofday(&program_start, 0);

  // switchbox_set_pin(IO_PMODA1, SWB_UART0_TX);
  switchbox_set_pin(IO_AR13, SWB_UART0_TX);
  uart_init(UART0);
  uart_reset_fifos(UART0);
  display_t display;
  display_init(&display);
  displayClearText(&display, RGB_BLACK);
  // display_set_flip(&display, 0, 1);
  FontxFile fx[2];
  // InitFontx(fx, "./ILMH32XB.FNT", "./ILMH32XB.FNT");
  
  // InitFontx(fx, "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILMH32XB.FNT", "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILMH32XB.FNT");
  InitFontx(fx, "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILGH16XB.FNT", "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILGH16XB.FNT");
  // InitFontx(fx16G, "/home/student/libpynq-5EWC0-2023-v0.2.6/fonts/ILGH16XB.FNT", "");

  // gpio_reset();
  leds_init_onoff();
  adc_init();
  buttons_init();
  switches_init();
  
  adc_channel_t pin = ADC0;
  // float threshold = 0.007*10000000;
  // float threshold = 0.1;
  // float thresholdTrim = 0;
  // float previousSample = adc_read_channel(pin);
  int previousTime = millis();
  // const int resetThreshold     = BUTTON1;
  // const int thresholdTrimUp    = BUTTON2;
  // const int thresholdTrimDown  = BUTTON3;
  float frequency = 0;
  float previousMeasurement = 0;
  int timeDifference = 0;

  frequency_t frequencyValue;
  frequencyValue.sample              = 0;
  // frequencyValue.schmittTriggerState = &schmittTriggerState;
  frequencyValue.previousTime        = &previousTime;
  frequencyValue.timeDifference      = &timeDifference;
  frequencyValue.pin                 = pin;
  frequencyValue.frequency           = &frequency;
  frequencyValue.previousMeasurement = &previousMeasurement;

  

  // int first = 1;


  do{
    // if (first || get_button_state(reset_button)){
    //   calibrateThresholds(&display, fx, &lowerThreshold, &upperThreshold, pin, calibrateThresholdL, calibrateThresholdH);
    //   frequencyValue.lowerThreshold      = lowerThreshold;
    //   frequencyValue.upperThreshold      = upperThreshold;
    //   first=0;
    // }


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
    
    valueTransmission( &display, fx, &timeValue, frequencyValue );
    
    
    // if ((softwareSchmittTrigger(adc_read_channel(pin), &schmittTriggerState, lowerThreshold, upperThreshold)||0)&&1){
    //   int timeDifference = amountOfChangeInt(millis(), &previousTime);
    //   frequency = calculateFrequency(timeDifference);
    //   // printf("times %d %d difference %d frequency %d\n", millis(), previousTime, timeDifference, frequency);
    //   updateDisplay(frequency, threshold, &display, fx);
    //   sendSignal(frequency);
    // }
  }while( !programExit() );
  clearAllIndicators( &display );
  switches_destroy();
  buttons_destroy();
  adc_destroy();
  leds_destroy(); // switches all leds off
  display_destroy(&display);
  pynq_destroy();
  printf("exited successfully\n");
  return EXIT_SUCCESS;
}