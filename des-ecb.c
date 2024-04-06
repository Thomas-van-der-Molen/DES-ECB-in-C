
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>

unsigned long generateKey();
unsigned long permutedChoiceOne(unsigned long);
unsigned long permutedChoiceTwo(unsigned long);
unsigned long shiftRoundKey(unsigned long, int);
unsigned long * GenerateRoundKeys(unsigned long);
unsigned long * getUserPlaintext(int *);
unsigned long * getUserCiphertext(int *);
unsigned long initialPermutation(unsigned long);
unsigned long finalPermutation(unsigned long);
void encrypt();
void decrypt();
unsigned long feistelExpansion(unsigned int);
unsigned int sBoxes(unsigned long);
unsigned int feistelPermutation(unsigned int);
void userMenu();

int main(int argc, char *argv[]){
    
    srand(time(NULL));

    userMenu();

    return 0;
}

unsigned long generateKey(){
    //needs to be a 64 bit (8 byte) pseudo random number
    //https://stackoverflow.com/questions/15621764/generate-a-random-byte-stream
    
    //I will represent the key as an unsigned long long (64 bit data type)
    unsigned long key;

    key = (((long)rand())<<32) + (int)rand();

    printf("\n the random key %016lx \n", key);    
    
    return key;
}

unsigned long permutedChoiceOne(unsigned long key){
    int LeftPC1Indices[] = {57, 49, 41, 33, 25, 17, 9,
                        1, 58, 50, 42, 34, 26, 18, 
                        10, 2, 59, 51, 43, 35, 27, 
                        19, 11, 3, 60, 52, 44, 36};

    int RightPC1Indices[] = {63, 55, 47, 39, 31, 23, 15,
                            7, 62, 54, 46, 38, 30, 22, 
                            14, 6, 61, 53, 45, 37, 29,
                            21, 13, 5, 28, 20, 12, 4};  

    //the left half will be 28 bits - just use a 32 bit long and ignore the last 4 bits
    //the right half will be 28 bits - just use a 32 bit long and ignore the last 4 bits

    unsigned int leftHalf = 0x0000;
    unsigned int rightHalf = 0x0000;
    unsigned char * pointer = (unsigned char*)&key;

    //do pc1 for the left half
    for(int i=0; i<28; i++){
        int bit = LeftPC1Indices[i];
        unsigned char bitValue = (unsigned char)pointer[7 - (int)floor(bit/8)];
        bitValue >>= 8 - (bit%8);
        bitValue &= 0x01;
        //printf("\nthe byte must be %d which is %x ",((int)floor(bit/8)), pointer[7 - (int)floor(bit/8)]);
        //printf("just confirming %x", bitValue);
        leftHalf <<= 1;
        leftHalf += bitValue;
    }
    //do pc1 for the right half
    for(int i=0; i<28; i++){
        int bit = RightPC1Indices[i];
        unsigned char bitValue = (unsigned char)pointer[7 - (int)floor(bit/8)];
        bitValue >>= 8 - (bit%8);
        bitValue &= 0x01;
        rightHalf <<= 1;
        rightHalf += bitValue;
    }
    //printf("\nLeft half %08x\nRight half %08x\n",leftHalf, rightHalf);
    return ((unsigned long)leftHalf << 32) + rightHalf;
}

unsigned long shiftRoundKey(unsigned long key, int shiftAmount){
    //firstly split the key into the left and right half
    unsigned int leftHalf = (unsigned int)(key>>32);
    
    unsigned int rightHalf = (unsigned int)key;
    
    //printf("\nthe left half is %08x and the right half is %08x\n", leftHalf, rightHalf);

    //now perform a circular(barrel) shift by the correct amount
    //IMPORANT NOTE - the left and right halves are only 28 bits, but they are stored as 32 bit integers
    //There are four leading 0s for both the left and right halves, and this must be accounted for
    unsigned int tempLeft;
    unsigned int tempRight;
    tempLeft = (leftHalf << shiftAmount) + (leftHalf >> (28-shiftAmount));
    tempLeft &= 0x0fffffff;
    tempRight = (rightHalf << shiftAmount) + (rightHalf >> (28-shiftAmount));
    tempRight &= 0x0fffffff;
    //printf("\nthe left half is %08x and the right half is %08x\n", tempLeft, tempRight);
    return ((unsigned long)tempLeft << 32) + tempRight;
}

