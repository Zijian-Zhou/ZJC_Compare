#include<stdio.h>
#include<stdlib.h>
#include <time.h>

typedef long ll;


ll getFileSize(char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Cannot open the file: %s\n", filename);
        //perror("FIO Error\n");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    ll size = ftell(file);
    fclose(file);

    return size;
}


ll maxv(ll a, ll b){
	if(a >= b) return a;
	return b;
}



int encode_file(char* left_file, char* right_file, char* parity_file){
	
	FILE* lf = fopen(left_file, "rb");
	FILE* rf = fopen(right_file, "rb");
	FILE* pf = fopen(parity_file, "wb");
	
	ll max_size = maxv(getFileSize(left_file), getFileSize(right_file));
	
	int* lb = (int*)malloc(sizeof(int));
	int* rb = (int*)malloc(sizeof(int));
	int* pb = (int*)malloc(sizeof(int));
	
	while(max_size--){
		if( 1 != fread(lb, 1, 1, lf)){
			fread(rb, 1, 1, rf);
			fwrite(rb, 1, 1, pf);
			continue;
		}
		
		if(1 != fread(rb, 1, 1, rf)){
			fread(lb, 1, 1, lf);
			fwrite(lb, 1, 1, pf);
			continue;
		}
		
		*pb = *lb ^ *rb;
		
		fwrite(pb, 1, 1, pf);
	}
	
	fclose(lf);
	fclose(rf);
	fclose(pf);
	
	return 0;
}

// left missing
int FEC(char* left_file, char* right_file, char* parity_file, int loss){
	FILE* lf = fopen(left_file, "wb");
	FILE* rf = fopen(right_file, "rb");
	FILE* pf = fopen(parity_file, "rb");

	ll size1 = getFileSize(parity_file);
	if(size1 == -1){
	    return -1;
	}
	ll size2 = getFileSize(right_file);
	if(size2 == -1){
	    return -1;
	}
	ll max_size = maxv(size1, size2);
	
	int* lb = (int*)malloc(sizeof(int));
	int* rb = (int*)malloc(sizeof(int));
	int* pb = (int*)malloc(sizeof(int));
	
	while(max_size-- && loss--){
		
		fread(pb, 1, 1, pf);
		
		if(1 != fread(rb, 1, 1, rf)){
			fwrite(pb, 1, 1, lf);
			continue;
		}
		
		*lb = *pb ^ *rb;
		
		fwrite(lb, 1, 1, lf);
	}
	
	fclose(lf);
	fclose(rf);
	fclose(pf);
	
	return 0;
}


// right missing
int BEC(char* left_file, char* right_file, char* parity_file, int loss){
	FILE* lf = fopen(left_file, "rb");
	FILE* rf = fopen(right_file, "wb");
	FILE* pf = fopen(parity_file, "rb");
	
	ll size1 = getFileSize(parity_file);
	if(size1 == -1){
	    return -1;
	}
	ll size2 = getFileSize(right_file);
	if(size2 == -1){
	    return -1;
	}
	ll max_size = maxv(size1, size2);
	
	int* lb = (int*)malloc(sizeof(int));
	int* rb = (int*)malloc(sizeof(int));
	int* pb = (int*)malloc(sizeof(int));
	
	while(max_size-- && loss--){
		fread(pb, 1, 1, pf); 
		
		if( 1 != fread(lb, 1, 1, lf)){
			fwrite(pb, 1, 1, rf);
			continue;
		}
		
		*rb = *lb ^ *pb;
		
		fwrite(rb, 1, 1, rf);
	}
	
	fclose(lf);
	fclose(rf);
	fclose(pf);
	
	return 0;
}


void record(double t1, double t2, double t3, double t4){
	FILE* res = fopen("xor_outpur.csv", "a+");
	fprintf(res, "%lld, %f, %f, %f, %f, %f, %f, %d\n", getFileSize("exp2.dat"), t1, t2, t3, t4, t2-t1, t4-t3);
    fclose(res);
}

void xor_base(){
	srand(time(NULL));

    time_t timest;
    double t1, t2, t3, t4;

	printf("XOR Encoding begin!\n");

	t1 = time(&timest);

	encode_file("exp1.dat", "exp2.dat", "xor_encode.dat");

	t2 = time(&timest);

	printf("XOR Encoding ended!\n");

	t3 = time(&timest);

	FEC("exp1.dat", "exp2.dat", "xor_encode.dat", getFileSize("exp2.dat"));

	t4 = time(&timest);

	record(t1, t2, t3, t4);

}


/*
int main(){
	
	char parity_file[] = "encoded.dat";
	char left_file[] = "left.dat";
	char left_ec[] = "left_de.dat";
	char right_file[] = "right.dat";
	char right_ec[] = "right_de.dat";
	
	encode_file(left_file, right_file, parity_file);
	
	FEC(left_ec, right_file, parity_file, getFileSize(left_file));
	BEC(left_file, right_ec, parity_file, getFileSize(right_file));
	
	return 0;
} 
*/
