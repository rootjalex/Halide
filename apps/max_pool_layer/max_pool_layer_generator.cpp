#include "Halide.h"

namespace {

using namespace Halide;

class MaxPoolLayer : public Halide::Generator<MaxPoolLayer> {
private:
    Halide::Var x{"x"}, y{"y"}, z{"z"}, n{"n"};

public:
    Input<uint8_t>  extent{"extent"};
    Input<uint8_t> stride{"stride"};
    Input<Buffer<float>> input{"input", 4};

    Output<Buffer<float>> output{"output", 4};

    void generate() {
        /* THE ALGORITHM */

        // padding
        Halide::Func in_b = BoundaryConditions::repeat_edge(input);

        Halide::RDom r(0, extent, 0, extent);

        output(x, y, z, n) = Halide::maximum(in_b(x * stride + r.x,
                                                  y * stride + r.y,
                                                  z, 
                                                  n));
    }

    void schedule() {
        /* THE SCHEDULE */

        if (auto_schedule) {

            // set some arbitray bounds estimates for the input
            input.dim(0).set_estimate(0, 5048);
            input.dim(1).set_estimate(0, 5048);
            input.dim(2).set_estimate(0, 3);
            input.dim(3).set_estimate(0, 10);

            // some common max pooling hyperparameters
            stride.set_estimate(2);
            extent.set_estimate(2);

            output.dim(0).set_estimate(0, 512); // w2 = (w1 - extent) / stride + 1
            output.dim(1).set_estimate(0, 512); // h2 = (h1 - extent) / stride + 1
            output.dim(2).set_estimate(0, 3);
            output.dim(3).set_estimate(0, 100);
        }
        else {
            output.parallel(n).vectorize(x, 16); // simple but effective
        }
   }
};

}  // namespace

HALIDE_REGISTER_GENERATOR(MaxPoolLayer, max_pool_layer)

