#include <stdio.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

int main(int argc, char *argv[]) {
    unsigned n;
    unsigned char *x;
    unsigned m;
    unsigned run_val;
    unsigned run_count;
    int width, height;
    GdkPixbuf *pixbuf;
    GError *gerr;
    
    if(argc != 2) {
    	fprintf(stderr, "Usage: %s <imagefile>\n", argv[0]);
    	return 1;
    }
    
    pixbuf = gdk_pixbuf_new_from_file(argv[1], &gerr);
    if(!pixbuf) {
    	fprintf(stderr, "gdk_pixbuf_new_from_file(%d): %s\n", gerr->code, gerr->message);
    	g_error_free(gerr);
    	return 1;
    }
 
 	width = gdk_pixbuf_get_width(pixbuf);
 	height = gdk_pixbuf_get_height(pixbuf);
 
    n = width * height;
    m = 0;
    x = gdk_pixbuf_get_pixels(pixbuf);

    printf("struct {\n");
    printf("  unsigned width;\n");
    printf("  unsigned height;\n");
    printf("  unsigned cwidth;\n");
    printf("  unsigned cheight;\n");
    printf("  unsigned char rundata[];\n");
    printf("} font = {\n");
    printf("  .width = %d,\n  .height = %d,\n  .cwidth = %d,\n  .cheight = %d,\n", width, height, width / 96, height);
    printf("  .rundata = {\n    ");
   
    run_val = (*x ? 0 : 255);
    run_count = 1;
    n--;
    x+=3;

    while(n-- > 0) {
        unsigned val = (*x ? 0 : 255);
        x+=3;
        if((val == run_val) && (run_count < 127)) {
            run_count++;
        } else {
            printf("0x%02x,",run_count | (run_val ? 0x80 : 0x00));
            run_val = val;
            run_count = 1;
            m += 5;
            if(m >= 75) {
                printf("\n    ");
                m = 0;
            }
        }
    }
    printf("0x%02x,",run_count | (run_val ? 0x80 : 0x00));
    printf("\n    0x00\n");
    printf("  }\n};\n");
    g_object_unref(pixbuf);
    return 0;
}
