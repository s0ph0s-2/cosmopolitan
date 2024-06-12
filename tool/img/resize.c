#include "libc/stdio/stdio.h"
#include "libc/mem/mem.h"
#include "third_party/stb/stb_image.h"
#include "third_party/stb/stb_image_write.h"
#include "third_party/stb/stb_image_resize.h"

void calculate_max_dims_in_square(int square_size, int input_w, int input_h, int *output_w, int *output_h) {
    if (input_w <= square_size && input_h <= square_size) {
        *output_w = input_w;
        *output_h = input_h;
        return;
    }
    double ratio_w = (double)square_size / (double)input_w;
    double ratio_h = (double)square_size / (double)input_h;
    double ratio = ratio_w < ratio_h ? ratio_w : ratio_h;

    *output_w = (int)(input_w * ratio);
    *output_h = (int)(input_h * ratio);
}

int main(void) {
    int rc, x, y, channels;
    int thumb_x, thumb_y;
    unsigned char *data, *thumbnail_data;

    printf("hello world\n");

    data = stbi_load("test-in.jpg", &x, &y, &channels, 0);
    if (data != NULL) {
        printf("successfully loaded image (%dx%d, %d channels)\n", x, y, channels);
        calculate_max_dims_in_square(192, x, y, &thumb_x, &thumb_y);
        printf("resizing to %dx%d\n", thumb_x, thumb_y);
        thumbnail_data = calloc(thumb_x * thumb_y * channels, sizeof(unsigned char));
        if (!thumbnail_data) {
            printf("malloc failed\n");
        } else {
            rc = stbir_resize_uint8(data, x, y, 0, thumbnail_data, thumb_x, thumb_y, 0, channels);
            if (rc) {
                rc = stbi_write_jpg("test-out.jpg", thumb_x, thumb_y, channels, thumbnail_data, 100);
                rc = stbi_write_webp("test-out.webp", thumb_x, thumb_y, channels, thumbnail_data, 75.0);
                if (rc) {
                    printf("successfully wrote output file (rc %d)\n", rc);
                } else {
                    printf("failed to write output file: %s\n", stbi_failure_reason());
                }
            } else {
                printf("failed to resize image\n");
            }
        }
        free(thumbnail_data);
    } else {
        printf("failed to load image: %s\n", stbi_failure_reason());
    }
    stbi_image_free(data);
    return 0;
}