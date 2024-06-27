#include <stdio.h>

#include "tool/img/lib/image.h"
#include "third_party/stb/stb_image.h"
#include "tool/img/lib/gradienthash.h"

const uint64_t example = -8330928499254513108;

/*
int main(int argc, char **argv) {
    double input[2][3] = { { 5, 6, 7 }, { 8, 9, 10 } };
    double output[3][2];
    TransposeLinearizedArray((double *)input, 3, 2, (double *)output);
    for (size_t i = 0; i < 3; i += 1) {
        for (size_t j = 0; j < 2; j += 1) {
            printf("%d,%d = %f\n", i, j, output[i][j]);
        }
    }
    return 0;
}
*/

int main(int argc, char const **argv) {
    int rc;
    ILImageu8_t image;
    uint64_t hash;
    if (argc != 2) {
        printf("usage: gradhash filename.jpg\n");
        return 1;
    }
    rc = ILImageu8LoadFromFile(&image, argv[1]);
    if (!rc) {
        printf("unable to load image: %s\n", stbi_failure_reason());
        return 1;
    }
    rc = GradientHash(image, &hash);
    if (!rc) {
        printf("unable to hash image\n");
        return 1;
    }
    printf("Gradient hash of image: %#018llx (%064llb)\n", hash, hash);
    // printf("Example gradient hash:  %#llx (%064llb)\n", example, example);
    return 0;
}