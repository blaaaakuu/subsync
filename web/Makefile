####################################
### default global configuration ###
####################################

TARGETS_DIR = $(abspath scripts)
DEPS_DIR    = $(abspath deps)
OBJ_DIR     = $(abspath obj)
GIZMO_DIR   = $(abspath ../gizmo)
NATIVE_DIR  = $(abspath native)
POCKET_BUILD = $(OBJ_DIR)/pocketsphinx-build

EXTRACTOR_TARGET  = $(TARGETS_DIR)/extractor.js
CORRELATOR_TARGET = $(TARGETS_DIR)/correlator.js

TARGETS = \
		  $(EXTRACTOR_TARGET) \
		  $(CORRELATOR_TARGET) \

EXTRACTOR_DEPS   = \
		 $(DEPS_DIR)/lib/libavcodec.a \
		 $(DEPS_DIR)/lib/libpocketsphinx.a \

CXXFLAGS += \
			-O3 \
			-Wall \
			-Wextra \
			-pedantic \
			-std=c++17 \
			-fvisibility=hidden \
			-I$(GIZMO_DIR) \
			-sMODULARIZE=1 \
			-sEXPORT_NAME=gizmo \
			-sSTRICT=1 \
			-sALLOW_MEMORY_GROWTH=1 \
			-g \
			-sDISABLE_EXCEPTION_CATCHING=0 \

LDFLAGS += \
		   -O3 \
		   --bind \
		   -sMODULARIZE=1 \
		   -sEXPORT_NAME=gizmo \
		   -sSTRICT=1 \
		   -sALLOW_MEMORY_GROWTH=1 \
		   -g \
		   -sDISABLE_EXCEPTION_CATCHING=0 \


######################
### artifact files ###
######################

SHARED_SOURCES = \
		  $(GIZMO_DIR)/match/wordmatcher.cpp \
		  $(GIZMO_DIR)/text/translator.cpp \
		  $(GIZMO_DIR)/text/dictionary.cpp \
		  $(GIZMO_DIR)/text/words.cpp \
		  $(GIZMO_DIR)/text/ssa.cpp \
		  $(GIZMO_DIR)/text/ngrams.cpp \
		  $(GIZMO_DIR)/text/utf8.cpp \
		  $(GIZMO_DIR)/math/point.cpp \
		  $(GIZMO_DIR)/math/line.cpp \
		  $(GIZMO_DIR)/math/linefinder.cpp \

EXTRACTOR_SOURCES = \
		  $(GIZMO_DIR)/wasm/extractor.cpp \
		  $(GIZMO_DIR)/media/demux.cpp \
		  $(GIZMO_DIR)/media/stream.cpp \
		  $(GIZMO_DIR)/media/audiodec.cpp \
		  $(GIZMO_DIR)/media/resampler.cpp \
		  $(GIZMO_DIR)/media/subdec.cpp \
		  $(GIZMO_DIR)/media/speechrec.cpp \
		  $(GIZMO_DIR)/general/exception.cpp \
		  $(GIZMO_DIR)/general/logger.cpp \

CORRELATOR_SOURCES = \
		  $(GIZMO_DIR)/wasm/correlator.cpp \
		  $(GIZMO_DIR)/synchro/synchronizer.cpp \

SHARED_OBJECTS     = $(addprefix $(OBJ_DIR)/,$(SHARED_SOURCES:.cpp=.o))
EXTRACTOR_OBJECTS  = $(addprefix $(OBJ_DIR)/,$(EXTRACTOR_SOURCES:.cpp=.o))
CORRELATOR_OBJECTS = $(addprefix $(OBJ_DIR)/,$(CORRELATOR_SOURCES:.cpp=.o))
OBJECTS = $(SHARED_OBJECTS) $(EXTRACTOR_OBJECTS) $(CORRELATOR_OBJECTS)


#####################
### build recipes ###
#####################

all: $(TARGETS)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(TARGETS)
	$(RM) $(TARGETS:.js=.wasm)
	$(RM) $(TARGETS:.js=.wast)

clean-all: clean clean-deps
	$(RM) -r $(TARGETS_DIR) $(OBJ_DIR)

$(EXTRACTOR_OBJECTS): CXXFLAGS += \
	-I$(DEPS_DIR)/include \
	-DTRACK_EXCEPTIONS \

$(EXTRACTOR_TARGET): LDFLAGS += \
	-lworkerfs.js \
	-lidbfs.js \
	-L$(DEPS_DIR)/lib \
	-lm \
	-lavdevice \
	-lavfilter \
	-lavformat \
	-lavcodec \
	-lswresample \
	-lswscale \
	-lavutil \
	-lpocketsphinx \
	-sFORCE_FILESYSTEM=1 \
	-sEXPORTED_RUNTIME_METHODS=FS \
	-sFETCH=1 \

$(EXTRACTOR_OBJECTS): $(EXTRACTOR_DEPS)
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

$(EXTRACTOR_TARGET): $(EXTRACTOR_OBJECTS) $(SHARED_OBJECTS) $(EXTRACTOR_DEPS)
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $(EXTRACTOR_OBJECTS) $(SHARED_OBJECTS) $(LDFLAGS)

$(CORRELATOR_TARGET): $(CORRELATOR_OBJECTS) $(SHARED_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $(CORRELATOR_OBJECTS) $(SHARED_OBJECTS) $(LDFLAGS)

%.wasm: %.js


############################
### dependencies recipes ###
############################

.ONESHELL:
$(DEPS_DIR)/lib/libpocketsphinx.a:
	emcmake cmake \
		-S $(NATIVE_DIR)/pocketsphinx \
		-B $(POCKET_BUILD) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(DEPS_DIR) \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_TESTING=OFF
	cmake --build $(POCKET_BUILD) --parallel 4
	cmake --install $(POCKET_BUILD)

.ONESHELL:
$(DEPS_DIR)/lib/libavcodec.a:
	cd $(NATIVE_DIR)/ffmpeg
	emconfigure ./configure \
		--prefix=$(DEPS_DIR) \
		--disable-debug \
		--disable-programs \
		--disable-encoders \
		--disable-muxers \
		--disable-devices \
		--disable-x86asm \
		--disable-inline-asm \
		--disable-doc \
		--disable-stripping \
		--nm='llvm-nm -g' \
		--ar=emar \
		--cc=emcc \
		--cxx=em++ \
		--objcc=emcc \
		--dep-cc=emcc
	emmake make -j4 install

clean-deps:
	$(RM) -r $(DEPS_DIR) $(POCKET_BUILD)

.PHONY: all clean clean-all clean-deps
