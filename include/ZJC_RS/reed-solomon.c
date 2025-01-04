#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>

#include "rs_tester.c"
#include "encode.c"
#include "decode.c"
#include "polynomial.c"

// coeff must be of size nroots + 1
// e.g. 2 roots (x + alpha)(x + alpha^2) yields a poly with 3 terms x^2 + g0*x + g1
static polynomial_t reed_solomon_build_generator(field_t field, unsigned int nroots, field_element_t first_consecutive_root, unsigned int root_gap, polynomial_t generator, field_element_t *roots) {
    // generator has order 2*t
    // of form (x + alpha^1)(x + alpha^2)...(x - alpha^2*t)
    for (unsigned int i = 0; i < nroots; i++) {
        roots[i] = field.exp[(root_gap * (i + first_consecutive_root)) % 255];
    }
    return polynomial_create_from_roots(field, nroots, roots);
}

correct_reed_solomon *correct_reed_solomon_create(field_operation_t primitive_polynomial, field_logarithm_t first_consecutive_root, field_logarithm_t generator_root_gap, size_t num_roots) {
    correct_reed_solomon *rs = calloc(1, sizeof(correct_reed_solomon));
    rs->field = field_create(primitive_polynomial);

    //rs->block_length = 255;
    rs->block_length = 255;
    rs->min_distance = num_roots;
    rs->message_length = rs->block_length - rs->min_distance;

    rs->first_consecutive_root = first_consecutive_root;
    rs->generator_root_gap = generator_root_gap;

    rs->generator_roots = malloc(rs->min_distance * sizeof(field_element_t));

    rs->generator = reed_solomon_build_generator(rs->field, rs->min_distance, rs->first_consecutive_root, rs->generator_root_gap, rs->generator, rs->generator_roots);

    rs->encoded_polynomial = polynomial_create(rs->block_length - 1);
    rs->encoded_remainder = polynomial_create(rs->block_length - 1);

    rs->has_init_decode = false;

    return rs;
}

void correct_reed_solomon_destroy(correct_reed_solomon *rs) {
    field_destroy(rs->field);
    polynomial_destroy(rs->generator);
    free(rs->generator_roots);
    polynomial_destroy(rs->encoded_polynomial);
    polynomial_destroy(rs->encoded_remainder);
    if (rs->has_init_decode) {
        free(rs->syndromes);
        free(rs->modified_syndromes);
        polynomial_destroy(rs->received_polynomial);
        polynomial_destroy(rs->error_locator);
        polynomial_destroy(rs->error_locator_log);
        polynomial_destroy(rs->erasure_locator);
        free(rs->error_roots);
        free(rs->error_vals);
        free(rs->error_locations);
        polynomial_destroy(rs->last_error_locator);
        polynomial_destroy(rs->error_evaluator);
        polynomial_destroy(rs->error_locator_derivative);
        for (unsigned int i = 0; i < rs->min_distance; i++) {
            free(rs->generator_root_exp[i]);
        }
        free(rs->generator_root_exp);
        for (field_operation_t i = 0; i < 256; i++) {
            free(rs->element_exp[i]);
        }
        free(rs->element_exp);
        polynomial_destroy(rs->init_from_roots_scratch[0]);
        polynomial_destroy(rs->init_from_roots_scratch[1]);
    }
    free(rs);
}

