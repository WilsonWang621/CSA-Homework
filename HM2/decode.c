#include<stdio.h>
#include<stdint.h>
#include <math.h>

union F32{
    float f;
    uint32_t u;
};

union F64{
    double d;
    uint64_t u;
};

void decode_float(float x){
    union F32 v;

    v.f = x;

    uint32_t sign = (v.u >> 31) & 1;
    uint32_t exp = (v.u >> 23) & 0xff;
    uint32_t frac = v.u & 0x7fffff;

    printf("sign = %u\n", sign);
    printf("exp = %u\n", exp);
    printf("frac = %u\n", frac);

    if(exp == 0 && frac == 0){
        printf("zero\n");
    }
    else if(exp == 0){
        printf("denormal\n");
    }
    else if(exp == 255 && frac == 0){
        printf("Inf\n");
    }
    else if(exp == 255){
        printf("NaN\n");
    }
    else{
        printf("normal\n");
    }
}

void decode_double(double x){
    union F64 v;
    v.d = x;

    uint64_t sign = (v.u >> 63) & 1;
    uint64_t exp = (v.u >> 52) & 0x7ff;
    uint64_t frac = v.u & 0xfffffffffffff;

    printf("sign = %lu\n", sign);
    printf("exp = %lu\n", exp);
    printf("frac = %lu\n", frac);

    if(exp == 0 && frac == 0){
        printf("zero\n");
    }
    else if(exp == 0){
        printf("denormal\n");
    }
    else if(exp == 2047 && frac == 0){
        printf("Inf\n");
    }
    else if(exp == 2047){
        printf("NaN\n");
    }
    else{
        printf("normal\n");
    }
}

int main(){
    float tests1[] = {
        0.0f,
        -0.0f,
        1.0f,
        33.0f,
        1.0f / 0.0f,
        0.0f / 0.0f
    };
    int n1 = sizeof(tests1) / sizeof(tests1[0]);

    double tests2[] = {
        0.0,
        -0.0,
        1.0,
        33.0,
        1.0 / 0.0,
        0.0 / 0.0
    };

    int n2 = sizeof(tests2) / sizeof(tests2[0]);

    printf("===== FLOAT =====\n");

    for(int i = 0; i < n1; i++){
        printf("input: %f\n", tests1[i]);
        decode_float(tests1[i]);
        printf("========\n");
    }

    printf("===== DOUBLE =====\n");
    for(int i = 0; i < n2; i++){
        printf("input: %lf\n", tests2[i]);
        decode_double(tests2[i]);
        printf("========\n");
    }

    return 0;
}