from PIL import Image
import sys

for f in sys.argv[1:]:
	if f[-4:].lower() != '.gif':
		continue

	im = Image.open(f)

	while True:
		try:
		    seq = im.tell()
		    im.seek(seq + 1)
		    im.save('%s-%03d.png' % (f[:-4], seq+1), quality=100)
		except:
		    break

