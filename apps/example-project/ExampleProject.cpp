#include "Halide.h"
#include "halide_trace_config.h"

namespace {

class ExampleProject : public Halide::Generator<ExampleProject> {
public:
    Input<Buffer<double>> f{"f", 2};
    Input<Buffer<uint8_t>> mask{"mask", 2};
    Input<int32_t> width{"width"};
    Input<int32_t> height{"height"};

    Output<Buffer<double>> out{"out", 0};

    /*

Stmt    dx(j0,i0) = select(i0 + 1 < width,select(mask(Max(0,Min(height - 1,j0)),Max(0,Min(width - 1,i0 + 1))) != (0:u8),input(Max(0,Min(height - 1,j0)),Max(0,Min(width - 1,i0 + 1))) - input(Max(0,Min(height - 1,j0)),Max(0,Min(width - 1,i0))),0.0),0.0)
        dy(j1,i1) = select(j1 + 1 < height,select(mask(Max(0,Min(height - 1,j1 + 1)),Max(0,Min(width - 1,i1))) != (0:u8),input(Max(0,Min(height - 1,j1 + 1)),Max(0,Min(width - 1,i1))) - input(Max(0,Min(height - 1,j1)),Max(0,Min(width - 1,i1))),0.0),0.0)
        out(k) = sum(r_j,sum(r_i,select(mask(Max(0,Min(height - 1,r_j)),Max(0,Min(width - 1,r_i))) != (0:u8),dx(r_j,r_i) * dx(r_j,r_i) + dy(r_j,r_i) * dy(r_j,r_i),0.0)))
*/
    void generate() {
        Var j0("j0"), i0("i0"), j1("j1"), i1("i1"), k("k");
        RDom r_j(0, height);
        RDom r_i(0, width);
        Func dx("dx"), dy("dy");

        Expr zero_d = cast<double>(0);
        Expr zero_u8 = cast<uint8_t>(0);

        Expr dx_mask_j = max(0, min(height - 1, j0));
        Expr dx_mask_i = max(0, min(width - 1, i0 + 1));
        Expr dx_diff = f(dx_mask_j, dx_mask_i) - f(dx_mask_j, dx_mask_i - 1);

        dx(j0, i0) = select(i0 + 1 < width,
                            select(mask(dx_mask_j, dx_mask_i) != zero_u8, dx_diff, zero_d),
                            zero_d);

        Expr dy_mask_j = max(0, min(height - 1, j1 + 1));
        Expr dy_mask_i = max(0, min(width - 1, i1));
        Expr dy_diff = f(dy_mask_j, dy_mask_i) - f(dy_mask_j - 1, dy_mask_i);

        dy(j1, i1) = select(j1 + 1 < height,
                            select(mask(dy_mask_j, dy_mask_i) != zero_u8, dy_diff, zero_d),
                            zero_d);

        Expr out_mask_j = max(0, min(height - 1, r_j.x));
        Expr out_mask_i = max(0, min(width - 1, r_i.x));

        out() = sum(r_j,
                    sum(r_i,
                        select(
                            mask(out_mask_j, out_mask_i) != zero_u8,
                            dx(r_j, r_i) * dx(r_j, r_i) + dy(r_j, r_i) * dy(r_j, r_i),
                            zero_d)));

        width.set_estimate(1000);
        height.set_estimate(1000);
        mask.dim(0).set_estimate(0, 1000);
        mask.dim(1).set_estimate(0, 1000);
        f.dim(0).set_estimate(0, 1000);
        f.dim(1).set_estimate(0, 1000);
    }
};

}  // namespace

HALIDE_REGISTER_GENERATOR(ExampleProject, example_project)
