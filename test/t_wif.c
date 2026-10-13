#include <stdio.h>
#include <string.h>
#include "../src/crypto/base58.h"
int main(void){
    char wif[64];
    uint8_t k[32];
    int i;
    /* known vector: privkey 0x01 -> WIF (compressed) */
    for(i=0;i<32;i++) k[i]=0; k[31]=1;
    wif_encode(wif,k); printf("k=1        %s\n", wif);
    /* privkey = 0x0000...0002 */
    k[31]=2; wif_encode(wif,k); printf("k=2        %s\n", wif);
    for(i=0;i<32;i++) k[i]=0xff; k[31]=0x00;
    wif_encode(wif,k); printf("k=ff..00   %s\n", wif);
    /* leading-zero key exercises the '1' padding path */
    for(i=0;i<32;i++) k[i]=0; k[30]=0x12; k[31]=0x34;
    wif_encode(wif,k); printf("k=0x1234   %s\n", wif);
    return 0;
}
