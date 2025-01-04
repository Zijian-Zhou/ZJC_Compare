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


long long getfsize(const char *filename){
	FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long long size = ftell(file);
    fclose(file);
    return size;
}

long long max(long long a, long long b){
    if(a>b) return a;
    return b;
}


int encode_file(char* filelist[], int n, int k){

    size_t message_length = (size_t)k;
    size_t min_distance = (size_t)(n - k);
    size_t block_length = (size_t)n;

    correct_reed_solomon *rs = correct_reed_solomon_create(
            correct_rs_primitive_polynomial_ccsds, 1, 1, min_distance);
    rs_testbench *testbench = rs_testbench_create(block_length, min_distance);

    rs_test test;
    test.encode = rs_correct_encode;
    test.decode = rs_correct_decode;
    test.encoder = rs;
    test.decoder = rs;

    FILE *FilePointers[n];
    long long max_filesize =  -1;

    for(int i = 0; i < n; i++){
        if(i < k){
            FilePointers[i] = fopen(filelist[i], "rb");
            max_filesize = max(max_filesize, getfsize(filelist[i]));
        }else{
            FilePointers[i] = fopen(filelist[i], "wb");
        }

        if(FilePointers[i] == NULL){
            printf("File Open Failed AT %s\n", filelist[i]);
            for(int j = 0; j < i; j++){
                fclose(FilePointers[j]);
            }
            return -1;
        }
    }

    int cnt = 0;
    
    while(cnt < max_filesize){
        cnt++;
        for(int i = 0;i < n; i++){
            if(i < k){
                if(fread(testbench->msg + i, 1, 1, FilePointers[i]) == 0){
                    testbench->msg[i] = (unsigned char)0x00;
                }
            }else testbench->msg[i] = (unsigned char)0x00;
        }
        test.encode(test.encoder, testbench->msg, message_length, testbench->encoded);
        for(int i = k; i < n;i++){
            fwrite(testbench->encoded + i, 1, 1, FilePointers[i]);
        }
    }


    for(int i = 0; i < n;i++) fclose(FilePointers[i]);
    rs_testbench_destroy(testbench);
    correct_reed_solomon_destroy(rs);
    return 0;
}

bool check_loss(int i, int loss_idx[], int loss_num){
    bool flag = 0;
    for(int j = 0;j < loss_num;j++){
        if(i == loss_idx[j]){
            flag = 1;
            break;
        }
    }
    return flag;
}

int decode_file(char* filelist[], int n, int k, int loss_idx[], int loss_num){
    size_t message_length = (size_t)k;
    size_t min_distance = (size_t)(n - k);
    size_t block_length = (size_t)n;
    
	
    correct_reed_solomon *rs = correct_reed_solomon_create(
            correct_rs_primitive_polynomial_ccsds, 1, 1, min_distance);
    rs_testbench *testbench = rs_testbench_create(block_length, min_distance);

    rs_test test;
    test.encode = rs_correct_encode;
    test.decode = rs_correct_decode;
    test.encoder = rs;
    test.decoder = rs;

    FILE *FilePointers[n];
    long long max_filesize =  -1;

    for(int i = 0; i < n; i++){
        bool flag = check_loss(i, loss_idx, loss_num);
        if(flag){
            FilePointers[i] = fopen(filelist[i], "wb");
        }else{
            FilePointers[i] = fopen(filelist[i], "rb");
            max_filesize = max(max_filesize, getfsize(filelist[i]));
        }

        if(FilePointers[i] == NULL){
            printf("File Open Failed AT %s\n", filelist[i]);
            for(int j = 0; j < i; j++){
                fclose(FilePointers[j]);
            }
            return -1;
        }
    }

    int cnt=0;
    
    while(cnt < max_filesize){
    	
        cnt++;
        int detected = 0;
        unsigned char* c = (unsigned char*)malloc(sizeof(unsigned char));
		memset(testbench->corrupted_encoded, 0, sizeof(testbench->corrupted_encoded));
        
        for(int i = 0; i < n; i++){
            bool flag = check_loss(i, loss_idx, loss_num);
            if(flag){
                testbench->erasure_locations[detected] = i;
                detected++; 
            }else{
                if(fread(testbench->corrupted_encoded + i, 1, 1, FilePointers[i]) == 0){
                	testbench->corrupted_encoded[i] = 0x00;
				}
            }
        }

        test.decode(test.decoder, testbench->corrupted_encoded, block_length,
                    testbench->erasure_locations, loss_num,
                    testbench->recvmsg, testbench->message_length - message_length, testbench->min_distance);
        
        for(int i = 0;i < loss_num;i++){
            *c = testbench->recvmsg[loss_idx[i]];
            fwrite(c, 1, 1, FilePointers[loss_idx[i]]);
        }
    }
    
    for(int i = 0;i < n;i++) fclose(FilePointers[i]);
    rs_testbench_destroy(testbench);
    correct_reed_solomon_destroy(rs);

    return 0;
}