//function will generate all round keys
unsigned long * GenerateRoundKeys(unsigned long key){
    
    //calculate permuted choice 1
    //It is only necessary to do this once, for the initial key
    key = permutedChoiceOne(key);

    //There are 16 round keys
    //to make it easier, roundKeys[0] will store the initial key
    unsigned long * roundKeys = (unsigned long *)malloc(sizeof(unsigned long)*16);
    unsigned long temp[16];
    temp[0] = key;
    //printf("\n The input key is %016lx \n", key);
    
    for(int round=1; round<17; round++){
        
        //determine how much the key halves should be shifted
        //Shift by one for rounds 1, 2, 9, and 16
        //Shift by two for all other rounds
        int roundShiftAmount;
        if(round == 1 || round == 2 || round == 9 || round == 16){
            roundShiftAmount = 1;
        }
        else{
            roundShiftAmount = 2;
        }

        //start by shifting the previous round key by the correct amount
        temp[round] = shiftRoundKey(temp[round - 1], roundShiftAmount);
        //printf("\nRound %d key - %016lx", round, roundKeys[round]);
        
        //next, each round key must be permuted using permutation choice 2, and then, finally, the round keys will be ready.
        //printf("\nRound %d key - %016lx", round, roundKeys[round]);
        //ERROR IN MY IMPLEMENTATION RIGHT HERE - THE OUTPUT FROM PC2 BECOME THE ROUND KEY BUT IS NOT USED IN THE CALCULATION OF THE NEXT KEY
        //EASY FIX DO IT ASAP
        roundKeys[round-1] = permutedChoiceTwo(temp[round]);
    }

    return roundKeys;
}

unsigned long permutedChoiceTwo(unsigned long key){
    

    //An important note here - each round key is represented as a long (8 bytes)
    //However, when you split the key into the left and right half you get two 4 byte numbers
    //But the left and right keys are only 28 bytes. So, each key half has a leading 4 zeros, this is fine, but needs to be accounted for in PC2

    //The input to this function is a key which has already undergone PC1 and has been rotated correctly for the appropriate round
    //The input to this function is TWO 28 bit halves which are joined together in one 64 bit long key
    //The left half has four leading 0s and the right half has four leading 0s
    
    //To make PC2 easier, shift the left sub-key by four and shift the right sub-key left by eight, so that there is only ONE BYTE of 0s at the end of the key
    //before doing PC2

    unsigned long temp = (unsigned long)(key>>32)<<36;
    temp += ((unsigned long)(key)&0x00000000ffffffff)<<8;
    //printf("\n correct math %016lx %016lx %016lx %016lx",key, (unsigned long)(key>>32)<<32, (unsigned long)(key)&0x00000000ffffffff, temp);

    //Also importantly, the output of the PC2 is the final round key
    //Again the output length if 8 bytes (64 bits)
    //The round key however is only 48 bits, so there will be 16 leading zeros

    int pc2Indices[] = {14, 17, 11, 24, 1, 5,
                        3, 28, 15, 6, 21, 10,
                        23, 19, 12, 4, 26, 8,
                        16, 7, 27, 20, 13, 2, 
                        41, 52, 31, 37, 47, 55,
                        30, 40, 51, 45, 33, 48,
                        44, 49, 39, 56, 34, 53,
                        46, 42, 50, 36, 29, 32};    
    
    unsigned long permutedKey = 0x0000000000000000;
    unsigned char * pointer = (unsigned char *)&temp;
    //printf(" \n the key here is %016lx %016lx\n", temp, key);
    for(int i=0; i<48; i++){
        //DES counts bits as starting from 1 at the MSB
        //FOR math purposes, it is easier to subtract one and start counting from 0
        int bit = pc2Indices[i]-1;
        
        int bitValue = (int)floor(bit/8);
        bitValue = *(pointer + 7 - bitValue);
        bitValue >>= (7-(bit%8));
        bitValue &= 0x01;
        permutedKey <<= 1;
        permutedKey += bitValue;
        
        //printf("bit %d must be in byte %d which is %02x\n", bit, bitValue, *(pointer+7-bitValue));

    }
    //printf("\n the key after PC2 %016lx\n", permutedKey);
    //love to see it :)
    return permutedKey;
}

