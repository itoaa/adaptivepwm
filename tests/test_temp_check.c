#include <stdio.h>
#include <math.h>

#define ADC_RESOLUTION 4096.0f
#define ADC_VREF_MV 3300.0f

float ConvertToTemp_Steinhart(unsigned short adc_value) {
    const float R_SERIES = 10000.0f;
    const float R_NOMINAL = 10000.0f;
    const float B_COEFF = 3950.0f;
    const float TEMP_NOMINAL_K = 298.15f;
    
    float adc_voltage = (adc_value / ADC_RESOLUTION) * ADC_VREF_MV;
    float resistance = R_SERIES * adc_voltage / (ADC_VREF_MV - adc_voltage);
    
    if (resistance <= 0.0f) return -273.15f;
    if (resistance > 10e6f) return 150.0f;
    
    float steinhart = resistance / R_NOMINAL;
    steinhart = logf(steinhart);
    steinhart /= B_COEFF;
    steinhart += 1.0f / TEMP_NOMINAL_K;
    steinhart = 1.0f / steinhart;
    
    float temp_celsius = steinhart - 273.15f;
    if (temp_celsius < -40.0f) temp_celsius = -40.0f;
    if (temp_celsius > 150.0f) temp_celsius = 150.0f;
    
    return temp_celsius;
}

int main() {
    printf("ADC 0: %.2f C\n", ConvertToTemp_Steinhart(0));
    printf("ADC 100: %.2f C\n", ConvertToTemp_Steinhart(100));
    printf("ADC 500: %.2f C\n", ConvertToTemp_Steinhart(500));
    printf("ADC 1000: %.2f C\n", ConvertToTemp_Steinhart(1000));
    printf("ADC 2048: %.2f C\n", ConvertToTemp_Steinhart(2048));
    printf("ADC 3000: %.2f C\n", ConvertToTemp_Steinhart(3000));
    printf("ADC 3500: %.2f C\n", ConvertToTemp_Steinhart(3500));
    printf("ADC 4000: %.2f C\n", ConvertToTemp_Steinhart(4000));
    printf("ADC 4095: %.2f C\n", ConvertToTemp_Steinhart(4095));
    return 0;
}
