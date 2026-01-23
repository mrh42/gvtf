gvtf: spv32.h spv64.h tf.c tf.h gvtf.go go.mod
	go build

GLSFLAGS = --target-env spirv1.6
# Darwin/metal doesn't have double
#GLSFLAGS += -DNO_DOUBLE

spv32.h: tf32.comp common.glsl
	glslangValidator $(GLSFLAGS) --vn spv32 -V tf32.comp -o spv32.h

spv64.h: tf64.comp common.glsl
	glslangValidator $(GLSFLAGS) --vn spv64 -V tf64.comp -o spv64.h

go.mod:
	go mod init gvtf
	go mod tidy

clean:
	rm spv64.h spv32.h gvtf