unsigned long * getUserPlaintext(int * PointerToNumOfPlaintextBlocks){

    
    //Due to ascii encoding, a character can be stored as a byte so the input length in bytes is the same as strlen of the input
    char input[400];

    printf("Enter plaintext ");
    //The user will not be allowed to use the whole input buffer, so that padding will always be possible
    fgets(input, sizeof(input)-8, stdin);

    //a common issue when using the fgets function, the last character in the string will be '\n' which needs to be replaced with 0
    input[strlen(input)-1] = 0;

    //printf("\nThe input was %s\n", input);
    //printf("String length is %ld which is %ld bits", strlen(input), strlen(input)*8);
    
    //add padding
    int paddingNeeded = 8 - strlen(input)%8;
    unsigned char paddingValue[] = {paddingNeeded,0};
    //printf("\n The input length is %ld which needs padding of %02x,",strlen(input), paddingValue);
    for(int i=0; i < paddingNeeded; i++){
        strcat(input, paddingValue);
    }

    //printf("\n AFTER PADDING THE LENGTH IS %ld WHICH NEEDS %ld BLOCKS", strlen(input), strlen(input)/8);
    int NumberOfPlaintextBlocks = strlen(input)/8;
    *PointerToNumOfPlaintextBlocks = NumberOfPlaintextBlocks;
    unsigned long * plaintextBlocks = (unsigned long *)malloc(sizeof(unsigned long)*NumberOfPlaintextBlocks);

    /*for(int i=0; i<strlen(input); i++){

        printf("\n character is %c which is %d same as %x",input[i], input[i], input[i]);

    }*/

    //convert the plaintext into blocks of 8 bytes(64 bits)
    //Need to manually reverse the endianness because my cpu uses little endian
    unsigned long * pointer = (unsigned long *)input;
    unsigned long block = 0x0000000000000000;
    for(int i=0; i < strlen(input)/8; i++){
        unsigned long temp = *(pointer + i);
        for(int j=0; j<8; j++){
            block <<= 8;
            block += *(((unsigned char *)&temp)+j);
        }
        
        //add the block to the list of plaintext blocks
        *(plaintextBlocks+i) = block;
    }

    //return the plaintext blocks
    return plaintextBlocks;

}

unsigned long * getUserCiphertext(int * PointerToNumOfPlaintextBlocks){
    
    printf("\n enter the ciphertext ");
    char cipherText[400];

    fgets(cipherText, sizeof(cipherText)-8, stdin);
    //printf("\n absolute lunacy %ld", (strlen(cipherText)-1)/16);
    //printf("\n I am going mad %ld", strlen(cipherText));
    int numCipherBlocks = (strlen(cipherText)-1)/16;
    *PointerToNumOfPlaintextBlocks = numCipherBlocks;
    unsigned long * cipherBlocks = (unsigned long *)malloc(sizeof(unsigned long) * numCipherBlocks);

    //printf("\n block one %016lx ", block);
    for(int i=0; i<numCipherBlocks; i++){
        
        char * temp;
        char tempStr[18];
        strncpy(tempStr, cipherText+(16*i), 16);
        unsigned long temp2 = strtoul(tempStr, &temp, 16);
        *(cipherBlocks+i)=temp2;
    }

    //printf("\nDon't worry, the segmentation fault isn't in the cipher text input function\n");
    return cipherBlocks;
}