void correct_reed_solomon_debug_print(correct_reed_solomon *rs) {
    for (unsigned int i = 0; i < 256; i++) {
        printf("%3d  %3d    %3d  %3d\n", i, rs->field.exp[i], i, rs->field.log[i]);
    }
    printf("\n");

    printf("roots: ");
    for (unsigned int i = 0; i < rs->min_distance; i++) {
        printf("%d", rs->generator_roots[i]);
        if (i < rs->min_distance - 1) {
            printf(", ");
        }
    }
    printf("\n\n");

    printf("generator: ");
    for (unsigned int i = 0; i < rs->generator.order + 1; i++) {
        printf("%d*x^%d", rs->generator.coeff[i], i);
        if (i < rs->generator.order) {
            printf(" + ");
        }
    }
    printf("\n\n");

    printf("generator (alpha format): ");
    for (unsigned int i = rs->generator.order + 1; i > 0; i--) {
        printf("alpha^%d*x^%d", rs->field.log[rs->generator.coeff[i - 1]], i - 1);
        if (i > 1) {
            printf(" + ");
        }
    }
    printf("\n\n");

    printf("remainder: ");
    bool has_printed = false;
    for (unsigned int i = 0; i < rs->encoded_remainder.order + 1; i++) {
        if (!rs->encoded_remainder.coeff[i]) {
            continue;
        }
        if (has_printed) {
            printf(" + ");
        }
        has_printed = true;
        printf("%d*x^%d", rs->encoded_remainder.coeff[i], i);
    }
    printf("\n\n");

    printf("syndromes: ");
    for (unsigned int i = 0; i < rs->min_distance; i++) {
        printf("%d", rs->syndromes[i]);
        if (i < rs->min_distance - 1) {
            printf(", ");
        }
    }
    printf("\n\n");

    printf("numerrors: %d\n\n", rs->error_locator.order);

    printf("error locator: ");
    has_printed = false;
    for (unsigned int i = 0; i < rs->error_locator.order + 1; i++) {
        if (!rs->error_locator.coeff[i]) {
            continue;
        }
        if (has_printed) {
            printf(" + ");
        }
        has_printed = true;
        printf("%d*x^%d", rs->error_locator.coeff[i], i);
    }
    printf("\n\n");

    printf("error roots: ");
    for (unsigned int i = 0; i < rs->error_locator.order; i++) {
        printf("%d@%d", polynomial_eval(rs->field, rs->error_locator, rs->error_roots[i]), rs->error_roots[i]);
        if (i < rs->error_locator.order - 1) {
            printf(", ");
        }
    }
    printf("\n\n");

    printf("error evaluator: ");
    has_printed = false;
    for (unsigned int i = 0; i < rs->error_evaluator.order; i++) {
        if (!rs->error_evaluator.coeff[i]) {
            continue;
        }
        if (has_printed) {
            printf(" + ");
        }
        has_printed = true;
        printf("%d*x^%d", rs->error_evaluator.coeff[i], i);
    }
    printf("\n\n");

    printf("error locator derivative: ");
    has_printed = false;
    for (unsigned int i = 0; i < rs->error_locator_derivative.order; i++) {
        if (!rs->error_locator_derivative.coeff[i]) {
            continue;
        }
        if (has_printed) {
            printf(" + ");
        }
        has_printed = true;
        printf("%d*x^%d", rs->error_locator_derivative.coeff[i], i);
    }
    printf("\n\n");

    printf("error locator: ");
    for (unsigned int i = 0; i < rs->error_locator.order; i++) {
        printf("%d@%d", rs->error_vals[i], rs->error_locations[i]);
        if (i < rs->error_locator.order - 1) {
            printf(", ");
        }
    }
    printf("\n\n");
}


void print_test_type(size_t block_length, size_t message_length,
                     size_t num_errors, size_t num_erasures) {
    printf(
        "testing reed solomon block length=%zu, message length=%zu, "
        "errors=%zu, erasures=%zu...",
        block_length, message_length, num_errors, num_erasures);
}

void fail_test() {
    printf("FAILED\n");
    exit(1);
}

void pass_test() { printf("PASSED\n"); }

void run_tests(correct_reed_solomon *rs, rs_testbench *testbench,
               size_t block_length, size_t test_msg_length, size_t num_errors,
               size_t num_erasures, size_t num_iterations) {
    rs_test test;
    test.encode = rs_correct_encode;
    test.decode = rs_correct_decode;
    test.encoder = rs;
    test.decoder = rs;
    print_test_type(block_length, test_msg_length, num_errors, num_erasures);
    for (size_t i = 0; i < num_iterations; i++) {
        rs_test_run run = test_rs_errors(&test, testbench, test_msg_length, num_errors,
                                     num_erasures);
        if (!run.output_matches) {
            fail_test();
        }
    }
    pass_test();
}

void writedata(FILE* f){
    unsigned char* c = (unsigned char*)malloc(sizeof(unsigned char));
    for(int j = 0;j<5242880;j++){
        *c = rand() % 256;
        fwrite(c, 1, 1, f);
    }
}

