#include <string.h>
#define MAX_MEASURMENTS 64


static float measurments[MAX_MEASURMENTS];
static float weights[MAX_MEASURMENTS];
static size_t num_measurments;


int backup_init() {

}

int cal_init() {
    memset(measurments, 0, MAX_MEASURMENTS * sizeof measurments[0]);
    num_measurments = 0;
    return 0;
}

int cal_add_measurment(float adc_measurment, float weight) {
    if(num_measurments == MAX_MEASURMENTS) {
        return -1; // too many measurments
    }
    else {
        for(int i = 0; i < num_measurments; i++) {
            if(adc_measurment == measurments[i]) {
                return -2; // ensure invertible matrix
            }
        }
    }
    measurments[num_measurments] = adc_measurment;
    weights[num_measurments++] = weight;
    return 0;
}

int cal_commit_measurments() {
    // basic least squares regression
    float X[num_measurments][3];
    for(int i = 0; i < num_measurments; i++) {
        float val = measurments[i];
        X[i][0] = 1.0;
        X[i][1] = val;
        X[i][2] = val * val;
    }
    float XX[3][3];
    for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
            float temp = 0.0;
            for(int k = 0; k < num_measurments; k++) {
                temp += X[k][i] * X[k][j];
            }
            XX[i][j] = temp;
        }
    }
    float det = 0.0;
    for(int i = 0; i < 3; i++) {
        det += (XX[0][i]*(XX[1][(i+1)%3]*XX[2][(i+2)%3] - XX[1][(i+2)%3]* XX[2][(i+1)%3]));
    }
    float adj_XX[3][3];
    for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
            
        }
    }

}