//this function will perform the initial permutation for the plaintext input
/*
    The inputs to this function are:
    blocks - a pointer which represents an array. Each elemment is an unsigned long represting a 64 bit block of the plaintext
    numBlocks - is needed to iterate over the blocks pointer properly

    The function returns the same data structure as the "block" input - a pointer representing an array of 8 byte blocks after the initial permutation is done
    My implementation of DES is electronic code book mode, so each block is just treated separately for simplicity
*/
unsigned long initialPermutation(unsigned long block){

    int ipIndices[] = {58, 50, 42, 34, 26, 18, 10, 2,
                    60, 52, 44, 36, 28, 20, 12, 4,
                    62, 54, 46, 38, 30, 22, 14, 6,
                    64, 56, 48, 40, 32, 24, 16, 8,
                    57, 49, 41, 33, 25, 17, 9, 1,
                    59, 51, 43, 35, 27, 19, 11, 3,
                    61, 53, 45, 37, 29, 21, 13, 5,
                    63, 55, 47, 39, 31, 23, 15, 7};

    unsigned long outputBlock;
    //ECB mode, need to do the initial permutation for all blocks, but separately
    
    unsigned long blockInput = block;
    outputBlock = 0x0000000000000000;

    for(int i=0; i<64; i++){

        int bit = ipIndices[i];
        //DES algorithm starts counting bits at 1 but it is easier to start counting at 0, so just subtract 1 from what the bit should be
        bit--;
        unsigned char * pointer = (unsigned char *)&blockInput;
        unsigned char bitValue = pointer[7 - (int)floor(bit/8)];
        //printf(" \n byte %d is %x ", 7-(int)floor(bit/8), bitValue);
        //printf("\n want bit %d which is in byte %d which is %02x", bit, (int)floor(bit/8), bitValue);

        //again, it is best to start counting bits at 0, rather than 1 so shift left by 7, rather than 8
        bitValue >>= 7 - (bit%8);
        bitValue &= 0x01;

        outputBlock <<= 1;
        outputBlock += bitValue;

    }
    //printf("\n in initial permutation func, result of initial permutation is %016lx", outputBlock);
    
    return outputBlock;
}

unsigned long finalPermutation(unsigned long block){

    int ipIndices[] = {40, 8, 48, 16, 56, 24, 64, 32,
                    39, 7, 47, 15, 55, 23, 63, 31,
                    38, 6, 46, 14, 54, 22, 62, 30,
                    37, 5, 45, 13, 53, 21, 61, 29,
                    36, 4, 44, 12, 52, 20, 60, 28,
                    35, 3, 43, 11, 51, 19, 59, 27,
                    34, 2, 42, 10, 50, 18, 58, 26,
                    33, 1, 41, 9, 49, 17, 57, 25};

    
    unsigned long blockInput = block;
    unsigned long outputBlock = 0x0000000000000000;

    for(int i=0; i<64; i++){

        int bit = ipIndices[i];
        //DES algorithm starts counting bits at 1 but it is easier to start counting at 0, so just subtract 1 from what the bit should be
        bit--;
        unsigned char * pointer = (unsigned char *)&blockInput;
        unsigned char bitValue = pointer[7 - (int)floor(bit/8)];
        //printf(" \n byte %d is %x ", 7-(int)floor(bit/8), bitValue);
        //printf("\n want bit %d which is in byte %d which is %02x", bit, (int)floor(bit/8), bitValue);

        //again, it is best to start counting bits at 0, rather than 1 so shift left by 7, rather than 8
        bitValue >>= 7 - (bit%8);
        bitValue &= 0x01;

        outputBlock <<= 1;
        outputBlock += bitValue;
        
    }
    
    return outputBlock;
}