int generate(unsigned int n){
    //FILE* f1 = fopen("src1.dat", "ab+");
    //FILE* f2 = fopen("src2.dat", "ab+");
    FILE* f1 = fopen("exp1.dat", "ab+");
    FILE* f2 = fopen("exp2.dat", "ab+");
    if(f1 == NULL || f2 == NULL) return 0;
    for(unsigned int i = 0;i < n;i++){
        writedata(f1);
        writedata(f2);
    }
    fclose(f1);
    fclose(f2);
    return 1;
}

unsigned int getfsize(const char *filename){
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return -1;
    }
    fseek(file, 0, SEEK_END);
    unsigned int size = ftell(file);
    fclose(file);
    return size;
}

void add_records(double t1, double t2, double t3, double t4, int mind){
    FILE* res = fopen("output.csv", "a+");
    fprintf(res ,"%f, %f, %f, %f, %f, %f, %d\n", t1, t2, t3, t4, t2-t1, t4-t3, mind);
    fclose(res);
}

unsigned int max(unsigned int a, unsigned int b){
    if(a>b) return a;
    return b;
}


/*
 * rs_encodef : generate the rs priority block
 *      param: left the i-th block's path
 *      param: right the (i+1)-th block's path
 *      param: rs_f the priority block path
 *      default: FEC MODE if BEC, exchanging the left & right
 * */
int rs_encodef(char* left, char* right, char* rs_f, int mind){

    // self beg
    /*
    size_t block_length = 255;
    size_t min_distance = 85;
    size_t message_length = block_length - min_distance;
     */

    size_t message_length = (size_t)mind;
    size_t min_distance = (size_t)mind/2;
    size_t block_length = (size_t)(message_length + min_distance);

    correct_reed_solomon *rs = correct_reed_solomon_create(
            correct_rs_primitive_polynomial_ccsds, 1, 1, min_distance);
    rs_testbench *testbench = rs_testbench_create(block_length, min_distance);

    rs_test test;
    test.encode = rs_correct_encode;
    test.decode = rs_correct_decode;
    test.encoder = rs;
    test.decoder = rs;

    FILE* src1 = fopen(left, "rb");
    FILE* src2 = fopen(right, "rb");
    FILE* src3 = fopen(rs_f, "wb");
    if(src1 == NULL || src2 == NULL || src3 == NULL){
        printf("\nFILE OPENED FAILED!\n");
        return 1;
    }

    unsigned int fsize = max(getfsize(left), getfsize(right));
    unsigned int cnt = 0;

    while(cnt<fsize){
        cnt += min_distance;
        for(unsigned char i=0;i<block_length;i++){
            if(i<min_distance){
                if(fread(testbench->msg + i, 1, 1, src1) == 0){
                    testbench->msg[i] = (unsigned char)0x00;
                }
            }
            else if(min_distance <= i && i < message_length){
                if(fread(testbench->msg + i, 1, 1, src2) == 0){
                    testbench->msg[i] = (unsigned char)0x00;
                }
            }
            else testbench->msg[i] = (unsigned char)0x00;
            //printf("S-%d %d\n",i, testbench->msg[i]);
        }

        test.encode(test.encoder, testbench->msg, message_length, testbench->encoded);
        //memcpy(testbench->corrupted_encoded, testbench->encoded, block_length);
        //if(cnt < 100) for(int i=0;i<block_length;i++){printf("E-%2d %2X %2X\n", i, testbench->msg[i], testbench->encoded[i]);}
        for(int i = message_length; i < block_length;i++){
            fwrite(testbench->encoded + i, 1, 1, src3);
        }
    }

    fclose(src1);
    fclose(src2);
    fclose(src3);

    rs_testbench_destroy(testbench);
    correct_reed_solomon_destroy(rs);

    return 0;
}

/*
 *  rs_fec: recovers the damaged block
 *      param: src  the survival block's path
 *      param: rs_f the priority block's path
 *      param: target   the recovery block's path
 *      param: fec_flag the recovery mode
 *              1->fec
 *              0->bec
 * */
