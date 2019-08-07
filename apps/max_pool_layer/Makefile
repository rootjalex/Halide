include ../support/Makefile.inc

CLI_INCLUDE_LOC ?= -I/Users/alexanderroot/Documents/UROP/Graphics/cli_tools/include

all: $(BIN)/$(HL_TARGET)/process

$(GENERATOR_BIN)/max_pool_layer.generator: max_pool_layer_generator.cpp $(GENERATOR_DEPS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(filter-out %.h,$^) -o $@ $(LDFLAGS)

$(BIN)/%/max_pool_layer.a: $(GENERATOR_BIN)/max_pool_layer.generator
	@mkdir -p $(@D)
	$^ -g max_pool_layer -e $(GENERATOR_OUTPUTS) -o $(@D) -f max_pool_layer target=$* auto_schedule=false

$(BIN)/%/max_pool_layer_auto_schedule.a: $(GENERATOR_BIN)/max_pool_layer.generator
	@mkdir -p $(@D)
	$^ -g max_pool_layer -e $(GENERATOR_OUTPUTS) -o $(@D) -f max_pool_layer_auto_schedule target=$*-no_runtime auto_schedule=true

$(BIN)/%/process: process.cpp $(BIN)/%/max_pool_layer.a $(BIN)/%/max_pool_layer_auto_schedule.a
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -I$(BIN)/$* -Wall $^ -o $@ $(CLI_INCLUDE_LOC) $(LDFLAGS)

run: $(BIN)/$(HL_TARGET)/process
	@mkdir -p $(@D)
	$^

clean:
	rm -rf $(BIN)

test: run
