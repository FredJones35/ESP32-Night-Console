# Ekran görüntülerini (PPM) 3x büyütülmüş tek bir PNG'de birleştirir.
import sys
from PIL import Image
names = sys.argv[2:]
imgs = [Image.open(f"tools/sim/out/{n}.ppm").resize((384, 384), Image.NEAREST) for n in names]
cols = min(4, len(imgs))
rows = (len(imgs) + cols - 1) // cols
m = Image.new("RGB", (cols * 392, rows * 392), (60, 60, 60))
for i, im in enumerate(imgs):
    m.paste(im, ((i % cols) * 392 + 4, (i // cols) * 392 + 4))
m.save(sys.argv[1])
