typedef enum SendCode {
    TEMP_VAL,
    WEIGHT_VAL,
    ADC_VAL
} SendCode;

enum RecieveCode {
    ADC_REQ,
    TEMP_REQ,
    WEIGHT_REQ,
    TARE_REQ,
    MONOMIAL_0,
    MONOMIAL_1,
    MONOMIAL_2
} RecieveCode;