void encrypt(){
    
    //generate the key
    unsigned long long key = generateKey();
    //generate the key for the first round
    //key = shiftRoundKey(key, 1);
    unsigned long * roundKeys = GenerateRoundKeys(key);

    
    int NumberOfPlaintextBlocks;
    unsigned long * plaintextBlocks = getUserPlaintext(&NumberOfPlaintextBlocks);

    /*for(int i=0; i<NumberOfPlaintextBlocks; i++){
        printf("\n %016lx",*(testingText+i));
    }*/

    printf("\n the encrypted text: \n");

    //for each block of the text input
    for(int block=0; block<NumberOfPlaintextBlocks; block++){

        int pid = fork();
        if(pid == 0){

            //start with initial permutation
            unsigned long nextBlock = initialPermutation(*(plaintextBlocks+block));

            unsigned int rightHalf = *(unsigned int*)&nextBlock;
            unsigned int leftHalf = *(((unsigned int*)&nextBlock)+1);
            
            //do 16 rounds of the encryption
            for(int round=0; round<16; round++){

                //if the round is an even number, input the right half to the feistel function, and xor with the left half
                
                if(round % 2 == 0){

                    //--feistel function on the right half
                    unsigned long temp = feistelExpansion(rightHalf);
                    unsigned long temp2 = (*(roundKeys+round))^(temp);
                    unsigned int temp3 = sBoxes(temp2);
                    unsigned int fiestelFunctionResult = feistelPermutation(temp3);
                    //--feistel function on the right half

                    //printf("\n inputs left %08x right %08x roundkey %016lx \n", leftHalf, rightHalf, *(roundKeys+round));
                    //printf("\n outputs E(R) %016lx \nkey xor expansion %016lx \nsboxes %08x",temp, temp2, temp3);
                    leftHalf ^= fiestelFunctionResult;
                    //printf("\npermutation %08x final xor %08x",fiestelFunctionResult, leftHalf);
                }
                else{
                //--feistel function on the right half
                    unsigned long temp = feistelExpansion(leftHalf);
                    unsigned long temp2 = (*(roundKeys+round))^(temp);
                    unsigned int temp3 = sBoxes(temp2);
                    unsigned int fiestelFunctionResult = feistelPermutation(temp3);
                    //--feistel function on the right half

                    //printf("\n inputs left %08x right %08x roundkey %016lx \n", leftHalf, rightHalf, *(roundKeys+round));
                    //printf("\n outputs E(R) %016lx \nkey xor expansion %016lx \nsboxes %08x",temp, temp2, temp3);
                    rightHalf ^= fiestelFunctionResult;
                    //printf("\npermutation %08x final xor %08x",fiestelFunctionResult, leftHalf);
                }
            }

            //after the final round, the halves are swapped
            nextBlock = (((unsigned long)rightHalf)<<32) + leftHalf;
            nextBlock = finalPermutation(nextBlock);
            printf("%016lx", nextBlock);
            //end with final permutation, output the block
            //unsigned long finalEncryption = finalPermutation();
            exit(0);
        }


    }

    //parent process must wait for blocks to be encrypted
    for(int block=0; block<NumberOfPlaintextBlocks; block++){
        wait(NULL);
    }

}

