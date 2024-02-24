gvtf: spv32.h spv64.h tf.c tf.h gvtf.go go.mod
	go build

#comp.spv: tf.comp
#	glslangValidator --target-env spirv1.6 -V tf.comp

spv32.h: tf32.comp
	glslangValidator --target-env spirv1.6 --vn spv32 -V tf32.comp -o spv32.h

spv64.h: tf64.comp
	glslangValidator --target-env spirv1.6 --vn spv64 -V tf64.comp -o spv64.h


go.mod:
	go mod init gvtf
	go mod tidy
