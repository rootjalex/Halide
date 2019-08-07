import torch
import torch.nn.functional as func 
# import torch.autograd.profiler as profiler
import sys
import os
import argparse
import time

def profiler(runner, iter=100):
	with torch.autograd.profiler.profile() as prof:
		for _ in range(iter):
			runner()
	# print("Profiler reports average time of:", 
	# 	prof.key_averages()[0].cpu_time_str, 
	# 	"for PyTorch")
	print("PyTorch")
	print(prof.key_averages().table(sort_by="self_cpu_time_total"))

def pool_lambda(stride, extent, w, h, c, n):
	images = torch.randn((w, h, c, n))
	def run_pool():
		y = func.max_pool2d(images, extent, stride)
	return run_pool






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

	profiler(pool_lambda(stride=args.stride, extent=args.extent, w=args.width, h=args.height, 
		c=args.channels, n=args.nImages), args.iterations)

	images = torch.randn((args.width, args.height, args.channels, args.nImages))
	timePassed = 0.0

	for _ in range(args.iterations):
		start = time.time()
		y = func.max_pool2d(images, args.extent, args.stride)
		diff = time.time() - start
		timePassed += diff
		
	print("Average time for PyTorch max_pool:", timePassed / args.iterations)