void decrypt(){

    //decryption is the same as encryption, but the round keys are used in reverse

    //first, get the key from the user
    printf("\nEnter the decryption key ");
    unsigned long userKey;
    scanf("%lx", &userKey);

    //I hate scanf
    char garbage[20];
    fgets(garbage, 20, stdin);

    unsigned long * roundKeys = GenerateRoundKeys(userKey);

    //get the ciphertext from the user
    int NumberOfCipherBlocks;
    unsigned long * cipherBlocks = getUserCiphertext(&NumberOfCipherBlocks);
    
    //for each block of the text input
    for(int block=0; block<NumberOfCipherBlocks; block++){

        int pid = fork();
        if(pid==0){

            //start with initial permutation
            unsigned long nextBlock = initialPermutation(*(cipherBlocks+block));

            unsigned int rightHalf = *(unsigned int*)&nextBlock;
            unsigned int leftHalf = *(((unsigned int*)&nextBlock)+1);
            
            //do 16 rounds of the encryption
            for(int round=0; round<16; round++){

                //if the round is an even number, input the right half to the feistel function, and xor with the left half
                
                if(round % 2 == 0){

                    //--feistel function on the right half
                    unsigned long temp = feistelExpansion(rightHalf);
                    unsigned long temp2 = (*(roundKeys + 15 -round))^(temp);
                    unsigned int temp3 = sBoxes(temp2);
                    unsigned int fiestelFunctionResult = feistelPermutation(temp3);
                    //--feistel function on the right half

                    //printf("\n inputs left %08x right %08x roundkey %016lx \n", leftHalf, rightHalf, *(roundKeys+round));
                    //printf("\n outputs E(R) %016lx \nkey xor expansion %016lx \nsboxes %08x",temp, temp2, temp3);
                    leftHalf ^= fiestelFunctionResult;
                    //printf("\npermutation %08x final xor %08x",fiestelFunctionResult, leftHalf);
                }
                else{
                //--feistel function on the right half
                    unsigned long temp = feistelExpansion(leftHalf);
                    unsigned long temp2 = (*(roundKeys+ 15 -round))^(temp);
                    unsigned int temp3 = sBoxes(temp2);
                    unsigned int fiestelFunctionResult = feistelPermutation(temp3);
                    //--feistel function on the right half

                    //printf("\n inputs left %08x right %08x roundkey %016lx \n", leftHalf, rightHalf, *(roundKeys+round));
                    //printf("\n outputs E(R) %016lx \nkey xor expansion %016lx \nsboxes %08x",temp, temp2, temp3);
                    rightHalf ^= fiestelFunctionResult;
                    //printf("\npermutation %08x final xor %08x",fiestelFunctionResult, leftHalf);
                }

            }

            //after the final round, the halves are swapped
            nextBlock = (((unsigned long)rightHalf)<<32) + leftHalf;
            nextBlock = finalPermutation(nextBlock);
            //printf("\nthe decrypted block %016lx ", nextBlock);
            //after the decryption is done, the bytes representing characters need to be turned back into a string.
            char * outputString = (char *)&nextBlock;
            for(int i=0; i<8; i++){
                printf("%c", *(outputString+7-i));
            }
            exit(0);
        }
        
        //end with final permutation, output the block
        //unsigned long finalEncryption = finalPermutation();
    }

    //parent process waits for blocks to be decrypted before continuing program
    for(int block=0; block<NumberOfCipherBlocks; block++){
        wait(NULL);
    }

}

//The input to the feistel expansion function is a 32 bit half block
//the output is a 48 bit sequence
//The 48 bits have to be represented with a 64 bit long, so there will be 4 bytes of zeros at the front (32 leading 0s)
unsigned long feistelExpansion(unsigned int halfBlock){
    
    int expansionBits[] = {32, 1, 2, 3, 4, 5,
                        4, 5, 6, 7, 8, 9,
                        8, 9, 10, 11, 12, 13,
                        12, 13, 14, 15, 16, 17,
                        16, 17, 18, 19, 20, 21,
                        20, 21, 22, 23, 24, 25,
                        24, 25, 26, 27, 28, 29,
                        28, 29, 30, 31, 32, 1};

    unsigned long expansionResult = 0x0000000000000000;
    unsigned char * pointer = (unsigned char *)&halfBlock;
    for(int i=0; i<48; i++){
        int bit = expansionBits[i];
        bit--;
        unsigned char bitValue = pointer[3 - (int)floor(bit/8)];
        bitValue >>= 7 - (bit%8);
        bitValue &= 0x01;
        expansionResult <<= 1;
        expansionResult += bitValue;
    }
    //printf("\n the expansion result is %016lx ", expansionResult);
    return expansionResult;
}

