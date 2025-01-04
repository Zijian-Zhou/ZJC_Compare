#include "lrc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* gcc example.c -llrc */

void closeallfile(FILE* files[], int n){
    for(int i = 0; i < n ; i++){
       fclose(files[i]);
    }
}

long long getfsize(const char * fn){

    FILE * file = fopen(fn, "rb");
    if(file == NULL) return -1;
    
    fseek(file, 0, SEEK_END);
    long long size = ftell(file);
    fclose(file);
    
    return size;
}

long long max(long long a, long long b){
   if(a >= b) return a;
   return b;
}


int encode_file(char* files[], int k, int l, int r){

    lrc_t *lrc = &(lrc_t) {0};
    lrc_buf_t *buf = &(lrc_buf_t) {0};
    long long max_size = -1;
    
    FILE* filelist[k + l + r];
    for(int i = 0; i < k + l + r;i++){
        if(i < k){
            filelist[i] = fopen(files[i], "rb");
            max_size = max(max_size, getfsize(files[i]));
        }else filelist[i] = fopen(files[i], "wb");
    
        if(filelist[i] == NULL){
            for(int j = 0; j < i ; j++){
            	fclose(filelist[j]);
            }
            printf("FILE OPEN ERROR AT %s\n", files[i]);
            return -1;
        }
    }
    
    uint8_t* local_group_nums;
    local_group_nums = (uint8_t*)malloc(sizeof(uint8_t) * l);
    memset(local_group_nums, 0, sizeof(uint8_t) * l);
    for(int i = 0; i < l;i++){
        local_group_nums[i] = k / l;
    }
    
    if(lrc_init_n(lrc, l, local_group_nums, l + r) != 0){
        closeallfile(filelist, k + l + r);
        return -2;
    }
    
    if(lrc_buf_init(buf, lrc, 1) != 0){
        closeallfile(filelist, k + l + r);
    	return -3;
    }
    
    while(max_size > 0){
        max_size--;
    	
    	for(int i = 0 ; i < k ; i ++){
    	    memset(buf->data[i], 0, sizeof(buf->data[i]));
    	    fread(buf->data[i], 1, 1, filelist[i]);
    	}
    	
    	for(int i = k; i < k + l + r; i++){
    	    memset(buf->data[i], 0, sizeof(buf->data[i]));
    	}
    	
    	lrc_encode(lrc, buf);
    	
    	
    	for(int i = 0; i < l + r; i++){
    	    //printf("%s\n", buf->code[i]);
    	    fwrite(buf->code[i], 1, 1, filelist[k + i]);
    	}
    }
    
    closeallfile(filelist, k + l + r);
    lrc_destroy(lrc);
    lrc_buf_destroy(buf);
    free(local_group_nums);
    
    return 0;
}

int check_loss(int* loss_idx, int loss_num, int target){
    for(int i = 0; i < loss_num;i++){
        if(target == loss_idx[i]) return 1;
    }
    return 0;
}

int decode_file(char* files[], int k, int l, int r, int* loss_idx, int loss_num){

    lrc_t *lrc = &(lrc_t) {0};
    lrc_buf_t *buf = &(lrc_buf_t) {0};
    long long max_size = -1;
    
    FILE* filelist[k + l + r];
    for(int i = 0; i < k + l + r;i++){
        if(check_loss(loss_idx, loss_num, i) == 0){
            filelist[i] = fopen(files[i], "rb");
            max_size = max(max_size, getfsize(files[i]));
        }else filelist[i] = fopen(files[i], "wb");
    
        if(filelist[i] == NULL){
            for(int j = 0; j < i ; j++){
            	fclose(filelist[j]);
            }
            printf("FILE OPEN ERROR AT %s\n", files[i]);
            return -1;
        }
    }
    
    uint8_t* local_group_nums;
    local_group_nums = (uint8_t*)malloc(sizeof(uint8_t) * l);
    memset(local_group_nums, 0, sizeof(uint8_t) * l);
    for(int i = 0; i < l;i++){
        local_group_nums[i] = k / l;
    }
    
    if(lrc_init_n(lrc, l, local_group_nums, l + r) != 0){
        closeallfile(filelist, k + l + r);
        return -2;
    }
    
    if(lrc_buf_init(buf, lrc, 1) != 0){
        closeallfile(filelist, k + l + r);
    	return -3;
    }
    
    int8_t erased[k + l +r];
    for(int i = 0; i < k + l + r; i++){
        if(check_loss(loss_idx, loss_num, i)) erased[i] = 1;
        else erased[i] = 0;
    }
    
    while(max_size > 0){
    	max_size -= 1;
    	
    	for(int i = 0 ; i < k + l + r ; i ++){
    	    if(erased[i] == 1){
    	        if(i < k){
    	            memset(buf->data[i], 0, sizeof(buf->data[i]));
    	        }else{
    	            memset(buf->code[i - k], 0, sizeof(buf->code[i - k]));
    	        }
    	    }else{
    	        if(i < k){
    	            memset(buf->data[i], 0, sizeof(buf->data[i]));
    	    	    fread(buf->data[i], 1, 1, filelist[i]);
    	        }else{
    	            memset(buf->code[i - k], 0, sizeof(buf->code[i - k]));
    	    	    fread(buf->code[i - k], 1, 1, filelist[i]);
    	        }
    	    }
    	}

    	lrc_decode(lrc, buf, erased);

    	for(int i = 0; i < k + l + r; i++){
    	    if(erased[i]){
    	        if(i < k){
    	            fwrite(buf->data[i], 1, 1, filelist[i]);
    	        }else{
    	            fwrite(buf->code[i - k], 1, 1, filelist[i]);
    	        }
    	    }
    	}
    	
    }
    
    closeallfile(filelist, k + l + r);
    lrc_destroy(lrc);
    lrc_buf_destroy(buf);
    free(local_group_nums);
    
    return 0;
}

int main(int argc, char **argv){
    
    char * files[] = {
        "blocks/block_0.dat",
        "blocks/block_1.dat",
        "blocks/block_2.dat",
        "blocks/block_3.dat",
        "parity/parity_0.dat",
        "parity/parity_1.dat",
        "parity/parity_2.dat",
        "parity/parity_3.dat",
    };
    
    printf("ENCODE CODE: %d\n", encode_file(files, 4, 2, 2));
    
    
    int idx[] = {0, 3, 6, 7}; 
    for(int i = 1; i < 5; i++){
        printf("decode %d\n", i);
        printf("DECODE CODE: %d\n", decode_file(files, 4, 2, 2, idx, i));
    }
    
    return 0;
}
