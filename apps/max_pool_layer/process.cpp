#include <cstdio>
#include <chrono>

#include "max_pool_layer.h"
#include "max_pool_layer_auto_schedule.h"

#include <cxxopts.hpp>
#include <iostream>

#include "halide_benchmark.h"
#include "HalideBuffer.h"

using namespace Halide::Tools;
using namespace Halide::Runtime;


int main(int argc, char **argv) {


    cxxopts::Options options("MaxPool", "A benchmark for a Halide implementation of MaxPool2D");

    options.add_options()
    ("s,stride", "stride of the pool", cxxopts::value<int>())
    ("e,extent", "extent of the pool", cxxopts::value<int>())
    ("width", "width of random tensor", cxxopts::value<int>())
    ("height", "height of random tensor", cxxopts::value<int>())
    ("channels", "channels of random tensor", cxxopts::value<int>())
    ("nImages", "4th dimension of random tensor", cxxopts::value<int>())
    ("iterations", "number of iterations to run pool", cxxopts::value<int>())
    ;



    auto result = options.parse(argc, argv);

    int stride, extent, channels;
    int width, height, nImages, iterations;

    // Hyperparameters
    try {
        stride = result["stride"].as<int>();
    }
    catch (std::domain_error e) {
        printf("Default stride.\n");
        stride  = 2;
    }
    try {
        extent = result["extent"].as<int>();
    }
    catch (std::domain_error e) {
        printf("Default extent.\n");
        extent = 2;
    }

    // Batch parameters
    try {
        width = result["width"].as<int>();
    }
    catch (std::domain_error e) {
        printf("Default width.\n");
        width = 1024;
    }
    try {
        height = result["height"].as<int>();
    }
    catch (std::domain_error e) {
        printf("Default height.\n");
        height = 1024;
    }
    try {
        channels = result["channels"].as<int>();
    }
    catch (std::domain_error e) {
        printf("Default channels.\n");
        channels = 3;
    }
    try {
        nImages = result["nImages"].as<int>();
    }
    catch (std::domain_error e) {
        printf("Default nImages.\n");
        nImages = 10;
    }
    try {
        iterations = result["iterations"].as<int>();
    }
    catch (std::domain_error e) {
        printf("Default iterations.\n");
        iterations = 10;
    }



    Buffer<float> input(width, height, channels, nImages);

    for (int c = 0; c < input.dim(3).extent(); c++) {
        for (int z = 0; z < input.channels(); z++) {
            for (int y = 0; y < input.height(); y++) {
                for (int x = 0; x < input.width(); x++) {
                    input(x, y, z, c) = rand();
                }
            }
        }
    }

    int oWidth = (width - extent) / stride + 1;
    int oHeight = (height - extent) / stride + 1;

    Buffer<float> output(oWidth, oHeight, channels, nImages);

    max_pool_layer(extent, stride, input, output);

    // Check correctness
    for (int c = 0; c < output.dim(3).extent(); c++) {
        for (int z = 0; z < output.channels(); z++) {
            for (int y = 0; y < output.height(); y++) {
                for (int x = 0; x < output.width(); x++) {
                    float output_val = output(x, y, z, c);
                    float correct_val = 0.0f;
                    for (int i = 0; i < extent; i++) {
                        if (x * stride + i < input.width()) {
                            // simple boundaries check
                            for (int j = 0; j < extent; j++) {
                                if (y * stride + j < input.height()) {
                                    // another simple boundaries check
                                    correct_val = fmax(correct_val, input(x * stride + i, y * stride + j, z, c));
                                }
                            }
                        }
                    }

                    if (correct_val != output_val) {
                        printf("output(%d, %d, %d, %d) was %f instead of %f\n",
                       x, y, z, c, output_val, correct_val);
                        return -1;
                    }
                }
            }
        }
    }
    /*

            output(x, y, z, n) = Halide::maximum(in_b(x * stride + r.x,
                                                  y * stride + r.y,
                                                  z, 
                                                  n));
                                                  */

    // Timing code

    // Manually-tuned version
    double min_t_manual = benchmark(iterations, iterations, [&]() {
        max_pool_layer(extent, stride, input, output);
    });
    printf("Manually-tuned time: %gms\n", min_t_manual * 1e3);

    // Auto-scheduled version
    double min_t_auto = benchmark(iterations, iterations, [&]() {
        max_pool_layer_auto_schedule(extent, stride, input, output);
    });
    printf("Auto-scheduled time: %gms\n", min_t_auto * 1e3);

    return 0;
}