//the input the the sBoxes function is the 48 bit half block after it has been expanded
//The output is 32 bits which will then go through the permutation step
unsigned int sBoxes(unsigned long input){

    unsigned char sb1[][16] = {
        {14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7},
        {0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8},
        {4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0},
        {15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13}
    };

    unsigned char sb2[][16] = {
        {15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10},
        {3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5},
        {0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15},
        {13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9}
    };

    unsigned char sb3[][16] = {
        {10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8},
        {13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1},
        {13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7},
        {1,	10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12}

    };

    unsigned char sb4[][16] = {
        {7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15},
        {13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9},
        {10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4},
        {3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14}
    };

    unsigned char sb5[][16] = {
        {2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9},
        {14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6},
        {4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14},
        {11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3}
    };

    unsigned char sb6[][16] = {
        {12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11},
        {10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8},
        {9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6},
        {4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13}
    };

    unsigned char sb7[][16] = {
        {4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1},
        {13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6},
        {1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2},
        {6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12}
    };

    unsigned char sb8[][16] = {
        {13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7},
        {1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2},
        {7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8},
        {2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}
    };

    unsigned int output = 0x00000000;
    input <<= 16;
    //printf("\n input to sboxes %016lx damn %016lx", input, (input&0x8000000000000000)>>63);

    for(int i=0; i<8; i++){

        //extract the next six bits from the input
        //the interpret bits 0 and 5 as the row
        //interpret bits 1,2,3,4 as the column
        //get the correct value from the correct sbox table
        //add the result to the output
        unsigned char row = 0x00;
        unsigned char column = 0x00;

        row += (input & 0x8000000000000000)>>63;
        row<<=1;
        input <<=1; 

        column += (input & 0x8000000000000000)>>63;
        column <<= 1;
        input<<=1;

        column += (input & 0x8000000000000000)>>63;
        column<<=1;
        input<<=1;
        
        column += (input & 0x8000000000000000)>>63;
        column<<=1;
        input<<=1;

        column += (input & 0x8000000000000000)>>63;
        input<<=1;

        row += (input & 0x8000000000000000)>>63;
        input<<=1;

        if(i == 0) {
            output += sb1[row][column]&0x0f;
            output<<=4;
            //printf("\n row %02x column %d, res %02x", row, column, sb1[row][column]);
        }
        else if(i==1){
            output += sb2[row][column]&0x0f;
            output<<=4;
        }
        else if(i==2){
            output += sb3[row][column]&0x0f;
            output<<=4;
        }
        else if(i==3){
            output += sb4[row][column]&0x0f;
            output<<=4;
        }
        else if(i==4){
            output += sb5[row][column]&0x0f;
            output<<=4;
        }
        else if(i==5){
            output += sb6[row][column]&0x0f;
            output<<=4;
        }
        else if(i==6){
            output += sb7[row][column]&0x0f;
            output<<=4;
        }
        else if(i==7){
            output += sb8[row][column]&0x0f;
        }
        //printf("\n the result of sboxes is %08x ", output);

    }

    return output;

}

unsigned int feistelPermutation(unsigned int input){

    int permutationIndices[] = {16, 7, 20, 21, 29, 12, 28, 17,
                                1, 15, 23, 26, 5, 18, 31, 10,
                                2, 8, 24, 14, 32, 27, 3, 9,
                                19, 13, 30, 6, 22, 11, 4, 25};

    //printf("\n input to feistel permutation is %08x ", input);
    unsigned int output = 0x00000000;
    unsigned char * pointer = (unsigned char *)&input;
    for(int i=0; i<32; i++){

        int bit = permutationIndices[i];
        bit--;
        unsigned char bitValue = pointer[3 - (int)floor(bit/8)];
        bitValue >>= 7 - (bit%8);
        bitValue &= 0x01;
        //printf("\nthe byte must be %d which is %x ",((int)floor(bit/8)), pointer[7 - (int)floor(bit/8)]);
        //printf("just confirming %x", bitValue);
        output <<= 1;
        output += bitValue;
    }

    //printf("\n the output of feistel permutation %08x\n", output);
    return output;
}

void userMenu(){

    int userChoice=0;
    //encrypt();
    while(userChoice != 3){

        printf("\n---DES encryption/decryption menu---\n");
        printf("1. encrypt\n");
        printf("2. decrypt\n");
        printf("3. quit\n");
        char input[10];
        fgets(input, 10, stdin);
        userChoice = atoi(input);
        if(userChoice == 1){
            encrypt();
        }
        else if(userChoice == 2){
            decrypt();
        }
    }
}