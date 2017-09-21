from PIL import Image
import scipy.io as sio
import argparse
import numpy as np
import os

# colour map
label_colours = [(0,0,0)
                # 0=background
                ,(128,0,0),(0,128,0),(128,128,0),(0,0,128),(128,0,128)
                # 1=aeroplane, 2=bicycle, 3=bird, 4=boat, 5=bottle
                ,(0,128,128),(128,128,128),(64,0,0),(192,0,0),(64,128,0)
                # 6=bus, 7=car, 8=cat, 9=chair, 10=cow
                ,(192,128,0),(64,0,128),(192,0,128),(64,128,128),(192,128,128)
                # 11=diningtable, 12=dog, 13=horse, 14=motorbike, 15=person
                ,(0,64,0),(128,64,0),(0,192,0),(128,192,0),(0,64,128)]
                # 16=potted plant, 17=sheep, 18=sofa, 19=train, 20=tv/monitor

def mat2jpg(matFile, jpgFile, matKey='data', num_classes=21):
    if os.path.exists(jpgFile) and os.path.getmtime(jpgFile) > os.path.getmtime(matFile):
       return
 
    mat = sio.loadmat(matFile)
    mask = mat[matKey]
    h, w, c, n = mask.shape
    outputs = np.zeros((n, h, w, 3), dtype=np.uint8)

    for i in range(n):
        img = Image.new('RGB', (h, w))
        pixels = img.load()
        for k, v in enumerate(mask):
            for k2, v2 in enumerate(v):
                l = 0
                lmax = -10.0;
                for l_, ll_ in enumerate(v2):
                    if ll_.item() > lmax:
                        l = l_
                        lmax = ll_
                # if l > 0:
                #     print l
                pixels[k,k2] = label_colours[l]
        outputs[i] = np.array(img)
    
    im = Image.fromarray(outputs[0])
    im.save(jpgFile, quality=100)

    print jpgFile

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="mat(from matio library save) format file convert to jpg format file")
    parser.add_argument("matFile", type=str, help="mat format file.")
    parser.add_argument("jpgFile", type=str, help="jpg format file.")
    parser.add_argument("--matKey", type=str, help="mat data key.", default='data')
    FLAGS = parser.parse_args()
    mat2jpg(FLAGS.matFile, FLAGS.jpgFile, matKey=FLAGS.matKey)
