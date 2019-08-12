import tensorflow as tf

import sys
import os
import argparse
import time




if __name__ == "__main__":
	parser = argparse.ArgumentParser(description="Profile performance of PyTorch max_pool2d")
	parser.add_argument("--stride", "-s", dest="stride", type=int, default=2, help="stride of the pool")
	parser.add_argument("--extent", "-e", dest="extent", type=int, default=2, help="extent of the pool")
	parser.add_argument("--width", dest="width", type=int, default=1024, help="width of random tensor")
	parser.add_argument("--height", dest="height", type=int, default=1024, help="height of random tensor")
	parser.add_argument("--channels", dest="channels", type=int, default=3, help="channels of random tensor")
	parser.add_argument("--nImages", dest="nImages", type=int, default=10, help="4th dimension of random tensor")
	parser.add_argument("--iterations", "-i", dest="iterations", type=int, default=10, help="number of iterations to run pool")

	args = parser.parse_args()



	images = tf.random.uniform((args.width, args.height, args.channels, args.nImages), dtype=tf.dtypes.float64)
	timePassed = 0.0

	with tf.Session() as sess:
		sess.run(images)
		for _ in range(args.iterations):
			# print("X:", images)
			start = time.time()
			y = tf.nn.max_pool2d(images, [args.extent, args.extent], 
				args.stride, "SAME")
			sess.run(y)
			diff = time.time() - start
			# print("Y:", y)
			timePassed += diff
		
	print("Average time for Tensorflow max_pool:", timePassed / args.iterations * 100, " ms")