int rs_fec(char* src, char* rs_f, char* target, int fec_flag, int mind){

    // self beg
    /*
    size_t block_length = 255;
    size_t min_distance = 85;
    size_t message_length = block_length - min_distance;
     */

    size_t message_length = (size_t)mind;
    size_t min_distance = (size_t)mind/2;
    size_t block_length = (size_t)(message_length + min_distance);

    correct_reed_solomon *rs = correct_reed_solomon_create(
            correct_rs_primitive_polynomial_ccsds, 1, 1, min_distance);
    rs_testbench *testbench = rs_testbench_create(block_length, min_distance);

    rs_test test;
    test.encode = rs_correct_encode;
    test.decode = rs_correct_decode;
    test.encoder = rs;
    test.decoder = rs;

    FILE* src1;
    FILE* src2;


    src1 = fopen(src, "wb");
    if(src1 == NULL){printf("%s w F!\n", src);}
    src2 = fopen(target, "rb");
    if(src2 == NULL){printf("%s r F!\n", target);}
    FILE* src3 = fopen(rs_f, "rb");
    if(src3 == NULL){printf("%s r F!\n", rs_f);}
    if(src1 == NULL || src2 == NULL || src3 == NULL){
        printf("\nFile Opened Failed!\n");
        return 1;
    }

    unsigned int fsize = max(getfsize(src), getfsize(rs_f));
    unsigned int cnt = 0;

    while(cnt < fsize){
        cnt += min_distance;
        int pass = 0;
        memset(testbench->corrupted_encoded, 0, sizeof(testbench->corrupted_encoded));


        for (int i = 0; i < block_length; i++) {
            testbench->indices[i] = i;
        }

        for(int i=0;i<min_distance;i++){
            if(fec_flag == 1){
                testbench->corrupted_encoded[i] = 0;
                if(fread(testbench->corrupted_encoded + min_distance + i, 1, 1, src2) == 0){
                    pass++;
                    testbench->corrupted_encoded[min_distance + i] = 0x00;
                }
                int index = testbench->indices[i];
                testbench->erasure_locations[i] = index;
                if(fread(testbench->corrupted_encoded + min_distance + min_distance + i, 1, 1, src3) == 0){
                    testbench->corrupted_encoded[min_distance + min_distance + i] = 0x00;
                }
            }else if(fec_flag == 0){
                testbench->corrupted_encoded[min_distance + i] = 0x00;
                if(fread(testbench->corrupted_encoded + i, 1, 1, src2) == 0){
                    pass++;
                    testbench->corrupted_encoded[i] = 0x00;
                }
                int index = testbench->indices[min_distance + i];
                testbench->erasure_locations[i] = index;
                if(fread(testbench->corrupted_encoded + min_distance + min_distance + i, 1, 1, src3) == 0){
                    testbench->corrupted_encoded[min_distance + min_distance + i] = 0x00;
                }
            }else{
                testbench->corrupted_encoded[min_distance + min_distance + i] = 0x00;
                if(fread(testbench->corrupted_encoded + i, 1, 1, src2) == 0){
                    pass++;
                    testbench->corrupted_encoded[i] = 0x00;
                }
                int index = testbench->indices[min_distance + min_distance + i];
                testbench->erasure_locations[i] = index;
                if(fread(testbench->corrupted_encoded + min_distance + i, 1, 1, src3) == 0){
                    testbench->corrupted_encoded[min_distance + i] = 0x00;
                }
            }


        }

        test.decode(test.decoder, testbench->corrupted_encoded, block_length,
                    testbench->erasure_locations, min_distance,
                    testbench->recvmsg, testbench->message_length - message_length, testbench->min_distance);

        unsigned char* c = (unsigned char*)malloc(sizeof(unsigned char));
        for(int i = 0;i<min_distance-pass;i++){
            if(fec_flag == 1){*c = testbench->recvmsg[i];}
            else if(fec_flag == 0){*c = testbench->recvmsg[i + min_distance];}
            else {*c = testbench->recvmsg[i + min_distance + min_distance];}
            fwrite(c, 1, 1, src1);
        }
        //if(cnt < 100) for(int i=0;i<block_length;i++){printf("R-%2d %2X %2X %2X\n", i, *(testbench->msg + i), *(testbench->recvmsg + i), *(testbench->corrupted_encoded + i));}
    }

    fclose(src1);
    fclose(src2);
    fclose(src3);

    rs_testbench_destroy(testbench);
    correct_reed_solomon_destroy(rs);

    return 0;
}
