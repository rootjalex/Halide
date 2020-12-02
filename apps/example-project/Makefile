include ../support/Makefile.inc

.PHONY: build clean test

build: $(BIN)/$(HL_TARGET)/filter

$(GENERATOR_BIN)/example_project.generator: ExampleProject.cpp $(GENERATOR_DEPS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) -o $@ $(LIBHALIDE_LDFLAGS)

$(BIN)/%/example_project.a: $(GENERATOR_BIN)/example_project.generator
	@mkdir -p $(@D)
	$^ -g example_project -e $(GENERATOR_OUTPUTS) -o $(@D) -f example_project target=$* auto_schedule=false

$(BIN)/%/example_project_auto_schedule.a: $(GENERATOR_BIN)/example_project.generator
	@mkdir -p $(@D)
	$^ -g example_project -e $(GENERATOR_OUTPUTS) -o $(@D) -f example_project_auto_schedule target=$*-no_runtime auto_schedule=true -e static_library,c_header,schedule

$(BIN)/%/filter: filter.cpp $(BIN)/%/example_project.a $(BIN)/%/example_project_auto_schedule.a
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -I$(BIN)/$* $^ -o $@ $(IMAGE_IO_FLAGS) $(LDFLAGS)

$(BIN)/%/filter_viz: filter.cpp $(BIN)/%-trace_all/example_project.a
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -DNO_AUTO_SCHEDULE -I$(BIN)/$*-trace_all $^ -o $@ $(IMAGE_IO_FLAGS) $(LDFLAGS)

../../bin/HalideTraceViz: ../../util/HalideTraceViz.cpp
	$(MAKE) -C ../../ bin/HalideTraceViz

$(BIN)/%/example_project.mp4: $(BIN)/%/filter_viz viz.sh ../../bin/HalideTraceViz
	@mkdir -p $(@D)
	HL_AVCONV=$(HL_AVCONV) bash viz.sh $(BIN)/$*

$(BIN)/%/out.png: $(BIN)/%/filter
	@mkdir -p $(@D)
	$< $(IMAGES)/gray.png $@ 0.1 10

$(BIN)/%/out.tiff: $(BIN)/%/filter
	@mkdir -p $(@D)
	$< $(IMAGES)/gray.png $@ 0.1 10

clean:
	rm -rf $(BIN)

test: $(BIN)/$(HL_TARGET)/out.png $(BIN)/$(HL_TARGET)/out.tiff

viz: $(BIN)/$(HL_TARGET)/example_project.mp4
	$(HL_VIDEOPLAYER) $^

$(BIN)/%/viz_auto.mp4: $(BIN)/%/filter_viz ../support/viz_auto.sh ../../bin/HalideTraceViz
	@mkdir -p $(@D)
	HL_AVCONV=$(HL_AVCONV) bash ../support/viz_auto.sh "$(BIN)/$*/filter_viz $(IMAGES)/gray_small.png $(BIN)/$*/out_small.png 0.2 0" ../../bin/HalideTraceViz $@

viz_auto: $(BIN)/$(HL_TARGET)/viz_auto.mp4
	$(HL_VIDEOPLAYER) $^
