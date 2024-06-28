#include <stdio.h>
#include <stdlib.h>
#include "tool/img/lib/image.h"
#include "third_party/stb/stb_image.h"
#include "third_party/stb/stb_image_write.h"
#include "third_party/stb/stb_image_resize.h"

int main(int argc, char const **argv) {
    int rc;
    ILImageu8_t image, thumbnail;
    if (argc != 3) {
        printf("usage: resize input.(jpg|png) output.webp\n");
        return 1;
    }
    rc = ILImageu8LoadFromFile(&image, argv[1]);
    if (!rc) {
        printf("failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }
    rc = ILImageu8ResizeToFitSquare(image, 192, &thumbnail);
    ILImageu8Free(&image);
    if (!rc) {
        printf("unable to resize image: out of memory\n");
        return 1;
    }
    rc = ILImageu8SaveWebPFile(thumbnail, argv[2], 75.0f);
    ILImageu8Free(&thumbnail);
    if (!rc) {
        printf("failed to save resized image: %s\n", stbi_failure_reason());
        return 1;
    }
    return 0;
}