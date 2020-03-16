import time
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
import ncap3d
final_shape = [4, 224, 224]


file = ["231125424.mp4"]
final = np.zeros((final_shape[0], final_shape[1], final_shape[1], 3), dtype=np.uint8)

# given ptr for np array, fill the array with decoded frames
f = ncap3d.ncap3d()
s = time.perf_counter()
a = f.decode(file[0], final_shape[0], final_shape[2], final_shape[1], final.ctypes._data)
print(time.perf_counter() - s)


for index, i in enumerate(final):
    plt.imshow(i)
    plt.savefig('./example'+str(index)+'.jpg' )
