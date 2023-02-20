typedef enum SendCode {
    TEMP_VAL,
    WEIGHT_VAL
} SendCode;

enum RecieveCode {
    CALIBRATION_START,
    CALIBRATION_VAL,
    CALIBRATION_END,
    TEMP_REQ,
    WEIGHT_REQ,
    TARE_REQ
} RecieveCode;